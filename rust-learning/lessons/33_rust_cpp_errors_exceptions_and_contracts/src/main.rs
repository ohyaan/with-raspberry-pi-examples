use std::cell::Cell;
use std::num::IntErrorKind;

#[derive(Debug, PartialEq, Eq)]
enum Error {
    Format,
    Range,
}

fn parse(text: &str) -> Result<i32, Error> {
    let digits = text.strip_prefix('-').unwrap_or(text);
    if digits.is_empty() || !digits.bytes().all(|digit| digit.is_ascii_digit()) {
        return Err(Error::Format);
    }
    text.parse::<i32>().map_err(|error| match error.kind() {
        IntErrorKind::PosOverflow | IntErrorKind::NegOverflow => Error::Range,
        _ => Error::Format,
    })
}

struct Guard<'a> {
    closes: &'a Cell<usize>,
}

impl Drop for Guard<'_> {
    fn drop(&mut self) {
        self.closes.set(self.closes.get() + 1);
    }
}

fn run(text: &str, closes: &Cell<usize>) -> Result<i32, Error> {
    let _guard = Guard { closes };
    let value = parse(text)?;
    Ok(value)
}

fn main() {
    let closes = Cell::new(0);
    for text in ["0", "-500", "bad", "2147483648"] {
        match run(text, &closes) {
            Ok(value) => println!("OK={value}"),
            Err(error) => println!("ERR={error:?}"),
        }
    }
    assert_eq!(closes.get(), 4);
    println!("closes={}", closes.get());
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn signed_boundaries_are_valid() {
        assert_eq!(parse("2147483647"), Ok(i32::MAX));
        assert_eq!(parse("-2147483648"), Ok(i32::MIN));
    }

    #[test]
    fn zero_and_negative_values_are_not_errors() {
        assert_eq!(parse("0"), Ok(0));
        assert_eq!(parse("-0"), Ok(0));
        assert_eq!(parse("-500"), Ok(-500));
        assert_eq!(parse("0001"), Ok(1));
    }

    #[test]
    fn syntax_has_priority_over_range() {
        for text in ["", "-", "+1", " 1", "1 ", "1x", "１２", "999999999999x"] {
            assert_eq!(parse(text), Err(Error::Format), "{text:?}");
        }
    }

    #[test]
    fn valid_syntax_can_exceed_the_range() {
        assert_eq!(parse("2147483648"), Err(Error::Range));
        assert_eq!(parse("-2147483649"), Err(Error::Range));
    }

    #[test]
    fn cleanup_runs_on_success_and_error_returns() {
        let closes = Cell::new(0);
        assert_eq!(run("0", &closes), Ok(0));
        assert_eq!(closes.get(), 1);
        assert_eq!(run("bad", &closes), Err(Error::Format));
        assert_eq!(closes.get(), 2);
        assert_eq!(run("2147483648", &closes), Err(Error::Range));
        assert_eq!(closes.get(), 3);
    }
}
