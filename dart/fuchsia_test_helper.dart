// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ignore_for_file: implementation_imports

import 'dart:async';
import 'package:test/src/backend/declarer.dart';
import 'package:test/src/backend/group.dart';
import 'package:test/src/runner/runner_suite.dart';
import 'package:test/src/runner/configuration/suite.dart';
import 'package:test/src/runner/engine.dart';
import 'package:test/src/runner/plugin/environment.dart';
import 'package:test/src/runner/reporter/expanded.dart';

/// Use `package:test` internals to run test functions.
///
/// `package:test` doesn't offer a public API for running tests. This calls
/// private APIs to invoke test functions and collect the results.
///
/// See: https://github.com/dart-lang/test/issues/48
///      https://github.com/dart-lang/test/issues/12
///      https://github.com/dart-lang/test/issues/99
Future<bool> runFuchsiaTests(List<Function> mainFunctions) async {
  final Declarer declarer = new Declarer();

  for (final Function main in mainFunctions) {
    // TODO: use a nested declarer for each main?
    declarer.declare(main);
  }

  final Group group = declarer.build();

  final suite = new RunnerSuite(
      const PluginEnvironment(), SuiteConfiguration.empty, group);

  final engine = new Engine();
  engine.suiteSink.add(suite);
  engine.suiteSink.close();
  ExpandedReporter.watch(engine,
      color: false, printPath: false, printPlatform: false);

  return engine.run();
}
