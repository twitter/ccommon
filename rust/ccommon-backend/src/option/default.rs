use cc_binding::{
    _cc_alloc, option, OPTION_TYPE_BOOL, OPTION_TYPE_FPN, OPTION_TYPE_STR, OPTION_TYPE_UINT,
};

use std::ffi::CStr;
use std::fmt;

macro_rules! c_str {
    ($s:expr) => {
        concat!($s, "\0").as_ptr() as *const i8
    };
}

#[derive(Debug)]
pub struct OutOfMemoryError(());

impl fmt::Display for OutOfMemoryError {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "Out of memory")
    }
}

impl std::error::Error for OutOfMemoryError {}

pub unsafe fn option_default(opt: &mut option) -> Result<(), OutOfMemoryError> {
    use std::mem::MaybeUninit;

    opt.set = true;

    match opt.type_ {
        OPTION_TYPE_BOOL => opt.val.vbool = opt.default_val.vbool,
        OPTION_TYPE_UINT => opt.val.vuint = opt.default_val.vuint,
        OPTION_TYPE_FPN => opt.val.vfpn = opt.default_val.vfpn,
        OPTION_TYPE_STR => {
            let default = opt.default_val.vstr;

            if default.is_null() {
                opt.val.vstr = default;
            } else {
                let s = CStr::from_ptr(default);
                let bytes = s.to_bytes_with_nul();
                let mem = _cc_alloc(
                    bytes.len(),
                    c_str!(module_path!()),
                    line!() as std::os::raw::c_int,
                ) as *mut MaybeUninit<u8>;

                if mem.is_null() {
                    return Err(OutOfMemoryError(()));
                }

                std::ptr::copy_nonoverlapping(bytes.as_ptr(), mem as *mut u8, bytes.len());

                opt.val.vstr = mem as *mut libc::c_char
            }
        }
        _ => opt.val = opt.default_val,
    };

    Ok(())
}

pub unsafe fn option_load_default(options: &mut [option]) -> Result<(), OutOfMemoryError> {
    for opt in options.iter_mut() {
        option_default(opt)?;
    }

    Ok(())
}
