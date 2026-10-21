use std::num::ParseIntError;

fn parse_reading(input: &str) -> Result<Option<i32>, ParseIntError> {
    let trimmed = input.trim();
    if trimmed.is_empty() {
        return Ok(None);
    }
    let value = trimmed.parse::<i32>()?;
    Ok(Some(value))
}

struct Summary {
    measured: usize,
    missing: usize,
    invalid: usize,
    notice: usize,
}

fn summarise(inputs: &[&str]) -> Summary {
    let mut summary = Summary {
        measured: 0,
        missing: 0,
        invalid: 0,
        notice: 0,
    };

    for input in inputs {
        match parse_reading(input) {
            Ok(Some(value)) => {
                summary.measured += 1;
                if value >= 60_000 {
                    summary.notice += 1;
                }
            }
            Ok(None) => summary.missing += 1,
            Err(_error) => summary.invalid += 1,
        }
    }
    summary
}

fn main() {
    // Fixtures, including a real zero and an out-of-range integer.
    let inputs = ["46700", "60000", "63200", "", "bad", "0", "2147483648"];
    let summary = summarise(&inputs);
    println!(
        "measured={}, missing={}, invalid={}, notice={}",
        summary.measured, summary.missing, summary.invalid, summary.notice
    );
    println!("zero={:?}", parse_reading("0"));
    println!("empty={:?}", parse_reading(""));
    println!("invalid rejected={}", parse_reading("bad").is_err());
}

#[cfg(test)]
mod tests {
    use super::{parse_reading, summarise};

    #[test]
    fn zero_and_negative_values_are_present() {
        assert_eq!(parse_reading("0").expect("valid fixture"), Some(0));
        assert_eq!(parse_reading(" -500 ").expect("valid fixture"), Some(-500));
    }

    #[test]
    fn whitespace_only_input_is_missing() {
        assert_eq!(parse_reading(" \n\t ").expect("missing fixture"), None);
    }

    #[test]
    fn invalid_text_is_an_error() {
        assert!(parse_reading("bad").is_err());
        assert!(parse_reading("46.7").is_err());
    }

    #[test]
    fn integers_outside_i32_are_errors() {
        assert!(parse_reading("2147483648").is_err());
        assert!(parse_reading("-2147483649").is_err());
    }

    #[test]
    fn every_fixture_is_accounted_for() {
        let inputs = ["46700", "60000", "63200", "", "bad", "0", "2147483648"];
        let summary = summarise(&inputs);
        assert_eq!(summary.measured, 4);
        assert_eq!(summary.missing, 1);
        assert_eq!(summary.invalid, 2);
        assert_eq!(summary.notice, 2);
        assert_eq!(
            summary.measured + summary.missing + summary.invalid,
            inputs.len()
        );
    }

    #[test]
    fn empty_collection_has_zero_counts() {
        let summary = summarise(&[]);
        assert_eq!(summary.measured + summary.missing + summary.invalid, 0);
        assert_eq!(summary.notice, 0);
    }

    #[test]
    fn notice_includes_the_threshold() {
        let summary = summarise(&["59999", "60000", "60001"]);
        assert_eq!(summary.measured, 3);
        assert_eq!(summary.notice, 2);
    }

    #[test]
    #[should_panic(expected = "intentionally invalid fixture")]
    fn expect_panics_when_the_assumption_is_false() {
        let _value = "bad".parse::<i32>().expect("intentionally invalid fixture");
    }
}
