use std::error::Error;
use std::fmt;
use std::num::ParseIntError;

/// An invalid integer or an integer outside this exercise's fixture range.
#[derive(Debug)]
pub enum ParseError {
    InvalidInteger {
        input: String,
        source: ParseIntError,
    },
    OutOfRange {
        value: i32,
    },
}

impl fmt::Display for ParseError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidInteger { input, .. } => {
                write!(formatter, "invalid integer {input:?}")
            }
            Self::OutOfRange { value } => {
                write!(formatter, "reading outside fixture range: {value} mC")
            }
        }
    }
}

impl Error for ParseError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::InvalidInteger { source, .. } => Some(source),
            Self::OutOfRange { .. } => None,
        }
    }
}

/// Parse signed millidegrees in the inclusive fixture range -40000..=125000.
/// Whitespace-only input is missing; zero is a measured value.
///
/// # Errors
/// Returns InvalidInteger for invalid or overflowing i32 text, and
/// OutOfRange for an integer outside the application fixture range.
///
/// # Examples
/// ```
/// use pi_report::parse_reading;
/// assert_eq!(parse_reading(" 0 ").expect("valid fixture"), Some(0));
/// assert_eq!(parse_reading(" ").expect("missing fixture"), None);
/// ```
pub fn parse_reading(input: &str) -> Result<Option<i32>, ParseError> {
    let text = input.trim();
    if text.is_empty() {
        return Ok(None);
    }
    let value = text
        .parse::<i32>()
        .map_err(|source| ParseError::InvalidInteger {
            input: text.to_owned(),
            source,
        })?;
    if !(-40_000..=125_000).contains(&value) {
        return Err(ParseError::OutOfRange { value });
    }
    Ok(Some(value))
}

#[cfg(test)]
mod tests {
    use super::{ParseError, parse_reading};

    #[test]
    fn whitespace_is_missing() {
        assert_eq!(parse_reading(" \t\n").expect("missing fixture"), None);
    }

    #[test]
    fn zero_remains_measured() {
        assert_eq!(parse_reading("0").expect("valid fixture"), Some(0));
    }

    #[test]
    fn inclusive_fixture_boundaries_are_accepted() {
        assert_eq!(
            parse_reading("-40000").expect("lower boundary"),
            Some(-40_000)
        );
        assert_eq!(
            parse_reading("125000").expect("upper boundary"),
            Some(125_000)
        );
    }

    #[test]
    fn invalid_text_retains_its_context() {
        let error = parse_reading(" bad ").expect_err("invalid fixture");
        assert!(matches!(error, ParseError::InvalidInteger { input, .. } if input == "bad"));
    }

    #[test]
    fn overflowing_integer_is_not_a_range_checked_value() {
        assert!(matches!(
            parse_reading("2147483648"),
            Err(ParseError::InvalidInteger { .. })
        ));
    }

    #[test]
    fn integers_just_outside_the_range_are_rejected() {
        assert!(matches!(
            parse_reading("-40001"),
            Err(ParseError::OutOfRange { value: -40_001 })
        ));
        assert!(matches!(
            parse_reading("125001"),
            Err(ParseError::OutOfRange { value: 125_001 })
        ));
    }
}
