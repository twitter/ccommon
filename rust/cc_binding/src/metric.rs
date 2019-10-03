use std::mem::{align_of, size_of, MaybeUninit};
use std::os::raw::c_char;

use crate::metric_type_e;

#[repr(C)]
#[derive(Copy, Clone)]
pub struct metric {
    pub name: *mut c_char,
    pub desc: *mut c_char,
    pub type_: metric_type_e,
    pub data: metric_anon_union,
}

#[derive(Copy, Clone)]
#[repr(C)]
pub union metric_anon_union {
    bytes: [MaybeUninit<u8>; size_of::<u64>()],
    _align: u64,
}

impl metric_anon_union {
    pub unsafe fn as_ptr<T>(&self) -> *const T {
        assert!(align_of::<T>() <= align_of::<Self>());
        assert!(size_of::<T>() <= size_of::<Self>());

        (&self.bytes).align_to().1.as_ptr()
    }
    pub unsafe fn as_mut_ptr<T>(&mut self) -> *mut T {
        assert!(align_of::<T>() <= align_of::<Self>());
        assert!(size_of::<T>() <= size_of::<Self>());

        (&mut self.bytes).align_to_mut().1.as_mut_ptr()
    }

    fn uninit() -> Self {
        Self {
            bytes: [MaybeUninit::uninit(); size_of::<u64>()],
        }
    }

    pub fn counter(val: u64) -> Self {
        unsafe {
            let mut x = Self::uninit();
            std::ptr::write(x.as_mut_ptr(), val);
            x
        }
    }

    pub fn gauge(val: i64) -> Self {
        unsafe {
            let mut x = Self::uninit();
            std::ptr::write(x.as_mut_ptr(), val);
            x
        }
    }

    pub fn fpn(val: f64) -> Self {
        unsafe {
            let mut x = Self::uninit();
            std::ptr::write(x.as_mut_ptr(), val);
            x
        }
    }
}

#[test]
fn metric_anon_union_aligment_correct() {
    assert_eq!(
        std::mem::align_of::<metric_anon_union>(),
        std::mem::align_of::<u64>()
    );

    assert_eq!(
        std::mem::align_of::<metric_anon_union>(),
        std::mem::align_of::<i64>()
    );

    assert_eq!(
        std::mem::align_of::<metric_anon_union>(),
        std::mem::align_of::<f64>()
    );
}
