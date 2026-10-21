mod readings;

// Export the function without exposing the readings module itself.
pub use readings::parse_reading;

#[cfg(test)]
mod tests {
    use super::parse_reading;

    #[test]
    fn public_entry_preserves_missing_input() {
        assert_eq!(parse_reading(" ").expect("valid fixture"), None);
    }

    #[test]
    fn public_entry_preserves_real_zero() {
        assert_eq!(parse_reading("0").expect("valid fixture"), Some(0));
    }
}
