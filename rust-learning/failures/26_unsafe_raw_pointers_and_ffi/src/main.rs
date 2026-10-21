#![deny(unsafe_op_in_unsafe_fn)]

unsafe fn read_one(pointer: *const i32) -> i32 {
    *pointer
}

fn main() {}
