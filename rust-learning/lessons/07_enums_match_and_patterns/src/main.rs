enum ReadingState {
    Measured { millidegrees: i32 },
    Missing,
    Invalid(String),
}

fn classify(state: &ReadingState) -> &'static str {
    match state {
        ReadingState::Measured { millidegrees } if *millidegrees >= 60_000 => "notice",
        ReadingState::Measured { .. } => "below threshold",
        ReadingState::Missing => "missing",
        ReadingState::Invalid(_) => "invalid",
    }
}

fn print_measured(state: &ReadingState) {
    let ReadingState::Measured { millidegrees } = state else {
        println!("no measured value");
        return;
    };
    println!("measured={millidegrees}");
}

fn main() {
    // Fixtures: there is no sensor or parser in this program.
    let states = [
        ReadingState::Measured {
            millidegrees: 46_700,
        },
        ReadingState::Measured {
            millidegrees: 60_000,
        },
        ReadingState::Missing,
        ReadingState::Invalid(String::from("not a number")),
    ];

    for state in &states {
        println!("state={}", classify(state));
        if let ReadingState::Invalid(reason) = state {
            println!("reason={reason}");
        }
    }

    print_measured(&states[0]);
    print_measured(&states[2]);
}

#[cfg(test)]
mod tests {
    use super::{ReadingState, classify};

    #[test]
    fn below_threshold_is_classified() {
        assert_eq!(
            classify(&ReadingState::Measured {
                millidegrees: 59_999,
            }),
            "below threshold"
        );
    }

    #[test]
    fn equality_and_above_need_notice() {
        assert_eq!(
            classify(&ReadingState::Measured {
                millidegrees: 60_000,
            }),
            "notice"
        );
        assert_eq!(
            classify(&ReadingState::Measured {
                millidegrees: 60_001,
            }),
            "notice"
        );
    }

    #[test]
    fn missing_differs_from_real_zero() {
        assert_eq!(classify(&ReadingState::Missing), "missing");
        assert_eq!(
            classify(&ReadingState::Measured { millidegrees: 0 }),
            "below threshold"
        );
    }

    #[test]
    fn matching_does_not_consume_invalid_text() {
        let state = ReadingState::Invalid(String::from("bad input"));
        assert_eq!(classify(&state), "invalid");
        let ReadingState::Invalid(reason) = &state else {
            panic!("fixture must be invalid");
        };
        assert_eq!(reason, "bad input");
        assert_eq!(classify(&state), "invalid");
    }

    #[test]
    fn range_and_alternative_patterns_select_values() {
        for (sample, expected) in [(0, 0), (9, 9), (10, 10), (11, 10), (12, -1)] {
            let bucket = match sample {
                value @ 0..=9 => value,
                10 | 11 => 10,
                _ => -1,
            };
            assert_eq!(bucket, expected);
        }
    }
}
