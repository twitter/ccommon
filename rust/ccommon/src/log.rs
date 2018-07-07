use cc_binding as bind;
use rslog::{Level, Log, Metadata, Record};
use std::cell::Cell;
use std::ptr;
use std::sync::Mutex;
use time;


/*
binding:

pub struct logger {
    pub name: *mut ::std::os::raw::c_char,
    pub fd: ::std::os::raw::c_int,
    pub buf: *mut rbuf,
}
*/

#[repr(C)]
struct CCPtr {
    ptr: *mut bind::logger,
    level: Level,
}

impl CCPtr {
    unsafe fn write(&self, message: &str) -> bool {
        let msg = message.as_bytes();
        bind::log_write(
            self.ptr,
            msg.as_ptr() as *mut i8,
            msg.len() as u32
        )
    }

    unsafe fn _flush(&self) {
        bind::log_flush(self.ptr);
    }
}

impl Log for CCPtr {
    #[inline]
    fn enabled(&self, metadata: &Metadata) -> bool {
        metadata.level() <= self.level
    }

    // taken from borntyping/rust-simple_logger

    fn log(&self, record: &Record) {
        if self.enabled(record.metadata()) {
            let msg = format!(
                "{} {:<5} [{}] {}",
                time::strftime("%Y-%m-%d %H:%M:%S", &time::now()).unwrap(),
                record.level().to_string(),
                record.module_path().unwrap_or_default(),
                record.args());

            unsafe { self.write(msg.as_ref()); }
        }
    }

    fn flush(&self) {
        unsafe { self._flush(); }
    }
}

unsafe impl Send for CCPtr {}
unsafe impl Sync for CCPtr {}


struct CCLog {
    inner: Mutex<Cell<Option<CCPtr>>>
}

impl CCLog {
    fn new() -> CCLog {
        CCLog {
            inner: Mutex::new(Cell::new(None))
        }
    }

    /// Replace the current static logging instance with a different instance.
    /// Returns the original instance.
    fn replace(&self, opt_log: Option<CCPtr>) -> Option<CCPtr> {
        let cur = self.inner.lock().unwrap();
        cur.replace(opt_log)
    }

    /// Set the current logging instance if it hasn't been set yet. Returns
    /// Err(SetLoggerError) if the instance has already been initialized.
    fn try_set(&self, log: CCPtr) -> bool {
        let mut cur = self.inner.lock().unwrap();

        let b = { cur.get_mut().is_none() };

        if b {
            cur.set(Some(log));
            return true
        }

        false
    }
}

impl Log for CCLog {
    fn enabled(&self, _metadata: &Metadata) -> bool { true }

    fn log(&self, record: &Record) {
        let mut i = self.inner.lock().unwrap();
        if let Some(lg) = i.get_mut() {
            lg.log(record)
        }
    }

    fn flush(&self) {
        let mut i = self.inner.lock().unwrap();
        if let Some(lg) = i.get_mut() {
            lg.flush()
        }
    }
}


lazy_static! {
    static ref CC_LOG: CCLog = {
        CCLog::new()
    };
}

pub extern "C" fn rust_cc_log_setup(logger: *mut bind::logger, level: Level) -> bool {
    CC_LOG.try_set(CCPtr{ptr: logger, level})
}

pub extern "C" fn rust_cc_log_destroy() -> *mut bind::logger {
    match CC_LOG.replace(None) {
        Some(ccl) => ccl.ptr,
        None => ptr::null_mut(),
    }
}

pub extern "C" fn rust_cc_log_flush() {
    CC_LOG.flush()
}
