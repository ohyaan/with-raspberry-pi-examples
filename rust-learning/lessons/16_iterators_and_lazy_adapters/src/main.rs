use std::num::ParseIntError;

fn high_readings<'a>(readings: &'a [i32], threshold: i32) -> impl Iterator<Item = i32> + 'a {
    readings
        .iter()
        .copied()
        .filter(move |value| *value >= threshold)
}

fn parse_all(inputs: &[&str]) -> Result<Vec<i32>, ParseIntError> {
    inputs
        .iter()
        .map(|text| text.trim().parse::<i32>())
        .collect()
}

struct Countdown {
    remaining: u8,
}

impl Iterator for Countdown {
    type Item = u8;

    fn next(&mut self) -> Option<Self::Item> {
        if self.remaining == 0 {
            None
        } else {
            let value = self.remaining;
            self.remaining -= 1;
            Some(value)
        }
    }
}

fn main() {
    let mut readings = vec![46_700, 60_000, 0];
    let high: Vec<i32> = high_readings(&readings, 60_000).collect();
    println!("high={high:?}, original length={}", readings.len());

    for value in readings.iter_mut() {
        *value += 1;
    }
    println!("updated first={}", readings[0]);

    // The Vec is consumed here; the sum is widened for these fixtures.
    let total: i64 = readings.into_iter().map(i64::from).sum();
    println!("owned sum={total}");

    println!("strict={:?}", parse_all(&["46700", "0"]));
    println!(
        "invalid rejected={}",
        parse_all(&["46700", "bad", "0"]).is_err()
    );

    // A separate, explicit discard policy: invalid records are not reported.
    let valid_only: Vec<i32> = ["46700", "bad", "0"]
        .iter()
        .filter_map(|text| text.parse::<i32>().ok())
        .collect();
    println!("valid only={valid_only:?}");

    let countdown: Vec<u8> = Countdown { remaining: 3 }.collect();
    println!("countdown={countdown:?}");
}

#[cfg(test)]
mod tests {
    use super::{Countdown, high_readings, parse_all};

    #[test]
    fn filter_includes_boundary_and_preserves_zero() {
        let values = [-500, 0, 60_000];
        assert_eq!(
            high_readings(&values, 0).collect::<Vec<_>>(),
            vec![0, 60_000]
        );
        assert!(high_readings(&[], 0).collect::<Vec<_>>().is_empty());
    }

    #[test]
    fn borrowing_pipeline_leaves_the_owner_available() {
        let mut values = vec![0, 60_000];
        assert_eq!(high_readings(&values, 60_000).count(), 1);
        for value in values.iter_mut() {
            *value += 1;
        }
        assert_eq!(values, vec![1, 60_001]);
    }

    #[test]
    fn owned_items_can_include_non_copy_strings() {
        let labels = vec![String::from("Pi"), String::from("Pi 4B")];
        let lengths: Vec<usize> = labels.into_iter().map(|label| label.len()).collect();
        assert_eq!(lengths, vec![2, 5]);
    }

    #[test]
    fn unconsumed_map_never_runs_its_callback() {
        let mut visited = Vec::new();
        {
            let pipeline = [1, 2, 3].into_iter().map(|value| {
                visited.push(value);
                value + 1
            });
            drop(pipeline);
        }
        assert!(visited.is_empty());
    }

    #[test]
    fn take_requests_only_the_required_items() {
        let mut visited = Vec::new();
        let output: Vec<i32> = [1, 2, 3]
            .into_iter()
            .map(|value| {
                visited.push(value);
                value + 1
            })
            .take(1)
            .collect();
        assert_eq!(output, vec![2]);
        assert_eq!(visited, vec![1]);
    }

    #[test]
    fn result_collection_preserves_empty_and_valid_input() {
        assert_eq!(
            parse_all(&[]).expect("valid empty fixture"),
            Vec::<i32>::new()
        );
        assert_eq!(
            parse_all(&[" -500 ", "0"]).expect("valid fixture"),
            vec![-500, 0]
        );
        assert!(parse_all(&["bad"]).is_err());
        assert!(parse_all(&["2147483648"]).is_err());
    }

    #[test]
    fn result_collection_stops_at_the_first_error() {
        let mut visited = Vec::new();
        let result: Result<Vec<i32>, _> = ["1", "bad", "2"]
            .into_iter()
            .map(|text| {
                visited.push(text);
                text.parse::<i32>()
            })
            .collect();
        assert!(result.is_err());
        assert_eq!(visited, vec!["1", "bad"]);
    }

    #[test]
    fn custom_iterator_handles_zero_and_exhaustion() {
        let mut empty = Countdown { remaining: 0 };
        assert_eq!(empty.next(), None);
        let mut one = Countdown { remaining: 1 };
        assert_eq!(one.next(), Some(1));
        assert_eq!(one.next(), None);
        assert_eq!(one.next(), None);
    }
}
