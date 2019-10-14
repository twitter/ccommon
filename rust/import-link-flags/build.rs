
use std::path::Path;
use std::io::Read;
use std::env;
use std::fs::File;

fn process_flags(flags: &str) {
    eprintln!("{}", flags);

    let flags = flags.split(" ");
    let mut prev = None;

    for flag in flags {
        if flag == "" {
            continue;
        }
        
        if let Some(prevflag) = prev {
            println!("cargo:rustc-flags={} {}", prevflag, flag);
            prev = None;
            continue;
        }

        if flag == "-l" || flag == "-L" {
            prev = Some(flag);
            continue;   
        }

        let path = Path::new(flag);
        if !path.is_file() && path.exists() {
            panic!("Attempting to link to {} which is not a file", flag);
        }

        if let Some(parent) = path.parent() {
            println!("cargo:rustc-flags=-L{}", parent.display());
        }

        let filename = match path.file_name() {
            Some(filename) => filename,
            None => panic!("Attempted to link to a directory: {}", path.display())
        };

        println!("cargo:rustc-flags=-l{}", Path::new(filename).display());
    }

    if let Some(prev) = prev {
        panic!("Unmatched '{}' flag", prev);
    }
}

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-env-changed=CCOMMON_LINK_FLAGS_FILE");

    if let Some(var) = env::var_os("CCOMMON_LINK_FLAGS_FILE") {
        let path = Path::new(&var);

        println!("cargo:rerun-if-changed={}", path.display());

        let mut file = match File::open(&path) {
            Ok(file) => file,
            Err(e) => panic!("Unable to open file {}: {}", path.display(), e)
        };

        let mut bytes = Vec::new();
        file.read_to_end(&mut bytes).expect("Failed to read file");

        let flags = String::from_utf8(bytes).expect("Build flags contained invalid utf8");
        process_flags(&flags);
    }
}
