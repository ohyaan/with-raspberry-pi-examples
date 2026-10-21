pub trait FixtureName {
    fn fixture_name() -> &'static str;
}

/// Accept a fixture reading within this example's declared range.
/// This is a teaching policy, not a hardware specification.
pub fn checked_reading(value: i32) -> Option<i32> {
    (-100_000..=150_000).contains(&value).then_some(value)
}

#[macro_export]
macro_rules! readings {
    () => {
        ::std::vec::Vec::new()
    };
    ($($value:expr),+ $(,)?) => {{
        let mut values = ::std::vec::Vec::new();
        $(values.push($value);)+
        values
    }};
}

#[macro_export]
macro_rules! bounded_reading {
    ($value:expr) => {
        $crate::checked_reading($value)
    };
}

#[cfg(test)]
mod tests {
    #[test]
    fn empty_input_has_an_explicit_element_type() {
        let values: Vec<i32> = crate::readings!();
        assert!(values.is_empty());
    }

    #[test]
    fn trailing_comma_and_zero_are_preserved() {
        assert_eq!(crate::readings!(46_700, 0, -500,), [46_700, 0, -500]);
    }

    #[test]
    fn expressions_run_once_in_list_order() {
        let mut calls = 0;
        let values = crate::readings!(
            {
                calls += 1;
                calls
            },
            {
                calls += 1;
                calls
            },
        );
        assert_eq!(values, [1, 2]);
        assert_eq!(calls, 2);
    }

    #[test]
    fn expanded_local_does_not_replace_callers_binding() {
        let values = "caller";
        let generated = crate::readings!(0);
        assert_eq!(values, "caller");
        assert_eq!(generated, [0]);
    }

    #[test]
    fn function_still_enforces_range_policy() {
        assert_eq!(crate::bounded_reading!(-100_000), Some(-100_000));
        assert_eq!(crate::bounded_reading!(150_000), Some(150_000));
        assert_eq!(crate::bounded_reading!(150_001), None);
    }
}
