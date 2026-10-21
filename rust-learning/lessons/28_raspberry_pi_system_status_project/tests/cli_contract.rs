use std::process::Command;

#[test]
fn help_has_success_status_and_no_diagnostic() {
    let output = Command::new(env!("CARGO_BIN_EXE_pi_status"))
        .arg("--help")
        .output()
        .unwrap();
    assert!(output.status.success());
    assert_eq!(
        output.stdout,
        b"Usage: pi_status [--root ROOT] [--strict]\n"
    );
    assert!(output.stderr.is_empty());
}

#[test]
fn argument_error_has_no_success_report() {
    let output = Command::new(env!("CARGO_BIN_EXE_pi_status"))
        .arg("--wrong")
        .output()
        .unwrap();
    assert_eq!(output.status.code(), Some(2));
    assert!(output.stdout.is_empty());
    assert!(!output.stderr.is_empty());
}
