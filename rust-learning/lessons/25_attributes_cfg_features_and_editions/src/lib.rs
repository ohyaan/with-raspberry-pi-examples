#![forbid(unsafe_code)]
#![deny(unexpected_cfgs)]

pub const UNIT: &str = "mC";
pub static FIXTURE_NAME: &str = "Pi 4B fixture";

#[cfg(target_os = "linux")]
pub const PLATFORM: &str = "linux";

#[cfg(not(target_os = "linux"))]
pub const PLATFORM: &str = "other";

#[cfg_attr(feature = "detailed", derive(Debug))]
pub struct Mode;

pub const fn valid(value: i32) -> bool {
    value >= -100_000 && value <= 150_000
}

#[cfg(feature = "detailed")]
pub fn detail(value: i32) -> String {
    format!("{FIXTURE_NAME}: {value} {UNIT}")
}

pub fn render(value: i32) -> Option<String> {
    if !valid(value) {
        return None;
    }
    #[cfg(feature = "detailed")]
    let rendered = detail(value);
    #[cfg(not(feature = "detailed"))]
    let rendered = format!("{value} {UNIT}");
    Some(rendered)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fixture_policy_includes_both_boundaries_and_zero() {
        const ZERO_VALID: bool = valid(0);
        assert!(ZERO_VALID);
        assert!(valid(-100_000));
        assert!(valid(150_000));
        assert!(!valid(150_001));
        assert_eq!(render(-100_001), None);
    }

    #[test]
    fn common_api_remains_available_in_both_configurations() {
        assert!(render(0).unwrap().ends_with("0 mC"));
        assert!(render(-500).unwrap().ends_with("-500 mC"));
    }

    #[test]
    fn target_predicate_selects_exactly_one_platform() {
        if cfg!(target_os = "linux") {
            assert_eq!(PLATFORM, "linux");
        } else {
            assert_eq!(PLATFORM, "other");
        }
    }

    #[test]
    fn raw_identifier_is_not_a_raw_string() {
        let r#gen = r#"{"reading":0}"#;
        assert_eq!(r#gen.as_bytes()[0], b'{');
        assert!(r#gen.contains("reading"));
    }

    #[cfg(not(feature = "detailed"))]
    #[test]
    fn disabled_detail_keeps_plain_output() {
        assert_eq!(render(0).as_deref(), Some("0 mC"));
    }

    #[cfg(feature = "detailed")]
    #[test]
    fn enabled_detail_adds_api_output_and_debug_derivation() {
        assert_eq!(detail(0), "Pi 4B fixture: 0 mC");
        assert_eq!(format!("{:?}", Mode), "Mode");
        assert_eq!(render(0).as_deref(), Some("Pi 4B fixture: 0 mC"));
    }
}
