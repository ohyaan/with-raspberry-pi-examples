use pi_modules::parse_reading;

#[test]
fn external_caller_uses_the_reexport() {
    assert_eq!(parse_reading(" -500 ").expect("valid fixture"), Some(-500));
    assert!(parse_reading("invalid").is_err());
}
