use pi_report::parse_reading;

#[test]
fn zero_is_not_missing() {
    assert_eq!(parse_reading("0").expect("valid fixture"), None);
}
