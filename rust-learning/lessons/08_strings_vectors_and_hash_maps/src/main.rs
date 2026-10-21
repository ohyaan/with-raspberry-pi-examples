use std::collections::HashMap;

fn update_reading(readings: &mut Vec<i32>, millidegrees: i32) {
    readings.push(millidegrees);
}

fn main() {
    let mut label = String::from("Pi");
    label.push_str(" 4B");

    let mut readings: Vec<i32> = vec![46_700, 60_000];
    update_reading(&mut readings, 63_200);
    println!("{label}: {} readings", readings.len());
    println!("first={:?}", readings.first());
    println!("out-of-range={:?}", readings.get(99));

    let mut latest: HashMap<String, i32> = HashMap::new();
    let previous = latest.insert(label, readings[0]);
    // label has moved into the map. It is not used as an owned String again.
    println!("previous={previous:?}");

    // Choose an explicit reporting order, not the map's iteration order.
    for name in ["Pi 4B", "Pi 5"] {
        match latest.get(name) {
            Some(value) => println!("{name}={value}"),
            None => println!("{name}=missing"),
        }
    }

    let text = "π";
    println!(
        "Unicode fixture: bytes={}, scalars={}",
        text.len(),
        text.chars().count()
    );
}

#[cfg(test)]
mod tests {
    use super::{HashMap, update_reading};

    #[test]
    fn vector_grows_and_supports_checked_access() {
        let mut readings = Vec::new();
        assert_eq!(readings.first(), None);
        update_reading(&mut readings, 46_700);
        assert_eq!(readings.get(0), Some(&46_700));
        assert_eq!(readings.get(1), None);
    }

    #[test]
    fn borrowed_map_lookup_distinguishes_missing_from_zero() {
        let mut latest = HashMap::new();
        latest.insert(String::from("Pi"), 0);
        assert_eq!(latest.get("Pi"), Some(&0));
        assert_eq!(latest.get("absent"), None);
    }

    #[test]
    fn replacing_a_key_returns_the_previous_value() {
        let mut latest = HashMap::new();
        assert_eq!(latest.insert(String::from("Pi"), 46_700), None);
        assert_eq!(latest.insert(String::from("Pi"), 48_200), Some(46_700));
        assert_eq!(latest.len(), 1);
        assert_eq!(latest.remove("Pi"), Some(48_200));
    }

    #[test]
    fn unicode_text_has_valid_byte_boundaries() {
        let text = "π";
        assert_eq!(text.len(), 2);
        assert_eq!(text.chars().count(), 1);
        assert_eq!(text.get(..1), None);
        assert_eq!(text.get(..2), Some("π"));
    }

    #[test]
    fn combining_sequence_is_not_one_scalar() {
        let text = "e\u{301}";
        assert_eq!(text.chars().count(), 2);
        assert_eq!(text.len(), 3);
    }

    #[test]
    fn similar_display_does_not_normalise_map_keys() {
        let mut names = HashMap::new();
        names.insert(String::from("é"), 1);
        names.insert(String::from("e\u{301}"), 2);
        assert_eq!(names.len(), 2);
        assert_ne!("é", "e\u{301}");
    }

    #[test]
    fn entry_initialises_once_and_returns_mutable_access() {
        let mut counts = HashMap::new();
        *counts.entry(String::from("Pi")).or_insert(0) += 1;
        *counts.entry(String::from("Pi")).or_insert(99) += 1;
        assert_eq!(counts.get("Pi"), Some(&2));
    }

    #[test]
    #[should_panic]
    fn direct_vector_index_can_panic() {
        let readings = vec![46_700];
        let _missing = readings[99];
    }
}
