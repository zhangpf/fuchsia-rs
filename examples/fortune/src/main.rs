// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use rand;

const FORTINUES: [&'static str; 4] = [
  "If we have data, let’s look at data. If all we have are opinions, let’s go \
   \nwith mine. -- Jim Barksdale",
  "Things that are impossible just take longer.",
  "Better lucky than good.",
  "Fortune favors the bold.",
];

fn main() {
  println!("{}\n", FORTINUES[rand::random::<usize>() % FORTINUES.len()]);
}
