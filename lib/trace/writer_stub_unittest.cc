// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/tracing/lib/trace/writer.h"

#include "gtest/gtest.h"

namespace tracing {
namespace writer {
namespace {

TEST(WriterStubTest, AllFunctionsDoNothing) {
  EXPECT_FALSE(StartTracing(mx::vmo(), mx::eventpair(), {},
                            [](TraceDisposition disposition) {}));
  StopTracing();
  EXPECT_FALSE(IsTracingEnabled());
  EXPECT_FALSE(IsTracingEnabledForCategory("cat"));
  EXPECT_FALSE(TraceWriter::Prepare());
  EXPECT_FALSE(CategorizedTraceWriter::Prepare("cat"));
}

}  // namespace
}  // namespace writer
}  // namespace tracing
