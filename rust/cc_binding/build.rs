extern crate bindgen;

use std::env;
use std::fs;
use std::io;
use std::path::PathBuf;
use std::io::prelude::*;
use std::os::unix::fs as unix_fs;

fn get_cmake_binary_dir() -> io::Result<String> {
    // this file is written by cmake on each run, updated with the location of
    // the build directory.
    let mut fp = fs::File::open("../CMAKE_BINARY_DIR")?;
    let mut buf = String::new();
    let n = fp.read_to_string(&mut buf)?;
    assert!(n > 0, "file was empty");
    Ok(String::from(buf.trim_right()))
}

fn main() {
    println!("cargo:rustc-link-lib=static=ccommon-1.2.0");

    let include_path = fs::canonicalize("./../../include").unwrap();

    let cmake_binary_dir = match get_cmake_binary_dir() {
        Ok(p) => p,
        Err(err) => panic!("Failed locating the CMAKE_BINARY_DIR file: {:#?}", err),
    };

    let cbd = PathBuf::from(cmake_binary_dir);

    let mut config_h_dir = cbd.clone();
    config_h_dir.push("ccommon");

    let mut lib_dir = cbd.clone();
    lib_dir.push("lib");

    println!("cargo:rustc-link-search=native={}", lib_dir.to_str().unwrap());

    let bindings = bindgen::Builder::default()
        .clang_args(vec![
            "-I", include_path.to_str().unwrap(),
            "-I", config_h_dir.to_str().unwrap(),
            "-I", cbd.to_str().unwrap(),
            "-L", lib_dir.to_str().unwrap(),
        ])
        .header("wrapper.h")
        .blacklist_type("max_align_t") // https://github.com/rust-lang-nursery/rust-bindgen/issues/550
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");

    // ./target/debug/build/cc_binding-27eac70f0fa2e180/out  <<- starts here

    // cc_binding-27eac70f0fa2e180
    let symlink_content =
        out_path.parent().unwrap().file_name().unwrap();

    let build_dir = out_path.parent().and_then(|p| p.parent()).unwrap();

    let link_location = build_dir.join("cc_binding");
    let _ = fs::remove_file(link_location.as_path());
    unix_fs::symlink(symlink_content, link_location).unwrap();
}

