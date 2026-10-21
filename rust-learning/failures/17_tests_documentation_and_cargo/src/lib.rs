//! Parse fixture readings and account for measured, missing and invalid input.
//!
//! The implementation module is not part of the public API:
//! ```compile_fail
//! use pi_report::parsing::parse_reading;
//! ```

mod parsing;
pub use parsing::{ParseError, parse_reading};

/// Counts every input; matching is a subset of measured readings.
#[derive(Debug, Default, PartialEq, Eq)]
pub struct Summary {
    pub measured: usize,
    pub missing: usize,
    pub invalid: usize,
    pub matching: usize,
}

/// Apply the predicate only to valid measured fixture values.
///
/// ```
/// use pi_report::analyze;
/// let summary = analyze(["0", "", "bad"], |value| value >= 0);
/// assert_eq!((summary.measured, summary.missing, summary.invalid), (1, 1, 1));
/// assert_eq!(summary.matching, 1);
/// ```
pub fn analyze<I, F>(inputs: I, mut predicate: F) -> Summary
where
    I: IntoIterator,
    I::Item: AsRef<str>,
    F: FnMut(i32) -> bool,
{
    let mut summary = Summary::default();
    for result in inputs
        .into_iter()
        .map(|input| parse_reading(input.as_ref()))
    {
        match result {
            Ok(Some(value)) => {
                summary.measured += 1;
                if predicate(value) {
                    summary.matching += 1;
                }
            }
            Ok(None) => summary.missing += 1,
            Err(_error) => summary.invalid += 1,
        }
    }
    summary
}

#[cfg(test)]
mod tests {
    use super::{Summary, analyze};

    #[test]
    fn report_accounts_for_every_record() {
        let inputs = ["46700", "60000", "", "bad", "125001", "0"];
        let summary = analyze(inputs, |value| value >= 60_000);
        assert_eq!(
            summary,
            Summary {
                measured: 3,
                missing: 1,
                invalid: 2,
                matching: 1
            }
        );
        assert_eq!(
            summary.measured + summary.missing + summary.invalid,
            inputs.len()
        );
    }

    #[test]
    fn predicate_runs_only_for_measured_values() {
        let mut calls = 0;
        let summary = analyze(["0", "", "bad", "60000"], |_| {
            calls += 1;
            true
        });
        assert_eq!(calls, 2);
        assert_eq!(summary.matching, 2);
    }

    #[test]
    fn owned_strings_and_empty_input_are_supported() {
        let inputs = vec![String::from("0"), String::from(" ")];
        assert_eq!(analyze(inputs, |_| true).measured, 1);
        assert_eq!(analyze(Vec::<String>::new(), |_| true), Summary::default());
    }
}
