fn celsius(millidegrees: i32) -> f64 {
    f64::from(millidegrees) / 1_000.0
}

fn main() {
    // An array of fixtures, not live sensor data.
    let readings: [i32; 3] = [46_700, 60_000, -500];
    let report: (usize, f64) = (readings.len(), celsius(readings[0]));
    let (count, first) = report;
    let label = {
        let nonnegative = readings[0] >= 0;
        if nonnegative {
            "nonnegative"
        } else {
            "negative"
        }
    };

    println!("count={count}, first={first:.1} C, {label}");
    println!("integer division={}", readings[0] / 1_000);
    // An explicit library operation, independent of profile overflow settings.
    println!("checked overflow={:?}", 255_u8.checked_add(1));
}

#[cfg(test)]
mod tests {
    use super::celsius;

    #[test]
    fn zero_converts_to_zero() {
        assert_eq!(celsius(0), 0.0);
    }

    #[test]
    fn negative_fraction_is_preserved() {
        assert!((celsius(-500) + 0.5).abs() < 1e-9);
    }

    #[test]
    fn overflow_is_reported() {
        assert_eq!(255_u8.checked_add(1), None);
    }

    #[test]
    fn division_truncates_toward_zero() {
        assert_eq!(-500_i32 / 1_000, 0);
        assert_eq!(-7_i32 % 3, -1);
    }

    #[test]
    fn explicit_overflow_policies_differ() {
        assert_eq!(255_u8.wrapping_add(1), 0);
        assert_eq!(255_u8.saturating_add(1), 255);
    }

    #[test]
    fn narrowing_cast_discards_high_bits() {
        assert_eq!(300_u16 as u8, 44);
    }
}
