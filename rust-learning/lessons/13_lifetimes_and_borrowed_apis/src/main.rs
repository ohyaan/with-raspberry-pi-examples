// "Longer" means UTF-8 byte length here, not characters or storage lifetime.
fn longer_label<'a>(left: &'a str, right: &'a str) -> &'a str {
    if left.len() >= right.len() {
        left
    } else {
        right
    }
}

// Only the first input can be returned: do not tie it to the second input.
fn first_label<'a>(left: &'a str, _right: &str) -> &'a str {
    left
}

fn byte_length(label: &str) -> usize {
    label.len()
}

struct DeviceView<'a> {
    label: &'a str,
}

impl<'a> DeviceView<'a> {
    fn new(label: &'a str) -> Self {
        Self { label }
    }

    // Copy the shared reference to the backing label, not to the view.
    fn label(&self) -> &'a str {
        self.label
    }
}

fn fixture_label() -> &'static str {
    "fixture"
}

fn main() {
    let primary = String::from("Pi 4B");
    let alternative = String::from("Pi 5");
    let selected = longer_label(&primary, &alternative);
    println!("selected={selected}, bytes={}", byte_length(selected));

    let retained;
    {
        let view = DeviceView::new(&primary);
        retained = view.label();
    } // The view ends, but primary still owns the referenced bytes.
    println!("retained={retained}");

    let independent;
    {
        let temporary = String::from("temporary alternative");
        independent = first_label(&primary, &temporary);
    }
    println!("independent={independent}");
    println!("static={}", fixture_label());
}

#[cfg(test)]
mod tests {
    use super::{DeviceView, byte_length, first_label, fixture_label, longer_label};

    #[test]
    fn selection_can_return_either_input() {
        assert_eq!(longer_label("Pi 4B", "Pi 5"), "Pi 4B");
        assert_eq!(longer_label("Pi", "Pi Zero 2 W"), "Pi Zero 2 W");
    }

    #[test]
    fn empty_labels_are_valid() {
        assert_eq!(longer_label("", ""), "");
        assert_eq!(longer_label("", "Pi"), "Pi");
    }

    #[test]
    fn equal_byte_lengths_choose_the_left_input() {
        let left = String::from("AB");
        let right = String::from("CD");
        assert!(std::ptr::eq(longer_label(&left, &right), left.as_str()));
    }

    #[test]
    fn unicode_selection_uses_bytes_not_characters() {
        assert_eq!(byte_length("π"), 2);
        assert_eq!(longer_label("π", "a"), "π");
    }

    #[test]
    fn view_may_end_before_the_borrowed_label() {
        let owner = String::from("Pi 4B");
        let saved;
        {
            let view = DeviceView::new(&owner);
            saved = view.label();
        }
        assert_eq!(saved, "Pi 4B");
    }

    #[test]
    fn first_input_does_not_borrow_the_second_after_return() {
        let owner = String::from("Pi 4B");
        let saved;
        {
            let other = String::from("Pi Zero 2 W");
            saved = first_label(&owner, &other);
        }
        assert_eq!(saved, "Pi 4B");
    }

    #[test]
    fn literal_can_supply_a_static_reference() {
        let value: &'static str = fixture_label();
        assert_eq!(value, "fixture");
    }
}
