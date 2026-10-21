use std::cell::Cell;
use std::marker::PhantomPinned;
use std::pin::Pin;

struct Sample {
    reading: Cell<i32>,
    _pin: PhantomPinned,
}

impl Sample {
    fn new(value: i32) -> Self {
        Self {
            reading: Cell::new(value),
            _pin: PhantomPinned,
        }
    }

    fn reading(self: Pin<&Self>) -> i32 {
        self.get_ref().reading.get()
    }

    fn set_reading(self: Pin<&Self>, value: i32) {
        self.get_ref().reading.set(value);
    }
}

fn address(owner: &Pin<Box<Sample>>) -> *const Sample {
    owner.as_ref().get_ref() as *const Sample
}

fn main() {
    let owner = Box::pin(Sample::new(46700));
    let before = address(&owner);
    let transferred = owner;
    assert_eq!(before, address(&transferred));
    println!("owner transfer={}", before == address(&transferred));

    let mut records = vec![transferred];
    let old_capacity = records.capacity();
    // reserve takes an additional count relative to length, not capacity.
    records.reserve(old_capacity + 1);
    assert!(records.capacity() > old_capacity);
    assert_eq!(before, address(&records[0]));
    println!("collection growth={}", before == address(&records[0]));
    assert_eq!(records[0].as_ref().reading(), 46700);
    println!("reading={}", records[0].as_ref().reading());
    records[0].as_ref().set_reading(0);
    assert_eq!(records[0].as_ref().reading(), 0);
    assert_eq!(before, address(&records[0]));
    println!("updated={}", records[0].as_ref().reading());

    let ordinary = Box::pin(String::from("Pi 4B"));
    let boxed = Pin::into_inner(ordinary);
    let extracted = *boxed;
    assert_eq!(extracted, "Pi 4B");
    println!("ordinary bytes={}", extracted.len());
    // The raw pointer was only compared, never dereferenced or used after drop.
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn moving_the_owner_keeps_the_pointee() {
        let owner = Box::pin(Sample::new(-500));
        let before = address(&owner);
        let after = owner;
        assert_eq!(before, address(&after));
        assert_eq!(after.as_ref().reading(), -500);
    }

    #[test]
    fn owner_collection_can_grow_without_relocating_records() {
        let mut owners = vec![Box::pin(Sample::new(46700))];
        let before = address(&owners[0]);
        let capacity = owners.capacity();
        owners.reserve(capacity + 1);
        assert!(owners.capacity() > capacity);
        assert_eq!(before, address(&owners[0]));
    }

    #[test]
    fn controlled_mutation_preserves_identity_and_zero() {
        let owner = Box::pin(Sample::new(-500));
        let before = address(&owner);
        owner.as_ref().set_reading(0);
        assert_eq!(owner.as_ref().reading(), 0);
        assert_eq!(before, address(&owner));
    }

    #[test]
    fn unpin_string_can_be_extracted() {
        let owner = Box::pin(String::from("Pi 4B"));
        let extracted = *Pin::into_inner(owner);
        assert_eq!(extracted, "Pi 4B");
    }

    #[test]
    fn phantom_pinned_value_can_move_before_pinning() {
        fn transfer(value: Sample) -> Sample {
            value
        }
        let value = transfer(Sample::new(0));
        assert_eq!(value.reading.get(), 0);
    }
}
