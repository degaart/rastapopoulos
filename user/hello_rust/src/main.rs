#![no_std]
#![no_main]
use core::arch::asm;
use core::ffi::c_char;
use core::panic::PanicInfo;

unsafe fn syscall(
    number: u32,
    arg1: usize,
    arg2: usize,
    arg3: usize,
    arg4: usize,
    arg5: usize,
) -> i32 {
    let saved_args = [arg1, arg4, arg5];
    let result: i32;
    unsafe {
        asm!(
            "push ebx",
            "push esi",
            "mov ebx, [edi]",
            "mov esi, [edi + 4]",
            "mov edi, [edi + 8]",
            "int 0x80",
            "pop esi",
            "pop ebx",
            inlateout("eax") number => result,
            inlateout("ecx") arg2 => _,
            inlateout("edx") arg3 => _,
            inlateout("edi") saved_args.as_ptr() => _,
        );
    }
    result
}

unsafe fn strlen(string: *const c_char) -> usize {
    let mut length = 0;
    unsafe {
        while *string.add(length) != 0 {
            length += 1;
        }
    }
    length
}

unsafe fn sys_write(string: *const c_char) -> i32 {
    unsafe { syscall(6, string as usize, strlen(string), 0, 0, 0) }
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

#[unsafe(no_mangle)]
pub extern "C" fn main() -> i32 {
    unsafe {
        sys_write(c"Hello, from rust\n".as_ptr());
    }
    0
}
