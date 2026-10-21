type MilliAlias = i32;

#[derive(Debug, PartialEq)]
struct Milli(i32);

#[derive(Debug, PartialEq)]
struct Celsius(i32);

fn alias_identity(value: MilliAlias) -> i32 {
    value
}

fn to_celsius(value: Milli) -> f64 {
    f64::from(value.0) / 1_000.0
}

fn missing_fixture() -> ! {
    panic!("missing fixture");
}

fn require_fixture(value: Option<Milli>) -> Milli {
    match value {
        Some(value) => value,
        None => missing_fixture(),
    }
}

fn main() {
    let plain: i32 = 46_700;
    println!("alias accepts i32={}", alias_identity(plain));
    let reading = require_fixture(Some(Milli(plain)));
    println!("newtype display={:.1} C", to_celsius(reading));
    let whole_degrees = Celsius(46);
    println!("distinct Celsius={}", whole_degrees.0);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn alias_accepts_an_unwrapped_integer() {
        assert_eq!(alias_identity(46_700_i32), 46_700);
    }

    #[test]
    fn conversion_preserves_zero_and_negative_values() {
        assert_eq!(to_celsius(Milli(0)), 0.0);
        assert_eq!(to_celsius(Milli(-500)), -0.5);
    }

    #[test]
    fn present_fixture_retains_its_unit() {
        assert_eq!(require_fixture(Some(Milli(46_700))), Milli(46_700));
    }

    #[test]
    #[should_panic(expected = "missing fixture")]
    fn absent_fixture_panics_by_this_example_contract() {
        require_fixture(None);
    }
}
