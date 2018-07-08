use cc_binding as bind;
use lazy_static;
use rslog;
use rslog::{Log, Metadata, Record, SetLoggerError};
pub use rslog::Level;
use std::cell::Cell;
use std::ptr;
use std::sync::atomic::{ATOMIC_USIZE_INIT, AtomicUsize, Ordering};
use std::sync::Mutex;
use std::result::Result;
use time;

/*
binding:

pub struct logger {
    pub name: *mut ::std::os::raw::c_char,
    pub fd: ::std::os::raw::c_int,
    pub buf: *mut rbuf,
}
*/

#[derive(Fail, Debug)]
pub enum LoggingError {
    #[fail(display = "logging already set up")]
    LoggingAlreadySetUp,

    #[fail(display = "Other logger has already been set up with log crate")]
    LoggerRegistrationFailure,
}

impl From<SetLoggerError> for LoggingError {
    fn from(_: SetLoggerError) -> Self {
        LoggingError::LoggerRegistrationFailure
    }
}

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
            msg.len() as u32,
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


struct CCLog;

impl rslog::Log for CCLog {
    fn enabled(&self, _metadata: &Metadata) -> bool { true }

    fn log(&self, record: &Record) {
        let mut i = CC_PTR.lock().unwrap();
        if let Some(lg) = i.get_mut() {
            lg.log(record)
        }
    }

    fn flush(&self) {
        let mut i = CC_PTR.lock().unwrap();
        if let Some(lg) = i.get_mut() {
            lg.flush()
        }
    }
}

lazy_static! {
    static ref CC_PTR: Mutex<Cell<Option<CCPtr>>> = {
        Mutex::new(Cell::new(None))
    };
}

/// Replace the current static logging instance with a different instance.
/// Returns the original instance.
fn cc_ptr_replace(opt_log: Option<CCPtr>) -> Option<CCPtr> {
    lazy_static::initialize(&CC_PTR);

    let cell = CC_PTR.lock().unwrap();
    cell.replace(opt_log)
}

/// Set the current logging instance if it hasn't been set yet. Returns
/// [`Err(LoggingError)`] if the instance has already been initialized.
fn cc_ptr_try_set(log: CCPtr) -> Result<(), LoggingError> {
    lazy_static::initialize(&CC_PTR);

    let mut cur = CC_PTR.lock().unwrap();

    let b = { cur.get_mut().is_none() };

    if b {
        cur.set(Some(log));
        return Ok(());
    }

    Err(LoggingError::LoggingAlreadySetUp)
}

// Copied from the log crate. This lets us track if we've already succeeded in
// registering as the logging backend.
static STATE: AtomicUsize = ATOMIC_USIZE_INIT;

// There are three different states that we care about: the logger's
// uninitialized, the logger's initializing (set_logger's been called but
// LOGGER hasn't actually been set yet), or the logger's active.
const UNINITIALIZED: usize = 0;
const INITIALIZING: usize = 1;
const INITIALIZED: usize = 2;
const FAILED: usize = 3;


/// Establishes this module as the rust `log` crate's singleton logger. We first install a
/// no-op logger, and then replace it with an actual logging instance that has an output.
/// Returns a [`ccommon::Result`] that is Ok on success and will be a [`LoggingError`] on failure.
pub(crate) fn try_init_logger() -> Result<(), LoggingError> {
    match STATE.fetch_add(0, Ordering::SeqCst) {
        INITIALIZED => return Ok(()),
        FAILED => return Err(LoggingError::LoggerRegistrationFailure),
        _ => (),
    };

    if STATE.compare_and_swap(UNINITIALIZED, INITIALIZING, Ordering::SeqCst) != UNINITIALIZED {
        return Err(LoggingError::LoggingAlreadySetUp)
    }

    match rslog::set_boxed_logger(Box::new(CCLog)) {
        Ok(_) => { STATE.store(INITIALIZED, Ordering::SeqCst); Ok(()) }
        Err(err) => {
            eprintln!("Error setting up logger: {}", err);
            STATE.store(FAILED, Ordering::SeqCst);
            Err(err)
        }
    }.map_err(|e| e.into())
}

#[repr(u32)]
#[derive(Debug, PartialEq, PartialOrd, Eq)]
pub enum LoggerStatus {
    OK = 0,
    LoggerNotSetupError = 1,
    RegistrationFailure = 2,
    LoggerAlreadySetError = 3,
}

impl From<LoggingError> for LoggerStatus {
    fn from(e: LoggingError) -> Self {
        match e {
            LoggingError::LoggerRegistrationFailure => LoggerStatus::RegistrationFailure,
            LoggingError::LoggingAlreadySetUp => LoggerStatus::LoggerAlreadySetError,
        }
    }
}

/// This function will set up our logger as the default one for the `log` crate at the given
/// `level`. This function must be called as early as possible in program setup, followed by
/// a call to [`rust_cc_log_set`]
///
/// [`rust_cc_log_set`]: fn.rust_cc_log_set.html
///
/// # Errors
///
/// If we fail to set up our logger, we will print a message on stderr and return
/// [`LoggerStatus::RegistrationFailure`], which means we could not register ourselves as the provider
/// of the logging backend for the `log` crate. This should be treated as a fatal error because
/// one cannot un-register the existing backend, and this operation will *never* succeed.
///
/// If this method had been called previously, and we are the provider of the logging framework,
/// we return [`Ok`].
///
///
/// # Panics
///
/// This function panics if `logger` is NULL.
///
/// # Safety
///
/// The caller must ensure that the lifetime of `logger` lives until `rust_cc_log_destroy`
/// is called or the program terminates.
#[no_mangle]
pub extern "C" fn rust_cc_log_setup() -> LoggerStatus {
    match try_init_logger() {
        Ok(_) | Err(LoggingError::LoggingAlreadySetUp) => LoggerStatus::OK,
        Err(LoggingError::LoggerRegistrationFailure) => {
            eprintln!("Error setting up cc_log! {}", LoggingError::LoggerRegistrationFailure);
            LoggerStatus::RegistrationFailure
        }
    }
}

/// This function sets the cc_log logger instance to be the sink for messages logged from
/// the `log` crate. The user must call [`rust_cc_log_setup`] _before_ calling this function
/// to register us as the backend for the `log` crate.
///
/// # Panics
///
/// This function will panic if the `logger` pointer is NULL.
///
/// # Errors
///
/// Returns [`LoggerNotSetupError`] if [`rust_cc_log_setup`] was NOT
/// called prior to this function being called.
///
/// If there's already been a `logger` instance set up, then we will return
/// [`LoggerAlreadySetError`]. This error need not be fatal.
///
/// [`rust_cc_log_setup`]: fn.rust_cc_log_setup.html
/// [`LoggerNotSetupError`]: enum.LoggerStatus.html
/// [`LoggerAlreadySetError`]: enums.LoggerStatus.html
#[no_mangle]
pub extern "C" fn rust_cc_log_set(logger: *mut bind::logger, level: Level) -> LoggerStatus {
    assert!(logger.is_null());

    if STATE.fetch_add(0, Ordering::SeqCst) != INITIALIZED {
        return LoggerStatus::LoggerNotSetupError
    }
    
    cc_ptr_try_set(CCPtr { ptr: logger, level })
        .map(|_| LoggerStatus::OK)
        .unwrap_or_else(LoggerStatus::from)
}

/// This function replaces the existing `logger` instance with a no-op logger and returns
/// the instance. If there is no current logger instance, returns NULL.
#[no_mangle]
pub extern "C" fn rust_cc_log_unset() -> *mut bind::logger {
    match cc_ptr_replace(None) {
        Some(ccl) => ccl.ptr,
        None => ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn rust_cc_log_flush() {
    CCLog.flush();
}
