fn label_bytes(label: &str) -> usize {
    label.len()
}

fn add_suffix(label: &mut String) {
    label.push_str(" online");
}

fn first_reading(readings: &[i32]) -> i32 {
    // Empty input has the exercise fallback zero, not a measured temperature.
    if readings.is_empty() { 0 } else { readings[0] }
}

fn main() {
    let mut label = String::from("Pi 4B");
    let shared = &label;
    println!("label bytes={}", label_bytes(shared));
    // shared is not used again, allowing the exclusive borrow below.
    add_suffix(&mut label);
    println!("label={label}");

    let readings = [46_700, 60_000, 63_200];
    let tail = &readings[1..];
    println!("tail first={}", first_reading(tail));
}

#[cfg(test)]
mod tests {
    use super::{add_suffix, first_reading, label_bytes};

    #[test]
    fn borrowed_text_counts_bytes_not_characters() {
        assert_eq!(label_bytes("Pi"), 2);
        assert_eq!(label_bytes("π"), 2);
    }

    #[test]
    fn mutable_borrow_changes_the_callers_value() {
        let mut label = String::from("Pi");
        add_suffix(&mut label);
        assert_eq!(label, "Pi online");
    }

    #[test]
    fn empty_slice_uses_the_exercise_fallback() {
        assert_eq!(first_reading(&[]), 0);
        assert_eq!(first_reading(&[60_000]), 60_000);
    }

    #[test]
    fn empty_text_is_a_valid_view() {
        assert_eq!(label_bytes(""), 0);
    }

    #[test]
    fn mutable_slice_updates_the_original_array() {
        let mut readings = [46_700, 60_000];
        let view = &mut readings[1..];
        view[0] = 61_000;
        assert_eq!(readings[1], 61_000);
    }

    #[test]
    #[should_panic]
    fn splitting_a_unicode_scalar_panics() {
        let text = "π";
        let _invalid_view = &text[..1];
    }
}
