#![forbid(unsafe_code)]

use std::io::{self, Write};
use std::process::ExitCode;

fn main() -> ExitCode {
    let config = match pi_status::parse_args(std::env::args_os().skip(1)) {
        Ok(Some(config)) => config,
        Ok(None) => {
            return if writeln!(io::stdout().lock(), "{}", pi_status::USAGE).is_ok() {
                ExitCode::SUCCESS
            } else {
                ExitCode::FAILURE
            };
        }
        Err(reason) => {
            let _ = writeln!(io::stderr().lock(), "{reason}; {}", pi_status::USAGE);
            return ExitCode::from(2);
        }
    };
    let snapshot = pi_status::collect(&config.root, |path| std::fs::read_to_string(path));
    if let Err(error) = pi_status::write_snapshot(
        &snapshot,
        &mut io::stdout().lock(),
        &mut io::stderr().lock(),
    ) {
        let _ = writeln!(io::stderr().lock(), "output failed: {error}");
        return ExitCode::FAILURE;
    }
    if config.strict && !snapshot.complete() {
        ExitCode::FAILURE
    } else {
        ExitCode::SUCCESS
    }
}
