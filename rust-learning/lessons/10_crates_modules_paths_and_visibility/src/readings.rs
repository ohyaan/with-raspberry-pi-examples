use std::num::ParseIntError;

fn normalise(input: &str) -> &str {
    input.trim()
}

pub fn parse_reading(input: &str) -> Result<Option<i32>, ParseIntError> {
    let text = normalise(input);
    if text.is_empty() {
        return Ok(None);
    }
    Ok(Some(text.parse::<i32>()?))
}

#[cfg(test)]
mod tests {
    use super::{normalise, parse_reading};

    #[test]
    fn private_helper_trims_without_changing_inner_text() {
        assert_eq!(normalise(" 46 700 "), "46 700");
    }

    #[test]
    fn parser_rejects_invalid_or_overflowing_numbers() {
        assert!(parse_reading("bad").is_err());
        assert!(parse_reading("2147483648").is_err());
    }
}
