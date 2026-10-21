struct Reading {
    label: String,
    millidegrees: i32,
}

impl Reading {
    // Associated function: no receiver, so call it as Reading::new(...).
    fn new(label: String, millidegrees: i32) -> Self {
        Self {
            label,
            millidegrees,
        }
    }

    fn celsius(&self) -> f64 {
        f64::from(self.millidegrees) / 1_000.0
    }

    fn set_millidegrees(&mut self, millidegrees: i32) {
        self.millidegrees = millidegrees;
    }

    fn into_label(self) -> String {
        self.label
    }
}

fn main() {
    let mut reading = Reading::new(String::from("Pi 4B"), 46_700);
    println!("{}: {:.1} C", reading.label, reading.celsius());

    reading.set_millidegrees(48_200);
    println!("updated: {:.1} C", reading.celsius());

    let label = reading.into_label();
    // reading was consumed; the returned label is now owned here.
    println!("owned label: {label}");
}

#[cfg(test)]
mod tests {
    use super::Reading;

    #[test]
    fn named_fields_preserve_the_input() {
        let reading = Reading::new(String::from("Pi"), -500);
        assert_eq!(reading.label, "Pi");
        assert!((reading.celsius() + 0.5).abs() < 1e-9);
    }

    #[test]
    fn mutable_method_updates_the_reading() {
        let mut reading = Reading::new(String::from("Pi"), 0);
        reading.set_millidegrees(1_000);
        assert_eq!(reading.celsius(), 1.0);
    }

    #[test]
    fn consuming_method_returns_owned_text() {
        let reading = Reading::new(String::from("Pi"), 0);
        assert_eq!(reading.into_label(), "Pi");
    }

    #[test]
    fn struct_literal_does_not_require_new() {
        let reading = Reading {
            label: String::from("fixture"),
            millidegrees: 0,
        };
        assert_eq!(reading.celsius(), 0.0);
    }
}
