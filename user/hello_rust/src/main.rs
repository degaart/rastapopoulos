#![no_std]
#![no_main]
use core::panic::PanicInfo;
use core::ffi::c_char;

unsafe extern "C" {
    fn sys_write(s: *const c_char, len: i32) -> i32;
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

#[unsafe(no_mangle)]
pub extern "C" fn main() -> i32 {
    unsafe {
        sys_write(c"Hello, from rust!\n".as_ptr(), 18);
    }
    0
}

