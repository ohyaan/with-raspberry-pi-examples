#![deny(unsafe_op_in_unsafe_fn)]

use std::ffi::c_int;
use std::sync::mpsc;
use std::thread;

/// Read a single copied integer through a raw pointer.
///
/// # Safety
/// The pointer must be derived from storage containing a live, initialized,
/// aligned i32, readable for this call, with no conflicting mutation.
unsafe fn read_one(pointer: *const i32) -> i32 {
    // SAFETY: the caller must establish the documented pointer contract.
    unsafe { *pointer }
}

fn first_reading(values: &[i32]) -> Option<i32> {
    if values.is_empty() {
        return None;
    }
    // SAFETY: a nonempty shared i32 slice provides an initialized, aligned
    // first element; the borrow keeps its storage alive and prevents writes.
    Some(unsafe { read_one(values.as_ptr()) })
}

/// A type whose all-zero representation is a valid, initialized value.
///
/// # Safety
/// Implementers must guarantee that zero initialization produces a valid Self.
unsafe trait ZeroValid: Copy {}

// SAFETY: every i32 bit pattern is valid, including all-zero.
unsafe impl ZeroValid for i32 {}

fn zero_value<T: ZeroValid>() -> T {
    // SAFETY: ZeroValid's implementation contract guarantees validity.
    unsafe { std::mem::zeroed() }
}

#[repr(C)]
union Bits {
    signed: i32,
    unsigned: u32,
}

fn unsigned_bits(value: i32) -> u32 {
    let bits = Bits { signed: value };
    // SAFETY: i32/u32 have matching size/alignment and all bit patterns are
    // valid integers. The written field initializes the entire u32 storage.
    unsafe { bits.unsigned }
}

#[repr(C)]
struct WireReading {
    value: i32,
    present: u8,
}

mod exported {
    use std::ffi::c_int;

    // SAFETY: this uniquely named symbol is defined exactly once in this
    // teaching executable; its matching declaration uses the same C ABI.
    #[unsafe(no_mangle)]
    pub extern "C" fn rust_course_26_double(value: c_int) -> c_int {
        value.saturating_mul(2)
    }
}

unsafe extern "C" {
    #[link_name = "rust_course_26_double"]
    fn double_from_c_abi(value: c_int) -> c_int;
}

fn ffi_double(value: c_int) -> c_int {
    // SAFETY: the symbol above has this exact ABI/signature, accepts every
    // c_int, retains no pointers and neither panics nor unwinds.
    unsafe { double_from_c_abi(value) }
}

fn worker_report(input: &[i32]) -> Result<i64, &'static str> {
    thread::scope(|scope| {
        let (sender, receiver) = mpsc::sync_channel(1);
        let worker = scope.spawn(move || {
            let result = input.iter().try_fold(0_i64, |sum, &value| {
                if !(-100_000..=150_000).contains(&value) {
                    return Err("invalid fixture");
                }
                sum.checked_add(i64::from(value)).ok_or("sum overflow")
            });
            sender.send(result)
        });
        // Receive before joining: also works with a zero-capacity channel.
        let received = receiver.recv();
        let joined = worker.join();
        match joined {
            Ok(Ok(())) => {}
            Ok(Err(_)) => return Err("receiver disconnected"),
            Err(_) => return Err("worker panicked"),
        }
        received.map_err(|_| "no report")?
    })
}

fn main() {
    println!(
        "first={:?}, empty={:?}",
        first_reading(&[46_700]),
        first_reading(&[])
    );
    println!("zero={}, bits={}", zero_value::<i32>(), unsigned_bits(-1));
    println!("C ABI double={}", ffi_double(-500));
    println!(
        "wire size={}, align={}, present offset={}",
        std::mem::size_of::<WireReading>(),
        std::mem::align_of::<WireReading>(),
        std::mem::offset_of!(WireReading, present)
    );
    let input = [46_700, 0, -500];
    println!(
        "worker={:?}, retained={}",
        worker_report(&input),
        input.len()
    );
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn safe_pointer_wrapper_handles_empty_zero_and_negative() {
        assert_eq!(first_reading(&[]), None);
        assert_eq!(first_reading(&[0, 46_700]), Some(0));
        assert_eq!(first_reading(&[-500]), Some(-500));
    }

    #[test]
    fn pointer_wrapper_agrees_with_safe_slice_access() {
        for fixture in [vec![], vec![0], vec![-500, 46_700]] {
            assert_eq!(first_reading(&fixture), fixture.first().copied());
        }
    }

    #[test]
    fn only_promised_zero_representation_is_used() {
        assert_eq!(zero_value::<i32>(), 0);
    }

    #[test]
    fn union_integer_bits_agree_with_numeric_cast() {
        for value in [i32::MIN, -1, 0, i32::MAX] {
            assert_eq!(unsigned_bits(value), value as u32);
        }
    }

    #[test]
    fn c_abi_boundary_has_explicit_overflow_policy() {
        assert_eq!(ffi_double(0), 0);
        assert_eq!(ffi_double(-500), -1000);
        assert_eq!(ffi_double(c_int::MAX), c_int::MAX);
        assert_eq!(ffi_double(c_int::MIN), c_int::MIN);
    }

    #[test]
    fn c_layout_has_nonoverlapping_fields_and_tail_padding() {
        assert_eq!(std::mem::offset_of!(WireReading, value), 0);
        assert_eq!(std::mem::offset_of!(WireReading, present), 4);
        assert_eq!(std::mem::size_of::<WireReading>(), 8);
        assert_eq!(std::mem::align_of::<WireReading>(), 4);
    }

    #[test]
    fn worker_returns_sum_only_after_joining() {
        let input = [46_700, 0, -500];
        assert_eq!(worker_report(&input), Ok(46_200));
        assert_eq!(input, [46_700, 0, -500]);
    }

    #[test]
    fn worker_reports_invalid_input_instead_of_partial_success() {
        assert_eq!(worker_report(&[46_700, 150_001]), Err("invalid fixture"));
    }

    #[test]
    fn empty_worker_input_is_a_completed_zero_sum() {
        assert_eq!(worker_report(&[]), Ok(0));
    }
}
