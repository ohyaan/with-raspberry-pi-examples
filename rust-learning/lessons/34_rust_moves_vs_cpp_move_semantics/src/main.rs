fn extract(slot: &mut Option<Box<i32>>) -> Option<Box<i32>> {
    slot.take()
}

fn main() {
    let mut source = Box::new(46700);
    let destination = source;
    assert_eq!(*destination, 46700);
    println!("owned={destination}");
    // source cannot be read here, but assigning a new value reinitialises it.
    source = Box::new(0);
    println!("reused={source}");

    let mut pending = Some(Box::new(-500));
    let extracted = extract(&mut pending).expect("fixture is populated");
    assert!(pending.is_none());
    println!("slot empty={}, value={extracted}", pending.is_none());

    let label = String::from("Pi 4B");
    let repeated = move || label.len();
    let first = repeated();
    let second = repeated();
    assert_eq!((first, second), (5, 5));
    println!("repeated={first},{second}");

    let label = String::from("Pi 4B");
    let consume = move || label;
    let consumed = consume();
    assert_eq!(consumed, "Pi 4B");
    println!("consumed={}", consumed.len());

    let original = 7;
    let copied = original;
    assert_eq!((original, copied), (7, 7));
    println!("copied={original},{copied}");
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn extraction_empties_a_slot_without_losing_zero() {
        let mut slot = Some(Box::new(0));
        assert_eq!(extract(&mut slot).map(|value| *value), Some(0));
        assert!(slot.is_none());
        assert_eq!(extract(&mut slot), None);
    }

    #[test]
    fn a_slot_can_be_refilled() {
        let mut slot = Some(Box::new(-500));
        let retained = extract(&mut slot).unwrap();
        slot = Some(Box::new(46700));
        assert_eq!((*retained, *extract(&mut slot).unwrap()), (-500, 46700));
        assert!(slot.is_none());
    }

    #[test]
    fn move_capture_can_still_implement_fn() {
        fn twice<F: Fn() -> usize>(function: F) -> (usize, usize) {
            (function(), function())
        }
        let label = String::from("Pi 4B");
        assert_eq!(twice(move || label.len()), (5, 5));
    }

    #[test]
    fn explicit_clone_keeps_independent_string_values() {
        let mut original = String::from("Pi 4B");
        let retained = original.clone();
        original.clear();
        assert_eq!(retained, "Pi 4B");
        assert!(original.is_empty());
    }
}
