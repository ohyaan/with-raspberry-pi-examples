#![forbid(unsafe_code)]

use std::error::Error;
use std::ffi::OsString;
use std::fmt;
use std::io::{self, Write};
use std::num::ParseIntError;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

const USAGE: &str = "Usage: rust_cli FILE [--threshold MILLICELSIUS]";

#[derive(Debug)]
enum CliError {
    Argument(&'static str),
    Read(io::Error),
    Invalid { line: usize, source: ParseIntError },
    Range { line: usize },
    Overflow,
    Output(io::Error),
}

impl fmt::Display for CliError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Argument(reason) => write!(formatter, "{reason}; {USAGE}"),
            Self::Read(error) => write!(formatter, "cannot read input: {error}"),
            Self::Invalid { line, source } => write!(formatter, "line {line}: {source}"),
            Self::Range { line } => write!(formatter, "line {line}: outside fixture range"),
            Self::Overflow => write!(formatter, "report sum overflow"),
            Self::Output(error) => write!(formatter, "cannot write report: {error}"),
        }
    }
}

impl Error for CliError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Read(error) | Self::Output(error) => Some(error),
            Self::Invalid { source, .. } => Some(source),
            _ => None,
        }
    }
}

struct Config {
    path: PathBuf,
    threshold: i32,
}

fn parse_args(arguments: impl IntoIterator<Item = OsString>) -> Result<Option<Config>, CliError> {
    let mut arguments = arguments.into_iter();
    let first = arguments.next().ok_or(CliError::Argument("missing file"))?;
    if first == "--help" {
        return if arguments.next().is_none() {
            Ok(None)
        } else {
            Err(CliError::Argument("help takes no extra arguments"))
        };
    }
    let mut threshold = 60_000;
    if let Some(option) = arguments.next() {
        if option != "--threshold" {
            return Err(CliError::Argument("unknown option"));
        }
        let value = arguments
            .next()
            .ok_or(CliError::Argument("missing threshold"))?;
        threshold = value
            .to_str()
            .and_then(|text| text.parse::<i32>().ok())
            .ok_or(CliError::Argument("invalid threshold"))?;
    }
    if arguments.next().is_some() {
        return Err(CliError::Argument("unexpected extra argument"));
    }
    if !(-100_000..=150_000).contains(&threshold) {
        return Err(CliError::Argument("threshold outside fixture range"));
    }
    Ok(Some(Config {
        path: first.into(),
        threshold,
    }))
}

#[derive(Debug, Default, PartialEq)]
struct Report {
    measured: usize,
    missing: usize,
    matching: usize,
    sum: i64,
}

fn report(input: &str, threshold: i32) -> Result<Report, CliError> {
    let mut result = Report::default();
    for (index, text) in input.lines().enumerate() {
        let text = text.trim();
        if text.is_empty() {
            result.missing += 1;
            continue;
        }
        let line = index + 1;
        let value = text
            .parse::<i32>()
            .map_err(|source| CliError::Invalid { line, source })?;
        if !(-100_000..=150_000).contains(&value) {
            return Err(CliError::Range { line });
        }
        result.sum = result
            .sum
            .checked_add(i64::from(value))
            .ok_or(CliError::Overflow)?;
        result.measured += 1;
        result.matching += usize::from(value >= threshold);
    }
    Ok(result)
}

fn execute(
    arguments: impl IntoIterator<Item = OsString>,
    read: impl FnOnce(&Path) -> io::Result<String>,
    output: &mut impl Write,
) -> Result<(), CliError> {
    let Some(config) = parse_args(arguments)? else {
        writeln!(output, "{USAGE}").map_err(CliError::Output)?;
        return output.flush().map_err(CliError::Output);
    };
    let input = read(&config.path).map_err(CliError::Read)?;
    let result = report(&input, config.threshold)?;
    writeln!(
        output,
        "measured={}, missing={}, matching={}, sum={}",
        result.measured, result.missing, result.matching, result.sum
    )
    .map_err(CliError::Output)?;
    output.flush().map_err(CliError::Output)
}

fn main() -> ExitCode {
    let result = execute(
        std::env::args_os().skip(1),
        |path| std::fs::read_to_string(path),
        &mut io::stdout().lock(),
    );
    match result {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            let _ = writeln!(io::stderr().lock(), "{error}");
            if matches!(error, CliError::Argument(_)) {
                ExitCode::from(2)
            } else {
                ExitCode::FAILURE
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn args(values: &[&str]) -> Vec<OsString> {
        values.iter().map(OsString::from).collect()
    }

    #[test]
    fn default_and_explicit_thresholds_are_distinct() {
        assert_eq!(
            parse_args(args(&["fixture"])).unwrap().unwrap().threshold,
            60_000
        );
        assert_eq!(
            parse_args(args(&["fixture", "--threshold", "0"]))
                .unwrap()
                .unwrap()
                .threshold,
            0
        );
    }

    #[test]
    fn malformed_arguments_do_not_reach_the_reader() {
        for input in [
            vec![],
            vec!["file", "--wrong"],
            vec!["file", "--threshold"],
            vec!["file", "--threshold", "NaN"],
            vec!["--help", "extra"],
        ] {
            let result = execute(args(&input), |_| panic!("must not read"), &mut Vec::new());
            assert!(matches!(result, Err(CliError::Argument(_))));
        }
    }

    #[test]
    fn help_is_successful_without_reading_input() {
        let mut output = Vec::new();
        execute(args(&["--help"]), |_| panic!("must not read"), &mut output).unwrap();
        assert_eq!(String::from_utf8(output).unwrap(), format!("{USAGE}\n"));
    }

    #[test]
    fn zero_blank_negative_and_boundary_are_not_conflated() {
        let result = report("0\n\n-500\n60000\n", 60_000).unwrap();
        assert_eq!(
            result,
            Report {
                measured: 3,
                missing: 1,
                matching: 1,
                sum: 59_500
            }
        );
        assert_eq!(report("", 0).unwrap(), Report::default());
    }

    #[test]
    fn invalid_record_has_a_line_and_error_source() {
        let error = report("46700\ninvalid\n", 0).unwrap_err();
        assert!(matches!(error, CliError::Invalid { line: 2, .. }));
        assert!(error.source().is_some());
    }

    #[test]
    fn out_of_range_record_rejects_the_report() {
        assert!(matches!(
            report("150001\n", 0),
            Err(CliError::Range { line: 1 })
        ));
    }

    #[test]
    fn read_failures_and_invalid_records_emit_no_success_report() {
        let mut output = Vec::new();
        let denied = execute(
            args(&["file"]),
            |_| Err(io::ErrorKind::PermissionDenied.into()),
            &mut output,
        );
        assert!(matches!(denied, Err(CliError::Read(_))));
        assert!(output.is_empty());
        assert!(execute(args(&["file"]), |_| Ok("invalid".into()), &mut output).is_err());
        assert!(output.is_empty());
    }

    #[test]
    fn successful_report_uses_the_injected_reader_and_writer() {
        let mut output = Vec::new();
        execute(
            args(&["file"]),
            |path| {
                assert_eq!(path, Path::new("file"));
                Ok("46700\n\n0\n60000\n".into())
            },
            &mut output,
        )
        .unwrap();
        assert_eq!(
            String::from_utf8(output).unwrap(),
            "measured=3, missing=1, matching=1, sum=106700\n"
        );
    }

    #[test]
    fn output_failure_is_an_error_not_success() {
        struct Broken;
        impl Write for Broken {
            fn write(&mut self, _: &[u8]) -> io::Result<usize> {
                Err(io::ErrorKind::BrokenPipe.into())
            }
            fn flush(&mut self) -> io::Result<()> {
                Ok(())
            }
        }
        assert!(matches!(
            execute(args(&["file"]), |_| Ok("0".into()), &mut Broken),
            Err(CliError::Output(_))
        ));
    }

    #[cfg(unix)]
    #[test]
    fn non_unicode_path_remains_an_os_path() {
        use std::os::unix::ffi::{OsStrExt, OsStringExt};
        let path = OsString::from_vec(vec![b'p', 0xff]);
        let config = parse_args([path]).unwrap().unwrap();
        assert_eq!(config.path.as_os_str().as_bytes(), &[b'p', 0xff]);
    }
}
