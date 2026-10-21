use std::cell::Cell;

struct Sensor {
    reading: i32,
    hits: Cell<u32>,
}

impl Sensor {
    fn new(reading: i32) -> Self {
        Self {
            reading,
            hits: Cell::new(0),
        }
    }

    fn inspect(&self) -> i32 {
        self.hits.set(self.hits.get() + 1);
        self.reading
    }

    fn set(&mut self, reading: i32) {
        self.reading = reading;
    }
}

fn label(owner: &str) -> &str {
    owner
}

fn main() {
    let mut sensor = Sensor::new(46_700);
    let before = sensor.inspect();
    sensor.set(-500);
    let after = sensor.inspect();
    assert_eq!((before, after), (46_700, -500));
    println!("before={before}, after={after}");
    println!("inspections={}", sensor.hits.get());

    let owner = String::from("Pi 4B");
    let borrowed = label(&owner);
    println!("label bytes={}", borrowed.len());

    let mut plain = 0;
    let observed = &plain;
    assert_eq!(*observed, 0);
    // The shared borrow's final use is above, not after this assignment.
    plain = 1;
    println!("after borrow={plain}");
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn inspection_changes_only_the_counter() {
        let sensor = Sensor::new(46_700);
        let first = &sensor;
        let second = &sensor;
        assert_eq!(first.inspect(), 46_700);
        assert_eq!(second.inspect(), 46_700);
        assert_eq!(sensor.reading, 46_700);
        assert_eq!(sensor.hits.get(), 2);
    }

    #[test]
    fn updates_preserve_zero_and_negative_values() {
        let mut sensor = Sensor::new(0);
        assert_eq!(sensor.inspect(), 0);
        sensor.set(-500);
        assert_eq!(sensor.inspect(), -500);
    }

    #[test]
    fn returned_view_borrows_existing_storage() {
        let owner = String::from("Pi 4B");
        let borrowed = label(&owner);
        assert_eq!(borrowed, "Pi 4B");
        assert_eq!(borrowed.as_ptr(), owner.as_ptr());
    }

    #[test]
    fn empty_and_unicode_labels_are_byte_sequences() {
        assert_eq!(label("").len(), 0);
        assert_eq!(label("π").len(), 2);
        assert_eq!(label("π").chars().count(), 1);
    }
}
