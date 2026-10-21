fn decorate(label: String) -> String {
    // This function owns label and returns ownership of the result.
    format!("device={label}")
}

fn main() {
    let reading: i32 = 46_700;
    let copied = reading;
    println!("readings: {reading}, {copied}");

    let original = String::from("Pi 4B");
    let duplicate = original.clone();
    let moved = original;
    // original is no longer usable here; moved owns that String.
    println!("labels: {moved}, {duplicate}");

    let decorated = decorate(moved);
    println!("{decorated}");
}

#[cfg(test)]
mod tests {
    use super::decorate;

    #[test]
    fn owned_label_becomes_a_report() {
        assert_eq!(decorate(String::from("Pi 4B")), "device=Pi 4B");
    }

    #[test]
    fn string_clone_can_be_changed_independently() {
        let original = String::from("Pi");
        let mut duplicate = original.clone();
        duplicate.push_str(" 4B");
        assert_eq!(original, "Pi");
        assert_eq!(duplicate, "Pi 4B");
    }

    #[test]
    fn empty_owned_label_is_valid_input() {
        assert_eq!(decorate(String::new()), "device=");
    }
}
