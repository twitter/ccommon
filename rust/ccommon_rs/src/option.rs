// ccommon - a cache common library.
// Copyright (C) 2019 Twitter, Inc.
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

//! Types and methods for dealing with ccommon options.

use std::ffi::CStr;
use std::fmt;

use cc_binding::{
    option, option_describe_all, option_free, option_load_default, option_load_file,
    option_print_all, option_val_u, OPTION_TYPE_BOOL, OPTION_TYPE_FPN, OPTION_TYPE_STR,
    OPTION_TYPE_UINT,
};

// Sealed trait to prevent SingleOption from ever being implemented
// from outside of this crate.
mod private {
    pub trait Sealed {}
}

use self::private::Sealed;

/// A single option.
///
/// This trait is sealed and cannot be implemented outside
/// of ccommon_rs.
pub unsafe trait SingleOption: self::private::Sealed {
    /// The type of the value contained within this option.
    type Value;

    /// Create an option with the given description, name,
    /// and default value.
    ///
    /// Normally, this should only be called by the derive
    /// macro for `Options`.
    fn new(default: Self::Value, name: &'static CStr, desc: &'static CStr) -> Self;

    /// Create an option with the given description, name,
    /// and `Default::default()` as the default value.
    ///
    /// The only exception is that [`Str`](crate::option::Str)
    /// uses `std::ptr::null_mut()` as it's default value since
    /// pointers do not implement `Default`.
    ///
    /// Normally, this should only be called by the derive macro
    /// for `Options`.
    fn defaulted(name: &'static CStr, desc: &'static CStr) -> Self;

    /// The name of this option
    fn name(&self) -> &'static CStr;
    /// A C string describing this option
    fn desc(&self) -> &'static CStr;
    /// The current value of this option
    fn value(&self) -> Self::Value;
    /// The default value for this option
    fn default(&self) -> Self::Value;
    /// Whether the option has been set externally
    fn is_set(&self) -> bool;

    /// Overwrite the current value for the option.
    ///
    /// This will always set `is_set` to true.
    fn set_value(&mut self, val: Self::Value);
}

/// A type that can be safely viewed as a contiguous
/// array of [`option`s][0].
///
/// See [`OptionsExt`] for more useful functions on
/// `Options`.
///
/// This trait should normally only be implemented through
/// `#[derive(Options)]`. However, it must be implemented
/// manually for C types which have been bound using bindgen.
///
/// [0]: ../../cc_binding/struct.option.html
pub unsafe trait Options: Sized {
    fn new() -> Self;
}

pub trait OptionExt: Options {
    /// The number of options in this object when it
    /// is interpreted as an array.
    ///
    /// # Panics
    /// Panics if the size of this type is not a multiple
    /// of thie size of `option`.
    fn num_options() -> usize {
        use std::mem::size_of;

        // If this assert fails then there was no way that
        // options upholds it's safety requirements so it's
        // better to fail here.
        assert!(size_of::<Self>() % size_of::<option>() == 0);

        // If this assert fails then we'll pass an invalid
        // size to several ccommon methods.
        assert!(size_of::<Self>() / size_of::<option>() < std::u32::MAX as usize);

        size_of::<Self>() / size_of::<option>()
    }

    /// Get `self` as a const pointer to an array of `option`s.
    ///
    /// # Panics
    /// Panics if the size of this type is not a multiple
    /// of thie size of `option`.
    fn as_ptr(&self) -> *const option {
        self as *const _ as *const option
    }

    /// Get `self` as a mutable pointer to an array of `option`s.
    ///
    /// # Panics
    /// Panics if the size of this type is not a multiple
    /// of thie size of `option`.
    fn as_mut_ptr(&mut self) -> *mut option {
        self as *mut _ as *mut option
    }

    /// Print a description of all options in the current object
    /// given using the default value, name, and description.
    ///
    /// Internally this calls out to `option_describe_all`.
    fn describe_all(&self) {
        unsafe {
            option_describe_all(
                // Note: ccommon uses a mutable pointer but it
                //       should really be a const pointer.
                self.as_ptr() as *mut _,
                Self::num_options() as u32,
            )
        }
    }

    /// Print out the values of all options.
    ///
    /// Internally this calls out to `option_print_all`.
    fn print_all(&self) {
        unsafe {
            option_print_all(
                // Note: ccommon uses a mutable pointer but it
                //       should really be a const pointer.
                self.as_ptr() as *mut _,
                Self::num_options() as u32,
            )
        }
    }

    /// Load default values for all options.
    ///
    /// Internally this calls `option_load_default`
    fn load_default(&mut self) -> Result<(), crate::Error> {
        let status = unsafe { option_load_default(self.as_mut_ptr(), Self::num_options() as u32) };

        if status == 0 {
            Ok(())
        } else {
            Err(status.into())
        }
    }

    /// Load options from a file in `.ini` format.
    ///
    /// Internally this calls `option_load_file`.
    fn load_from_libc_file(&mut self, file: *mut libc::FILE) -> Result<(), crate::Error> {
        let status =
            unsafe { option_load_file(file, self.as_mut_ptr(), Self::num_options() as u32) };

        if status == 0 {
            Ok(())
        } else {
            Err(status.into())
        }
    }
}

impl<T: Options> OptionExt for T {}

/// A boolean option.
#[derive(Copy, Clone)]
#[repr(transparent)]
pub struct Bool(option);

impl Sealed for Bool {}

unsafe impl SingleOption for Bool {
    type Value = bool;

    fn new(default: bool, name: &'static CStr, desc: &'static CStr) -> Self {
        Self(option {
            name: name.as_ptr() as *mut _,
            set: false,
            type_: OPTION_TYPE_BOOL,
            default_val: option_val_u { vbool: default },
            val: option_val_u { vbool: default },
            description: desc.as_ptr() as *mut _,
        })
    }
    fn defaulted(name: &'static CStr, desc: &'static CStr) -> Self {
        Self::new(Default::default(), name, desc)
    }

    fn name(&self) -> &'static CStr {
        unsafe { CStr::from_ptr(self.0.name) }
    }
    fn desc(&self) -> &'static CStr {
        unsafe { CStr::from_ptr(self.0.description) }
    }
    fn value(&self) -> Self::Value {
        unsafe { self.0.val.vbool }
    }
    fn default(&self) -> Self::Value {
        unsafe { self.0.default_val.vbool }
    }
    fn is_set(&self) -> bool {
        self.0.set
    }

    fn set_value(&mut self, val: Self::Value) {
        self.0.set = true;
        self.0.val = option_val_u { vbool: val }
    }
}

impl fmt::Debug for Bool {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_struct("Bool")
            .field("name", &self.name())
            .field("desc", &self.desc())
            .field("value", &self.value())
            .field("default", &self.default())
            .field("is_set", &self.is_set())
            .finish()
    }
}

/// An unsigned integer option.
#[derive(Copy, Clone)]
#[repr(transparent)]
pub struct UInt(option);

impl Sealed for UInt {}

unsafe impl SingleOption for UInt {
    type Value = cc_binding::uintmax_t;

    fn new(default: Self::Value, name: &'static CStr, desc: &'static CStr) -> Self {
        Self(option {
            name: name.as_ptr() as *mut _,
            set: false,
            type_: OPTION_TYPE_UINT,
            default_val: option_val_u { vuint: default },
            val: option_val_u { vuint: default },
            description: desc.as_ptr() as *mut _,
        })
    }
    fn defaulted(name: &'static CStr, desc: &'static CStr) -> Self {
        Self::new(Default::default(), name, desc)
    }

    fn name(&self) -> &'static CStr {
        unsafe { CStr::from_ptr(self.0.name) }
    }
    fn desc(&self) -> &'static CStr {
        unsafe { CStr::from_ptr(self.0.description) }
    }
    fn value(&self) -> Self::Value {
        unsafe { self.0.val.vuint }
    }
    fn default(&self) -> Self::Value {
        unsafe { self.0.default_val.vuint }
    }
    fn is_set(&self) -> bool {
        self.0.set
    }

    fn set_value(&mut self, val: Self::Value) {
        self.0.set = true;
        self.0.val = option_val_u { vuint: val }
    }
}

impl fmt::Debug for UInt {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_struct("UInt")
            .field("name", &self.name())
            .field("desc", &self.desc())
            .field("value", &self.value())
            .field("default", &self.default())
            .field("is_set", &self.is_set())
            .finish()
    }
}

/// A floating-point option.
#[derive(Copy, Clone)]
#[repr(transparent)]
pub struct Float(option);

impl Sealed for Float {}

unsafe impl SingleOption for Float {
    type Value = f64;

    fn new(default: Self::Value, name: &'static CStr, desc: &'static CStr) -> Self {
        Self(option {
            name: name.as_ptr() as *mut _,
            set: false,
            type_: OPTION_TYPE_FPN,
            default_val: option_val_u { vfpn: default },
            val: option_val_u { vfpn: default },
            description: desc.as_ptr() as *mut _,
        })
    }
    fn defaulted(name: &'static CStr, desc: &'static CStr) -> Self {
        Self::new(Default::default(), name, desc)
    }

    fn name(&self) -> &'static CStr {
        unsafe { CStr::from_ptr(self.0.name) }
    }
    fn desc(&self) -> &'static CStr {
        unsafe { CStr::from_ptr(self.0.description) }
    }
    fn value(&self) -> Self::Value {
        unsafe { self.0.val.vfpn }
    }
    fn default(&self) -> Self::Value {
        unsafe { self.0.default_val.vfpn }
    }
    fn is_set(&self) -> bool {
        self.0.set
    }

    fn set_value(&mut self, val: Self::Value) {
        self.0.set = true;
        self.0.val = option_val_u { vfpn: val }
    }
}

impl fmt::Debug for Float {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_struct("Float")
            .field("name", &self.name())
            .field("desc", &self.desc())
            .field("value", &self.value())
            .field("default", &self.default())
            .field("is_set", &self.is_set())
            .finish()
    }
}

/// A string option.
///
/// Note that this type wraps a C string.
#[repr(transparent)]
pub struct Str(option);

impl Sealed for Str {}

unsafe impl SingleOption for Str {
    type Value = *mut std::os::raw::c_char;

    fn new(default: Self::Value, name: &'static CStr, desc: &'static CStr) -> Self {
        Self(option {
            name: name.as_ptr() as *mut _,
            set: false,
            type_: OPTION_TYPE_STR,
            default_val: option_val_u { vstr: default },
            val: option_val_u { vstr: default },
            description: desc.as_ptr() as *mut _,
        })
    }
    fn defaulted(name: &'static CStr, desc: &'static CStr) -> Self {
        Self::new(std::ptr::null_mut(), name, desc)
    }

    fn name(&self) -> &'static CStr {
        unsafe { CStr::from_ptr(self.0.name) }
    }
    fn desc(&self) -> &'static CStr {
        unsafe { CStr::from_ptr(self.0.description) }
    }
    fn value(&self) -> Self::Value {
        unsafe { self.0.val.vstr }
    }
    fn default(&self) -> Self::Value {
        unsafe { self.0.default_val.vstr }
    }
    fn is_set(&self) -> bool {
        self.0.set
    }

    fn set_value(&mut self, val: Self::Value) {
        self.0.set = true;
        self.0.val = option_val_u { vstr: val }
    }
}

// TODO(sean): Debug print the string pointer
impl fmt::Debug for Str {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_struct("Str")
            .field("name", &self.name())
            .field("desc", &self.desc())
            .field("value", &self.value())
            .field("default", &self.default())
            .field("is_set", &self.is_set())
            .finish()
    }
}

impl Drop for Str {
    fn drop(&mut self) {
        unsafe { option_free(self as *mut _ as *mut option, 1) }
    }
}

/// Implementations of Options for cc_bindings types
mod impls {
    use cc_binding::*;
    use super::Options;

    macro_rules! c_str {
        ($s:expr) => {
            concat!($s, "\0").as_bytes().as_ptr() as *const i8 as *mut _
        };
    }

    macro_rules! initialize_option_value {
        (OPTION_TYPE_BOOL, $default:expr) => {
            option_val_u { vbool: $default }
        };
        (OPTION_TYPE_UINT, $default:expr) => {
            option_val_u { vuint: $default.into() }
        };
        (OPTION_TYPE_FPN, $default:expr) => {
            option_val_u { vfpn: $default }
        };
        (OPTION_TYPE_STR, $default:expr) => {
            option_val_u { vstr: $default }
        };
    }

    macro_rules! impl_options {
        {
            $(
                impl Options for $metrics_ty:ty {
                    $(
                        ACTION( $field:ident, $type:ident, $default:expr, $desc:expr )
                    )*
                }
            )*
        } => {
            $(
                unsafe impl Options for $metrics_ty {
                    fn new() -> Self {
                        Self {
                            $(
                                $field: option {
                                    name: c_str!($desc),
                                    set: false,
                                    type_: $type,
                                    default_val: initialize_option_value!($type, $default),
                                    val: initialize_option_value!($type, $default),
                                    description: c_str!($desc)
                                },
                            )*
                        }
                    }
                }
            )*
        }
    }

    impl_options! {
        impl Options for buf_options_st {
            ACTION( buf_init_size,  OPTION_TYPE_UINT,   BUF_DEFAULT_SIZE,   "init buf size incl header" )
            ACTION( buf_poolsize,   OPTION_TYPE_UINT,   BUF_POOLSIZE,       "buf pool size"             )
        }

        impl Options for dbuf_options_st {
            ACTION( dbuf_max_power,      OPTION_TYPE_UINT,  DBUF_DEFAULT_MAX,   "max number of doubles")
        }

        impl Options for pipe_options_st {
            ACTION( pipe_poolsize,      OPTION_TYPE_UINT,   PIPE_POOLSIZE,  "pipe conn pool size" )
        }

        impl Options for tcp_options_st {
            ACTION( tcp_backlog,    OPTION_TYPE_UINT,   TCP_BACKLOG,    "tcp conn backlog limit" )
            ACTION( tcp_poolsize,   OPTION_TYPE_UINT,   TCP_POOLSIZE,   "tcp conn pool size"     )
        }

        impl Options for sockio_options_st {
            ACTION( buf_sock_poolsize,  OPTION_TYPE_UINT,   BUFSOCK_POOLSIZE,   "buf_sock limit" )
        }

        impl Options for array_options_st {
            ACTION( array_nelem_delta,  OPTION_TYPE_UINT,   NELEM_DELTA,      "max nelem delta during expansion" )
        }

        impl Options for debug_options_st {
            ACTION( debug_log_level, OPTION_TYPE_UINT, DEBUG_LOG_LEVEL,  "debug log level"     )
            ACTION( debug_log_file,  OPTION_TYPE_STR,  DEBUG_LOG_FILE,   "debug log file"      )
            ACTION( debug_log_nbuf,  OPTION_TYPE_UINT, DEBUG_LOG_NBUF,   "debug log buf size"  )
        }

        impl Options for stats_log_options_st {
            ACTION( stats_log_file, OPTION_TYPE_STR,  std::ptr::null_mut(), "file storing stats"   )
            ACTION( stats_log_nbuf, OPTION_TYPE_UINT, STATS_LOG_NBUF,       "stats log buf size"   )
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::Options;

    #[derive(Options)]
    #[repr(C)]
    struct TestOptions {
        #[desc = "The first test option"]
        opt1: Bool,

        #[desc = "The second test option"]
        #[default(5)]
        opt2: UInt,

        #[desc = "The third test option"]
        #[default(35.0)]
        opt3: Float,
    }

    #[test]
    fn test_option_properties() {
        assert_eq!(TestOptions::num_options(), 3);
    }

    #[test]
    fn test_option_defaults() {
        let options = TestOptions::new();
        let ptr =
            unsafe { std::slice::from_raw_parts(options.as_ptr(), TestOptions::num_options()) };

        unsafe {
            assert_eq!(ptr[0].default_val.vbool, false);
            assert_eq!(ptr[0].set, false);

            assert_eq!(ptr[1].default_val.vuint, 5);
            assert_eq!(ptr[1].set, false);

            assert_eq!(ptr[2].default_val.vfpn, 35.0);
            assert_eq!(ptr[2].set, false);
        }
    }

    #[test]
    fn option_size_sanity() {
        // Protect against a bad bindgen run
        assert!(std::mem::size_of::<option>() != 0);
    }
}
