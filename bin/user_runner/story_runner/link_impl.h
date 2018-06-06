// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERIDOT_BIN_USER_RUNNER_STORY_RUNNER_LINK_IMPL_H_
#define PERIDOT_BIN_USER_RUNNER_STORY_RUNNER_LINK_IMPL_H_

#include <set>
#include <vector>

#include <fuchsia/modular/cpp/fidl.h>
#include <fuchsia/modular/internal/cpp/fidl.h>
#include "lib/async/cpp/operation.h"
#include "lib/fidl/cpp/binding.h"
#include "lib/fidl/cpp/clone.h"
#include "lib/fidl/cpp/interface_handle.h"
#include "lib/fidl/cpp/interface_ptr.h"
#include "lib/fidl/cpp/interface_ptr_set.h"
#include "lib/fidl/cpp/interface_request.h"
#include "lib/fxl/macros.h"
#include "peridot/bin/user_runner/story_runner/key_generator.h"
#include "peridot/lib/ledger_client/ledger_client.h"
#include "peridot/lib/ledger_client/page_client.h"
#include "peridot/lib/ledger_client/types.h"
#include "peridot/lib/rapidjson/rapidjson.h"

namespace modular {

// Use the CrtAllocator and not the pool allocator so that merging doesn't
// require deep copying.
using CrtJsonDoc =
    rapidjson::GenericDocument<rapidjson::UTF8<>, rapidjson::CrtAllocator>;
using CrtJsonValue = CrtJsonDoc::ValueType;
using CrtJsonPointer = rapidjson::GenericPointer<CrtJsonValue>;

class LinkConnection;
class LinkWatcherConnection;

// A fuchsia::modular::Link is a mutable and observable value that is persistent
// across story restarts, synchronized across devices, and can be shared between
// modules.
//
// When a module requests to run more modules using
// fuchsia::modular::ModuleContext::StartModule(), one or more
// fuchsia::modular::Link instances are associated with each such request (as
// specified in the fuchsia::modular::Intent). fuchsia::modular::Link instances
// can be shared between multiple modules. The same fuchsia::modular::Link
// instance can be used in multiple StartModule() requests, so it can be shared
// between more than two modules. fuchsia::modular::Link instances have names
// that are local to each Module, and can be accessed by calling
// fuchsia::modular::ModuleContext.GetLink(name).
//
// If a watcher is registered through one handle using the Watch() method, it
// only receives notifications for changes by requests through other handles. To
// make this possible, each fuchsia::modular::Link connection is bound to a
// separate LinkConnection instance rather than to LinkImpl directly. LinkImpl
// owns all its LinkConnection instances.
//
// This implementation of LinkImpl works by storing the history of change
// operations made by the callers. Each change operation is stored as a separate
// key/value pair, which can be reconciled by the Ledger without conflicts. The
// ordering is determined by KeyGenerator, which orders changes based on time as
// well as a random nonce that's a tie breaker in the case of changes made at
// the same time on different devices.
//
// New changes are placed on the pending_ops_ queue within the class and also
// written to the Ledger. Because the state of the Snapshot can float, the
// change operations are kept in the pending_ops_ queue until a notification is
// received from the ledger that the op has been applied to the ledger, at which
// point the change operation is removed from pending_ops_.
//
// To arrive at the latest value, the history from the ledger is merged with the
// history in pending_ops_. Duplicates are removed. Then the changes are applied
// in order. This algorithm is not "correct" due to the lack of a vector clock
// to form the partial orderings. It will be replaced eventually by a CRDT based
// one.
class LinkImpl : PageClient {
 public:
  // The |link_path| contains the series of module names (where the last element
  // is the module that created this fuchsia::modular::Link) that this
  // fuchsia::modular::Link is namespaced under. If |create_link_info| is null,
  // then this is a request to connect to an existing link.
  LinkImpl(LedgerClient* ledger_client, LedgerPageId page_id,
           const fuchsia::modular::LinkPath& link_path,
           fuchsia::modular::CreateLinkInfoPtr create_link_info);

  ~LinkImpl() override;

  // Creates a new LinkConnection for the given request. LinkConnection
  // instances are deleted when their connections close, and they are all
  // deleted and close their connections when LinkImpl is destroyed.
  void Connect(fidl::InterfaceRequest<fuchsia::modular::Link> request);

  // Used by LinkConnection.
  void UpdateObject(fidl::VectorPtr<fidl::StringPtr> path, fidl::StringPtr json,
                    uint32_t src);
  void Set(fidl::VectorPtr<fidl::StringPtr> path, fidl::StringPtr json,
           uint32_t src);
  void Get(fidl::VectorPtr<fidl::StringPtr> path,
           const std::function<void(fidl::StringPtr)>& callback);
  void GetEntity(const fuchsia::modular::Link::GetEntityCallback& callback);
  void SetEntity(fidl::StringPtr entity_reference, const uint32_t src);
  void Erase(fidl::VectorPtr<fidl::StringPtr> path, uint32_t src);
  void AddConnection(LinkConnection* connection);
  void RemoveConnection(LinkConnection* connection);
  void Sync(const std::function<void()>& callback);
  void Watch(fidl::InterfaceHandle<fuchsia::modular::LinkWatcher> watcher,
             uint32_t conn);
  void WatchAll(fidl::InterfaceHandle<fuchsia::modular::LinkWatcher> watcher);

  // Used by LinkWatcherConnection.
  void RemoveConnection(LinkWatcherConnection* connection);

  // Used by StoryControllerImpl.
  const fuchsia::modular::LinkPath& link_path() const { return link_path_; }
  void set_orphaned_handler(const std::function<void()>& fn) {
    orphaned_handler_ = fn;
  }

 private:
  // |PageClient|
  void OnPageChange(const std::string& key, const std::string& value) override;

  // Applies the given |changes| to the current document. The current list of
  // pending operations is merged into the change stream. Implemented in
  // incremental_link.cc.
  void Replay(fidl::VectorPtr<fuchsia::modular::internal::LinkChange> changes);

  // Applies a single LinkChange. Implemented in incremental_link.cc.
  bool ApplyChange(fuchsia::modular::internal::LinkChange* change);

  // Implemented in incremental_link.cc.
  void MakeReloadCall(std::function<void()> done);
  void MakeIncrementalWriteCall(fuchsia::modular::internal::LinkChangePtr data,
                                std::function<void()> done);
  void MakeIncrementalChangeCall(fuchsia::modular::internal::LinkChangePtr data,
                                 uint32_t src);

  bool ApplySetOp(const CrtJsonPointer& ptr, fidl::StringPtr json);
  bool ApplyUpdateOp(const CrtJsonPointer& ptr, fidl::StringPtr json);
  bool ApplyEraseOp(const CrtJsonPointer& ptr);

  static bool MergeObject(CrtJsonValue& target, CrtJsonValue&& source,
                          CrtJsonValue::AllocatorType& allocator);

  void NotifyWatchers(uint32_t src);

  // Counter for LinkConnection IDs used for sequentially assigning IDs to
  // connections. ID 0 is never used so it can be used as pseudo connection ID
  // for WatchAll() watchers. ID 1 is used as the source ID for updates from the
  // Ledger.
  uint32_t next_connection_id_{2};
  static constexpr uint32_t kWatchAllConnectionId{0};
  static constexpr uint32_t kOnChangeConnectionId{1};

  // We can only accept connection requests once the instance is fully
  // initialized. So we queue connections on |requests_| until |ready_| is true.
  bool ready_{};
  std::vector<fidl::InterfaceRequest<fuchsia::modular::Link>> requests_;

  // The value of this fuchsia::modular::Link instance.
  CrtJsonDoc doc_;

  // Fidl connections to this fuchsia::modular::Link instance. We need to
  // explicitly keep track of connections so we can give some watchers only
  // notifications on changes coming from *other* connections than the one the
  // watcher was registered on.
  std::vector<std::unique_ptr<LinkConnection>> connections_;

  // Some watchers do not want notifications for changes made through the
  // connection they were registered on. Therefore, the connection they were
  // registered on is kept associated with them. The connection may still go
  // down before the watcher connection.
  //
  // Some watchers want all notifications, even from changes made through the
  // connection they were registered on. Therefore, they are not associated with
  // a connection, and the connection is recorded as nullptr. These watchers
  // obviously also may survive the connections they were registered on.
  std::vector<std::unique_ptr<LinkWatcherConnection>> watchers_;

  // The hierarchical identifier of this fuchsia::modular::Link instance within
  // its Story.
  fuchsia::modular::LinkPath link_path_;

  // The attributes passed by the link creator to initialize the link.
  const fuchsia::modular::CreateLinkInfoPtr create_link_info_;

  // When the fuchsia::modular::Link instance loses all its
  // fuchsia::modular::Link connections, this callback is invoked. It will cause
  // the fuchsia::modular::Link instance to be deleted. Remaining
  // fuchsia::modular::LinkWatcher connections do not retain the
  // fuchsia::modular::Link instance, but instead can watch it being deleted
  // (through their connection error handler).
  std::function<void()> orphaned_handler_;

  // Ordered key generator for incremental fuchsia::modular::Link values
  KeyGenerator key_generator_;

  // Track changes that have been saved to the Ledger but not confirmed
  std::vector<fuchsia::modular::internal::LinkChange> pending_ops_;

  // The latest key that's been applied to this fuchsia::modular::Link. If we
  // receive an earlier key in OnChange, then replay the history.
  std::string latest_key_;

  OperationQueue operation_queue_;

  // Operations implemented here.
  class ReadLinkDataCall;
  class WriteLinkDataCall;
  class FlushWatchersCall;
  class ReadCall;
  class WriteCall;
  class GetCall;
  class SetCall;
  class UpdateObjectCall;
  class EraseCall;
  class GetEntityCall;
  class WatchCall;
  class ChangeCall;
  // Calls below are for incremental links, which can be found in
  // incremental_link.cc.
  class ReloadCall;
  class IncrementalWriteCall;
  class IncrementalChangeCall;

  FXL_DISALLOW_COPY_AND_ASSIGN(LinkImpl);
};

class LinkConnection : fuchsia::modular::Link {
 public:
  ~LinkConnection() override;

  // Creates a new instance on the heap and registers it with the
  // given LinkImpl, which takes ownership. It cannot be on the stack
  // because it destroys itself when its fidl connection closes. The
  // constructor is therefore private and only accessible from here.
  static void New(LinkImpl* const impl, const uint32_t id,
                  fidl::InterfaceRequest<fuchsia::modular::Link> request) {
    new LinkConnection(impl, id, std::move(request));
  }

 private:
  // Private so it cannot be created on the stack.
  LinkConnection(LinkImpl* impl, uint32_t id,
                 fidl::InterfaceRequest<fuchsia::modular::Link> link_request);

  // |fuchsia::modular::Link|
  void UpdateObject(fidl::VectorPtr<fidl::StringPtr> path,
                    fidl::StringPtr json) override;
  void Set(fidl::VectorPtr<fidl::StringPtr> path,
           fidl::StringPtr json) override;
  void Get(fidl::VectorPtr<fidl::StringPtr> path,
           GetCallback callback) override;
  void Erase(fidl::VectorPtr<fidl::StringPtr> path) override;
  void GetEntity(GetEntityCallback callback) override;
  void SetEntity(fidl::StringPtr entity_reference) override;
  void Watch(
      fidl::InterfaceHandle<fuchsia::modular::LinkWatcher> watcher) override;
  void WatchAll(
      fidl::InterfaceHandle<fuchsia::modular::LinkWatcher> watcher) override;
  void Sync(SyncCallback callback) override;

  LinkImpl* const impl_;
  fidl::Binding<fuchsia::modular::Link> binding_;

  // The ID is used to identify a LinkConnection during notifications of
  // LinkWatchers about value changes, if a fuchsia::modular::LinkWatcher
  // requests to be notified only of changes to the fuchsia::modular::Link value
  // made through other LinkConnections than the one the
  // fuchsia::modular::LinkWatcher was registered through.
  //
  // An ID is unique within one LinkImpl instance over its whole life time. Thus
  // if a LinkConnection is closed its ID and is never reused for new
  // LinkConnection instances.
  const uint32_t id_;

  FXL_DISALLOW_COPY_AND_ASSIGN(LinkConnection);
};

class LinkWatcherConnection {
 public:
  LinkWatcherConnection(LinkImpl* impl,
                        fuchsia::modular::LinkWatcherPtr watcher,
                        uint32_t conn);
  ~LinkWatcherConnection();

  // Notifies the fuchsia::modular::LinkWatcher in this connection, unless src
  // is the LinkConnection this Watcher was registered on.
  void Notify(fidl::StringPtr value, uint32_t src);

 private:
  // The LinkImpl this instance belongs to.
  LinkImpl* const impl_;

  fuchsia::modular::LinkWatcherPtr watcher_;

  // The ID of the LinkConnection this fuchsia::modular::LinkWatcher was
  // registered on.
  const uint32_t conn_;

  FXL_DISALLOW_COPY_AND_ASSIGN(LinkWatcherConnection);
};

}  // namespace modular

#endif  // PERIDOT_BIN_USER_RUNNER_STORY_RUNNER_LINK_IMPL_H_
