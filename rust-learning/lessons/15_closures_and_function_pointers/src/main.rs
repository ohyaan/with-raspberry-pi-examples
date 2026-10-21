fn count_matching<F>(readings: &[i32], predicate: F) -> usize
where
    F: Fn(i32) -> bool,
{
    let mut count = 0;
    for &reading in readings {
        if predicate(reading) {
            count += 1;
        }
    }
    count
}

fn apply_twice<F>(mut operation: F, start: i32) -> i32
where
    F: FnMut(i32) -> i32,
{
    let first = operation(start);
    operation(first)
}

fn run_once<F>(operation: F) -> String
where
    F: FnOnce() -> String,
{
    operation()
}

fn measure_twice<F>(operation: &F) -> (usize, usize)
where
    F: Fn() -> usize,
{
    (operation(), operation())
}

fn add_one(value: i32) -> i32 {
    value + 1
}

fn apply_pointer(operation: fn(i32) -> i32, value: i32) -> i32 {
    operation(value)
}

fn make_offset(offset: i32) -> impl Fn(i32) -> i32 {
    move |value| value + offset
}

// The callback must accept the local borrow created inside this function.
fn with_local_label<F>(callback: F) -> usize
where
    F: for<'a> Fn(&'a str) -> usize,
{
    let label = String::from("Pi 4B");
    callback(&label)
}

fn main() {
    let threshold = 60_000;
    let predicate = |reading| reading >= threshold;
    println!(
        "matching={}",
        count_matching(&[46_700, 60_000, 61_000], predicate)
    );

    let mut calls = 0;
    let mut update = |value| {
        calls += 1;
        value + calls
    };
    let result = apply_twice(&mut update, 0);
    println!("stateful={result}, calls={calls}");

    let owned = String::from("Pi 4B");
    let finish = move || owned;
    println!("once={}", run_once(finish));

    // Own the String, but only inspect it: this move closure implements Fn.
    let label = String::from("Pi 4B");
    let inspect = move || label.len();
    println!("move read-only={:?}", measure_twice(&inspect));

    println!(
        "pointers={},{}",
        apply_pointer(add_one, 5),
        apply_pointer(|value| value + 2, 5)
    );
    let offset = make_offset(2);
    println!("returned={}", offset(5));
    println!("local bytes={}", with_local_label(|text| text.len()));
}

#[cfg(test)]
mod tests {
    use super::{add_one, apply_pointer, apply_twice, count_matching};
    use super::{make_offset, measure_twice, run_once, with_local_label};

    #[test]
    fn predicate_captures_threshold_and_includes_the_boundary() {
        let threshold = 60_000;
        assert_eq!(count_matching(&[59_999, 60_000], |v| v >= threshold), 1);
        assert_eq!(count_matching(&[], |v| v >= threshold), 0);
    }

    #[test]
    fn mutable_capture_records_both_calls() {
        let mut calls = 0;
        let mut update = |value| {
            calls += 1;
            value + calls
        };
        assert_eq!(apply_twice(&mut update, 0), 3);
        assert_eq!(calls, 2);
    }

    #[test]
    fn once_can_transfer_a_non_copy_capture() {
        let label = String::from("Pi 4B");
        assert_eq!(run_once(move || label), "Pi 4B");
    }

    #[test]
    fn move_read_only_capture_still_satisfies_fn() {
        let label = String::from("Pi 4B");
        let inspect = move || label.len();
        assert_eq!(measure_twice(&inspect), (5, 5));
        assert_eq!(inspect(), 5);
    }

    #[test]
    fn pointer_accepts_function_and_non_capturing_closure() {
        assert_eq!(apply_pointer(add_one, 0), 1);
        assert_eq!(apply_pointer(|value| value + 2, -2), 0);
    }

    #[test]
    fn returned_closure_owns_its_offset() {
        let operation = make_offset(-500);
        assert_eq!(operation(500), 0);
        assert_eq!(operation(0), -500);
    }

    #[test]
    fn callback_accepts_local_borrow_without_retaining_it() {
        assert_eq!(with_local_label(|label| label.len()), 5);
        assert_eq!(with_local_label(str::len), 5);
    }
}
