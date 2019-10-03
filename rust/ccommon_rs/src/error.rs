
use std::fmt::{self, Formatter, Display};
use std::error::Error as StdError;

/// Error codes that could be returned by ccommon functions.
#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub enum Error {
    Generic = -1,
    EAgain = -2,
    ERetry = -3,
    ENoMem = -4,
    EEmpty = -5,
    ERdHup = -6,
    EInval = -7,
    EOther = -8,
}

impl From<std::os::raw::c_int> for Error {
    fn from(val: std::os::raw::c_int) -> Self {
        match val {
            -1 => Error::Generic,
            -2 => Error::EAgain,
            -3 => Error::ERetry,
            -4 => Error::ENoMem,
            -5 => Error::EEmpty,
            -6 => Error::ERdHup,
            -7 => Error::EInval,
            _ => Error::EOther,
        }
    }
}

impl Display for Error {
    fn fmt(&self, fmt: &mut Formatter) -> fmt::Result {
        match self {
            Error::Generic => write!(fmt, "Generic Error"),
            Error::EAgain => write!(fmt, "EAGAIN"),
            Error::ERetry => write!(fmt, "ERETRY"),
            Error::ENoMem => write!(fmt, "ENOMEM"),
            Error::EEmpty => write!(fmt, "EEMPTY"),
            Error::ERdHup => write!(fmt, "ERDHUP"),
            Error::EInval => write!(fmt, "EINVAL"),
            Error::EOther => write!(fmt, "EOTHER")
        }
    }
}

impl StdError for Error {}
