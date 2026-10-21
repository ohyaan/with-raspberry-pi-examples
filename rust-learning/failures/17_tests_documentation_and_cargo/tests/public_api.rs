use pi_report::{ParseError, analyze, parse_reading};
use std::error::Error;

#[test]
fn public_parser_preserves_the_zero_and_missing_contract() {
    assert_eq!(parse_reading("0").expect("valid fixture"), Some(0));
    assert_eq!(parse_reading("").expect("missing fixture"), None);
    assert_eq!(analyze(["0", ""], |_| true).measured, 1);
}

#[test]
fn public_errors_distinguish_syntax_from_domain_range() {
    let syntax = parse_reading("bad").expect_err("invalid fixture");
    assert_eq!(syntax.to_string(), "invalid integer \"bad\"");
    assert!(syntax.source().is_some());
    let range = parse_reading("125001").expect_err("outside fixture range");
    assert!(matches!(range, ParseError::OutOfRange { value: 125_001 }));
    assert!(range.source().is_none());
}
