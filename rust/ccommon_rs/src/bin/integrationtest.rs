extern crate cc_binding as bind;
extern crate ccommon_rs as ccommon;
#[macro_use]
extern crate log as rs_log;
extern crate tempfile;

use ccommon::log as cc_log;
use ccommon::log::{Level, LoggerStatus};
use ccommon::Result;
use std::ffi::CString;
use std::io::Read;
use std::str;
use tempfile::NamedTempFile;

struct CCLogState {
    logfile: tempfile::NamedTempFile,
    logger: *mut bind::logger,
    stats: *mut bind::log_metrics_st,
}

impl Drop for CCLogState {
    fn drop(&mut self) {
        unsafe {
            bind::log_destroy(&mut self.logger);
            bind::log_metrics_destroy(&mut self.stats);
        }
    }
}

fn setup_cc_log() -> Result<CCLogState> {
    let logfile = NamedTempFile::new()?;

    let stats: *mut bind::log_metrics_st = unsafe { bind::log_metrics_create() };
    assert!(!stats.is_null());

    unsafe { bind::log_setup(stats) };

    let path = CString::new(logfile.path().to_str().unwrap())?;
    let logger: *mut bind::logger = unsafe {
        bind::log_create(path.into_raw(), 0)
    };
    assert!(!logger.is_null());

    Ok(CCLogState{logfile, stats, logger})
}

fn basic_roundtrip_test() -> Result<()> {
    let mut state = setup_cc_log()?;

    assert_eq!(cc_log::rust_cc_log_setup(), LoggerStatus::OK);
    assert_eq!(cc_log::rust_cc_log_set(state.logger, Level::Debug), LoggerStatus::OK);

    let logged_msg = "this message should be sent to the cc logger";

    warn!("msg: {}", logged_msg);

    let mut buf = Vec::new();
    {
        let f = state.logfile.as_file_mut();
        let sz = f.read_to_end(&mut buf)?;
        assert!(sz > logged_msg.len());
    }
    let s = str::from_utf8(&buf[..])?;
    assert!(s.rfind(logged_msg).is_some());

    drop(state);

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
