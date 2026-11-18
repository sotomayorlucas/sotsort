use core::slice;
use std::sync::atomic::{AtomicU64, Ordering};
pub static RS_CMPS: AtomicU64 = AtomicU64::new(0);
#[no_mangle] pub extern "C" fn rs_cmps_take() -> u64 { RS_CMPS.swap(0, Ordering::Relaxed) }

macro_rules! mk { ($t:ty, $d:ident, $g:ident, $i:ident) => {
  #[no_mangle] pub unsafe extern "C" fn $d(p: *mut $t, n: usize) { slice::from_raw_parts_mut(p, n).sort(); }
  #[no_mangle] pub unsafe extern "C" fn $g(p: *mut $t, n: usize) { glidesort::sort(slice::from_raw_parts_mut(p, n)); }
  #[no_mangle] pub unsafe extern "C" fn $i(p: *mut $t, n: usize) { slice::from_raw_parts_mut(p, n).sort_unstable(); }
}}
mk!(u32, rs_drift_u32, rs_glide_u32, rs_ipn_u32);
mk!(u64, rs_drift_u64, rs_glide_u64, rs_ipn_u64);

// sort by external comparator (C callback): for counting comparisons / adversary / records
pub type CmpFn = unsafe extern "C" fn(*const u8, *const u8, *mut core::ffi::c_void) -> i32;
#[repr(C, align(8))] #[derive(Clone, Copy)] pub struct Rec<const N: usize>([u8; N]);
macro_rules! mkc { ($n:expr, $d:ident, $g:ident, $i:ident) => {
  #[no_mangle] pub unsafe extern "C" fn $d(p: *mut u8, n: usize, c: CmpFn, ctx: *mut core::ffi::c_void) {
    let s = slice::from_raw_parts_mut(p as *mut Rec<$n>, n);
    s.sort_by(|a, b| { let r = c(a.0.as_ptr(), b.0.as_ptr(), ctx); r.cmp(&0) }); }
  #[no_mangle] pub unsafe extern "C" fn $g(p: *mut u8, n: usize, c: CmpFn, ctx: *mut core::ffi::c_void) {
    let s = slice::from_raw_parts_mut(p as *mut Rec<$n>, n);
    glidesort::sort_by(s, |a, b| { let r = c(a.0.as_ptr(), b.0.as_ptr(), ctx); r.cmp(&0) }); }
  #[no_mangle] pub unsafe extern "C" fn $i(p: *mut u8, n: usize, c: CmpFn, ctx: *mut core::ffi::c_void) {
    let s = slice::from_raw_parts_mut(p as *mut Rec<$n>, n);
    s.sort_unstable_by(|a, b| { let r = c(a.0.as_ptr(), b.0.as_ptr(), ctx); r.cmp(&0) }); }
}}
mkc!(8, rs_drift_c8, rs_glide_c8, rs_ipn_c8);
mkc!(16, rs_drift_c16, rs_glide_c16, rs_ipn_c16);
mkc!(32, rs_drift_c32, rs_glide_c32, rs_ipn_c32);
mkc!(64, rs_drift_c64, rs_glide_c64, rs_ipn_c64);

// inline key comparisons: p8 = u64 with key in high 32 bits; r32 = 32-byte record, key = first u64
#[repr(C)] #[derive(Clone, Copy)] pub struct R32 { k: u64, p: [u64; 3] }
#[no_mangle] pub unsafe extern "C" fn rs_drift_p8(p: *mut u64, n: usize) { slice::from_raw_parts_mut(p, n).sort_by_key(|x| *x >> 32); }
#[no_mangle] pub unsafe extern "C" fn rs_glide_p8(p: *mut u64, n: usize) { glidesort::sort_by_key(slice::from_raw_parts_mut(p, n), |x| *x >> 32); }
#[no_mangle] pub unsafe extern "C" fn rs_ipn_p8(p: *mut u64, n: usize) { slice::from_raw_parts_mut(p, n).sort_unstable_by_key(|x| *x >> 32); }
#[no_mangle] pub unsafe extern "C" fn rs_drift_r32(p: *mut R32, n: usize) { slice::from_raw_parts_mut(p, n).sort_by_key(|x| x.k); }
#[no_mangle] pub unsafe extern "C" fn rs_glide_r32(p: *mut R32, n: usize) { glidesort::sort_by_key(slice::from_raw_parts_mut(p, n), |x| x.k); }
#[no_mangle] pub unsafe extern "C" fn rs_ipn_r32(p: *mut R32, n: usize) { slice::from_raw_parts_mut(p, n).sort_unstable_by_key(|x| x.k); }
