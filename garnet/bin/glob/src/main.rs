// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
use std::env;
use std::process::exit;

use glob::glob;

// This binary will test glob path passed to it as arg and write the path to
// stdout if found, else will write error to stderr and return 1.
fn main() {
    let argv = env::args().collect::<Vec<String>>();
    println!("argv.len() == {}", argv.len());
    if argv.len() != 2 {
        eprintln!("Usage: {} <glob_path>", argv[0]);
        exit(1);
    }

    let entries = match glob(&argv[1]) {
        Ok(v) => v,
        Err(e) => {
            eprintln!("glob failed: {:?}", e);
            exit(1);
        }
    };
    let cnt = entries.fold(0, |sum, path| {
        match path {
            Ok(p) => println!("{}", p.display()),
            Err(e) => {
                eprintln!("glob failed: {:?}", e);
                exit(1);
            }
        };
        sum + 1
    });

    if cnt == 0 {
        eprintln!("no match found");
        exit(1);
    }
}
