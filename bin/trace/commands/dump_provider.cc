// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include <mx/socket.h>

#include "apps/tracing/src/trace/commands/dump_provider.h"

#include "lib/ftl/strings/string_number_conversions.h"
#include "lib/ftl/time/time_delta.h"
#include "lib/mtl/tasks/message_loop.h"

namespace tracing {
namespace {

constexpr ftl::TimeDelta kReadTimeout = ftl::TimeDelta::FromSeconds(5);
constexpr size_t kBufferSize = 16 * 1024;

}  // namespace

Command::Info DumpProvider::Describe() {
  return Command::Info{[](modular::ApplicationContext* context) {
                         return std::make_unique<DumpProvider>(context);
                       },
                       "dump-provider",
                       "dumps provider with specified id",
                       {}};
}

DumpProvider::DumpProvider(modular::ApplicationContext* context)
    : CommandWithTraceController(context) {}

void DumpProvider::Run(const ftl::CommandLine& command_line) {
  if (command_line.positional_args().size() != 1) {
    err() << "Need provider id, please check your command invocation"
          << std::endl;
    return;
  }

  uint32_t provider_id;
  if (!ftl::StringToNumberWithError(command_line.positional_args()[0],
                                    &provider_id)) {
    err() << "Failed to parse provider id";
    return;
  }

  mx::socket incoming, outgoing;
  mx_status_t status = mx::socket::create(0u, &incoming, &outgoing);
  FTL_CHECK(status == NO_ERROR);

  trace_controller()->DumpProvider(provider_id, std::move(outgoing));

  std::vector<uint8_t> buffer(kBufferSize);
  for (;;) {
    mx_signals_t pending;
    status = incoming.wait_one(MX_SOCKET_READABLE | MX_SOCKET_PEER_CLOSED,
                               kReadTimeout.ToNanoseconds(), &pending);
    FTL_CHECK(status == NO_ERROR);

    if (!(pending & MX_SOCKET_READABLE))
      break;

    mx_size_t actual;
    status = incoming.read(0u, buffer.data(), buffer.size(), &actual);
    FTL_CHECK(status == NO_ERROR);

    out().write(reinterpret_cast<const char*>(buffer.data()), actual);
  }
  out() << std::endl;

  mtl::MessageLoop::GetCurrent()->QuitNow();
}

}  // namespace tracing
