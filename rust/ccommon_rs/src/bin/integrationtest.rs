extern crate cc_binding as bind;
extern crate ccommon_rs as ccommon;
#[macro_use]
extern crate log as rs_log;
extern crate tempfile;

use ccommon::log as cc_log;
use ccommon::log::{Level, LoggerStatus};
use ccommon::Result;
use std::ffi::CString;
use std::fs::File;
use std::io::Read;
use std::str;
use rs_log::LevelFilter;


fn basic_roundtrip_test() -> Result<()> {
    let mut stats: *mut bind::log_metrics_st = unsafe { bind::log_metrics_create() };
    assert!(!stats.is_null());

    unsafe { bind::log_setup(stats) };

    let path = "/tmp/logtest.log";

    let logger: *mut bind::logger = unsafe {
        bind::log_create(CString::new(path)?.into_raw(), 0)
    };
    assert!(!logger.is_null());

    assert_eq!(cc_log::rust_cc_log_setup(), LoggerStatus::OK);
    assert_eq!(cc_log::rust_cc_log_set(logger, Level::Debug), LoggerStatus::OK);
    rs_log::set_max_level(LevelFilter::Trace);

    let logged_msg = "this message should be sent to the cc logger";

    error!("msg: {}", logged_msg);

    cc_log::rust_cc_log_flush();

    let mut buf = Vec::new();
    {
        let mut fp = File::open(path)?;
        let sz = fp.read_to_end(&mut buf)?;
        assert!(sz > logged_msg.len());
    }
    let s = str::from_utf8(&buf[..])?;
    assert!(s.rfind(logged_msg).is_some());

    let mut ptr = cc_log::rust_cc_log_unset();
    assert_eq!(ptr, logger);

    unsafe { bind::log_destroy(&mut ptr) };
    unsafe { bind::log_metrics_destroy(&mut stats) }

    Ok(())
}

fn main() {
    std::process::exit(
        match basic_roundtrip_test() {
            Ok(_) => {
                eprintln!("passed!");
                0
            },
            Err(err) => {
                eprintln!("failed: {}", err);
                1
            }
        }
    )
}
