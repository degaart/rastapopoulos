use std::{env, path::PathBuf};

fn main() {
    let out_dir = PathBuf::from(env::var_os("OUT_DIR").unwrap());
    let profile_dir = out_dir
        .ancestors()
        .find(|path| path.file_name().is_some_and(|name| name == "build"))
        .unwrap()
        .parent()
        .unwrap();
    let map_file = profile_dir.join("hello_rust.map");
    let linker_script =
        PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap()).join("../user.ld");
    let library_dir =
        PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap()).join("../lib/build");

    println!("cargo:rustc-link-search=native={}", library_dir.display());
    println!("cargo:rustc-link-lib=static=crt0");
    println!("cargo:rustc-link-arg=-T{}", linker_script.display());
    println!("cargo:rustc-link-arg=-Map={}", map_file.display());
    println!("cargo:rerun-if-changed={}", linker_script.display());
    println!(
        "cargo:rerun-if-changed={}",
        library_dir.join("libcrt0.a").display()
    );
}
