extern crate cc_binding;
extern crate log as rslog;
extern crate time;
#[macro_use]
extern crate lazy_static;
extern crate failure;
#[macro_use]
extern crate failure_derive;
extern crate tempfile;

pub mod bstring;
pub mod log;

use std::result;

pub type Result<T> = result::Result<T, failure::Error>;
