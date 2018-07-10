// ccommon - a cache common library.
// Copyright (C) 2013 Twitter, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! Glue between rust's standard `log` crate and ccommon's cc_log logger.
//!
//! This library allows rust embedded into projects using ccommon to use
//! the same logger provided by `cc_log.h`
//!
//! # Safety
//!
//! This library is AGGRESSIVELY NON-THREADSAFE...for SPEED.
//!
//! If you are using the standard rust macros for logging, you must
//! ensure that you are running your rust code from a single thread or
//! _bad things may happen_.
//!

// TODO(simms): add C-side setup code here.

use bstring::BString;
use bstring::BStringRef;
use cc_binding as bind;
use rslog;
use rslog::{Log, Metadata, Record, SetLoggerError};
pub use rslog::Level;
use rslog::LevelFilter;
use std::ptr;
use std::result::Result;
use std::sync::atomic::{ATOMIC_USIZE_INIT, AtomicUsize, Ordering};
use time;

/*
binding:

pub struct logger {
    pub name: *mut ::std::os::raw::c_char,
    pub fd: ::std::os::raw::c_int,
    pub buf: *mut rbuf,
}
*/
pub type CLogger = bind::logger;

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

/// The API around writing to the underlying logger
#[doc(hidden)]
trait RawWrapper: Log {
    fn inner(&self) -> *mut CLogger;
    unsafe fn inner_write(&self, message: &str) -> bool;
    unsafe fn inner_flush(&self);
    fn is_nop(&self) -> bool { true }
}

struct NopRawWrapper;

impl RawWrapper for NopRawWrapper {
    fn inner(&self) -> *mut CLogger { ptr::null_mut() }
    unsafe fn inner_write(&self, _: &str) -> bool { false }
    unsafe fn inner_flush(&self) {}
}

impl Log for NopRawWrapper {
    fn enabled(&self, _: &Metadata) -> bool { false }
    fn log(&self, _: &Record) {}
    fn flush(&self) {}
}


#[doc(hidden)]
#[repr(C)]
struct RawLogger {
    ptr: *mut CLogger,
    level: LevelFilter,
}

impl Default for RawLogger {
    fn default() -> Self {
        RawLogger { ptr: ptr::null_mut(), level: LevelFilter::Off }
    }
}

impl RawWrapper for RawLogger {
    fn inner(&self) -> *mut CLogger {
        self.ptr
    }

    #[inline]
    unsafe fn inner_write(&self, message: &str) -> bool {
        if self.ptr.is_null() {
            return false;
        }

        let msg = message.as_bytes();
        let b = bind::log_write(self.ptr, msg.as_ptr() as *mut i8, msg.len() as u32);
        if !b {
            eprintln!("failed to write to log: {}", message);
        }
        b
    }

    #[inline]
    unsafe fn inner_flush(&self) {
        if !self.ptr.is_null() {
            bind::log_flush(self.ptr);
        }
    }

    #[inline]
    fn is_nop(&self) -> bool { false }
}

impl Log for RawLogger {
    #[inline]
    fn enabled(&self, metadata: &Metadata) -> bool {
        metadata.level() <= self.level
    }

    #[inline]
    fn log(&self, record: &Record) {
        // taken from borntyping/rust-simple_logger
        if self.enabled(record.metadata()) {
            let msg = format!(
                "{} {:<5} [{}] {}\n",
                time::strftime("%Y-%m-%d %H:%M:%S", &time::now()).unwrap(),
                record.level().to_string(),
                record.module_path().unwrap_or_default(),
                record.args());

            unsafe { self.inner_write(msg.as_ref()); }
        }
    }

    #[inline]
    fn flush(&self) {
        unsafe { self.inner_flush(); }
    }
}

unsafe impl Send for RawLogger {}

unsafe impl Sync for RawLogger {}


fn make_logger() -> &'static Logger {
    // NOTE(simms): this is how you get a &'static T reference.
    // This drove me nuts for hours trying to figure out:
    // * A static reference is a reference that lives for the life of the program
    // * If you create a new Box, copy an object to it, then convert that to a raw
    //   pointer, you've essentially malloced, then forgotten (in terms of rust lifetimes)
    //   that object. This essentially makes that reference _live for the life of the program_.
    // * There is an implicit conversion (bijective) between &mut T and *mut T, so we
    //   can assign a *mut T to a variable that expects a &'static mut T.
    Box::leak(Box::new(Logger::new()))
}

static mut RAW_LOG: &'static RawWrapper = &NopRawWrapper;

lazy_static! {
    static ref LOGGER: &'static Logger = make_logger();
}

// `Logger` is a shim struct that we can create a static instance of and hand through to the
// `log` crate. It forwards calls to the underlying `logger` instance via a static reference.
// It also behaves correctly if `log` is called after `log_rs_unset` has been called (i.e.
// there is no underlying `RawLogger` configured).
#[doc(hidden)]
struct Logger {}

impl Logger {
    pub fn new() -> Self { Logger {} }
}

impl rslog::Log for Logger {
    #[inline]
    fn enabled(&self, _metadata: &Metadata) -> bool { true }

    #[inline]
    fn log(&self, record: &Record) { unsafe { RAW_LOG.log(record) }; }

    #[inline]
    fn flush(&self) { unsafe { RAW_LOG.flush() }; }
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

fn get_state() -> usize {
    STATE.fetch_add(0, Ordering::SeqCst)
}

/// Establishes this module as the rust `log` crate's singleton logger. We first install a
/// no-op logger, and then replace it with an actual logging instance that has an output.
/// Returns a [`ccommon::Result`] that is Ok on success and will be a [`LoggingError`] on failure.
pub(crate) fn try_init_logger() -> Result<(), LoggingError> {
    match get_state() {
        UNINITIALIZED => (),
        INITIALIZED => return Ok(()),
        FAILED => return Err(LoggingError::LoggerRegistrationFailure),
        _ => (),
    };

    if STATE.compare_and_swap(UNINITIALIZED, INITIALIZING, Ordering::SeqCst) != UNINITIALIZED {
        return Err(LoggingError::LoggingAlreadySetUp)
    }

    match rslog::set_logger(*LOGGER) {
        Ok(_) => {
            // set the default max level to 'trace' and provide an API to adjust it
            rslog::set_max_level(rslog::LevelFilter::Trace);
            STATE.store(INITIALIZED, Ordering::SeqCst);
            Ok(())
        }
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
    InvalidUTF8 = 4,
}

impl From<LoggingError> for LoggerStatus {
    fn from(e: LoggingError) -> Self {
        match e {
            LoggingError::LoggerRegistrationFailure => LoggerStatus::RegistrationFailure,
            LoggingError::LoggingAlreadySetUp => LoggerStatus::LoggerAlreadySetError,
        }
    }
}

/// This function will set up our logger as the default
/// one for the `log` crate at the given
/// `level`. This function must be called as early
/// as possible in program setup, followed by
/// a call to [`log_rs_set`]
///
/// [`log_rs_set`]: fn.log_rs_set.html
///
/// # Errors
///
/// If we fail to set up our logger, we will print a
/// message on stderr and return
/// [`LoggerStatus::RegistrationFailure`], which means
/// we could not register ourselves as the provider
/// of the logging backend for the `log` crate.
/// This should be treated as a fatal error because
/// one cannot un-register the existing backend, and
/// this operation will *never* succeed.
///
/// If this method had been called previously,
/// and we are the provider of the logging framework,
/// we return [`Ok`].
///
/// # Safety
///
/// The caller must ensure that the lifetime of `logger`
/// lives until `rust_cc_log_destroy`
/// is called or the program terminates.
#[no_mangle]
pub extern "C" fn log_rs_setup() -> LoggerStatus {
    match try_init_logger() {
        Ok(_) => LoggerStatus::OK,
        Err(LoggingError::LoggingAlreadySetUp) => {
            eprintln!("cc_log logging already set up");
            LoggerStatus::OK
        }
        Err(LoggingError::LoggerRegistrationFailure) => {
            eprintln!("Error setting up cc_log! {}", LoggingError::LoggerRegistrationFailure);
            LoggerStatus::RegistrationFailure
        }
    }
}

/// This function sets the cc_log logger instance to be the
/// sink for messages logged from the `log` crate. The user
/// must call [`log_rs_setup`] _before_ calling this function
/// to register us as the backend for the `log` crate.
///
/// # Panics
///
/// This function will panic if the `logger` pointer is NULL.
///
/// # Errors
///
/// Returns [`LoggerNotSetupError`] if [`log_rs_setup`] was NOT
/// called prior to this function being called.
///
/// If there's already been a `logger` instance set up, then we will return
/// [`LoggerAlreadySetError`]. This error need not be fatal.
///
/// [`log_rs_setup`]: fn.log_rs_setup.html
/// [`LoggerNotSetupError`]: enum.LoggerStatus.html
/// [`LoggerAlreadySetError`]: enums.LoggerStatus.html
///
/// # Undefined Behavior
///
/// If the `logger` pointer becomes invalid before [`log_rs_unset`] is called, the
/// behavior is undefined.
///
/// [`log_rs_unset`]: fn.log_rs_unset.html
#[no_mangle]
pub extern "C" fn log_rs_set(logger: *mut CLogger, level: Level) -> LoggerStatus {
    assert!(!logger.is_null());

    let cur_state = get_state();
    if cur_state != INITIALIZED {
        eprintln!("log_rs_set: error state was: {}", cur_state);
        return LoggerStatus::LoggerNotSetupError
    }

    let nop = unsafe { RAW_LOG.is_nop() };

    if nop {
        let rl = Box::new(RawLogger { ptr: logger, level: level.to_level_filter() });
        unsafe { RAW_LOG = Box::leak(rl) };
        
        LoggerStatus::OK
    } else {
        LoggerStatus::LoggerAlreadySetError
    }
}

/// Returns true if [`log_rs_setup`] has been called previously and
/// it is safe to set the logger instance.
#[no_mangle]
pub unsafe extern "C" fn log_rs_is_setup() -> bool {
    if get_state() != INITIALIZED {
        return false;
    }

    !RAW_LOG.is_nop()
}

/// Log a message through the rust path at the given level.
/// Useful for testing from the C side that the rust side is properly set up.
///
/// # Errors
///
/// [`LoggerStatus::InvalidUTF8`] will be returned if the
/// bstring's contents are not valid UTF8.
///
/// # Panics
///
/// This function panics if the `msg` pointer is NULL.
#[no_mangle]
pub unsafe extern "C" fn log_rs_log(msg: *const BString, level: Level) -> LoggerStatus {
    assert!(!msg.is_null());
    let bsr = BStringRef::from_raw(msg);

    match bsr.to_str() {
        Ok(s) => {
            log!(level, "{}", s);
            LOGGER.flush();
        },
        Err(err) => {
            eprintln!("error in log_rs_log: {:?}", err);
            return LoggerStatus::InvalidUTF8;
        }
    }

    LoggerStatus::OK
}


/// Set the level at which the rust logging macros should be active.
/// Default is 'Trace' which allows messages at all levels.
pub extern "C" fn log_rs_set_max_level(level: Level) {
    rslog::set_max_level(level.to_level_filter())
}

/// Replace the existing `logger` instance with a no-op logger and returns
/// the instance. If there is no current logger instance, returns NULL.
#[no_mangle]
pub unsafe extern "C" fn log_rs_unset() -> *mut CLogger {
    if RAW_LOG.is_nop() {
        return ptr::null_mut()
    }

    let orig = &*RAW_LOG;
    RAW_LOG = &NopRawWrapper;
    
    orig.inner()
}


/// Flushes the current logger instance by calling the underlying `log_flush` function in cc_log.
///
/// # Undefined Behavior
///
/// If the underlying `logger` pointer has become invalid the behavior is undefined.
#[no_mangle]
pub unsafe extern "C" fn log_rs_flush() {
    RAW_LOG.inner_flush();
}
