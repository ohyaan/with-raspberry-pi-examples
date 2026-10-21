#![forbid(unsafe_code)]

use std::ffi::OsString;
use std::io::{self, Write};
use std::path::{Path, PathBuf};

pub const USAGE: &str = "Usage: pi_status [--root ROOT] [--strict]";

pub struct Config {
    pub root: PathBuf,
    pub strict: bool,
}

pub fn parse_args(
    arguments: impl IntoIterator<Item = OsString>,
) -> Result<Option<Config>, &'static str> {
    let mut arguments = arguments.into_iter();
    let mut root = None;
    let mut strict = false;
    while let Some(option) = arguments.next() {
        if option == "--root" && root.is_none() {
            root = Some(PathBuf::from(arguments.next().ok_or("missing root")?));
        } else if option == "--strict" && !strict {
            strict = true;
        } else if option == "--help" && root.is_none() && !strict && arguments.next().is_none() {
            return Ok(None);
        } else {
            return Err("unknown, duplicate or misplaced argument");
        }
    }
    Ok(Some(Config {
        root: root.unwrap_or_else(|| PathBuf::from("/")),
        strict,
    }))
}

#[derive(Debug, PartialEq)]
pub struct Memory {
    total_kib: u64,
    available_kib: u64,
}

/// Parse only the required memory fields, without guessing missing values.
///
/// ```
/// let memory = pi_status::parse_memory("MemTotal: 100 kB\nMemAvailable: 40 kB\n").unwrap();
/// assert_eq!(memory.used_estimate_kib(), 60);
/// ```
pub fn parse_memory(input: &str) -> Result<Memory, &'static str> {
    let mut total = None;
    let mut available = None;
    for line in input.lines() {
        let Some((key, value)) = line.split_once(':') else {
            continue;
        };
        let slot = match key {
            "MemTotal" => &mut total,
            "MemAvailable" => &mut available,
            _ => continue,
        };
        if slot.is_some() {
            return Err("duplicate memory field");
        }
        let mut parts = value.split_whitespace();
        let number = parts
            .next()
            .ok_or("missing memory value")?
            .parse::<u64>()
            .map_err(|_| "invalid memory value")?;
        if parts.next() != Some("kB") || parts.next().is_some() {
            return Err("invalid memory unit or extra tokens");
        }
        *slot = Some(number);
    }
    let total = total.ok_or("missing MemTotal")?;
    let available = available.ok_or("missing MemAvailable")?;
    if total == 0 || available > total {
        return Err("inconsistent memory values");
    }
    Ok(Memory {
        total_kib: total,
        available_kib: available,
    })
}

impl Memory {
    pub fn used_estimate_kib(&self) -> u64 {
        self.total_kib - self.available_kib
    }
}

#[derive(Debug, PartialEq)]
pub struct Thermal {
    zone: String,
    millidegrees: i32,
}

pub fn parse_thermal(zone: &str, temperature: &str) -> Result<Thermal, &'static str> {
    let zone = zone.trim();
    if zone.is_empty() || zone.chars().any(char::is_control) {
        return Err("invalid thermal zone name");
    }
    let millidegrees = temperature
        .trim()
        .parse::<i32>()
        .map_err(|_| "invalid temperature")?;
    Ok(Thermal {
        zone: zone.to_owned(),
        millidegrees,
    })
}

pub struct Snapshot {
    pub memory: Result<Memory, String>,
    pub thermal: Result<Thermal, String>,
}

impl Snapshot {
    pub fn complete(&self) -> bool {
        self.memory.is_ok() && self.thermal.is_ok()
    }
}

pub fn collect(root: &Path, read: impl Fn(&Path) -> io::Result<String>) -> Snapshot {
    let load = |relative: &str| {
        let path = root.join(relative);
        read(&path).map_err(|error| format!("{path:?}: {error}"))
    };
    let memory = load("proc/meminfo").and_then(|text| parse_memory(&text).map_err(str::to_owned));
    let thermal = (|| {
        let zone = load("sys/class/thermal/thermal_zone0/type")?;
        let temperature = load("sys/class/thermal/thermal_zone0/temp")?;
        parse_thermal(&zone, &temperature).map_err(str::to_owned)
    })();
    Snapshot { memory, thermal }
}

pub fn write_snapshot(
    snapshot: &Snapshot,
    output: &mut impl Write,
    diagnostics: &mut impl Write,
) -> io::Result<()> {
    match &snapshot.memory {
        Ok(memory) => writeln!(
            output,
            "memory_total_kib={}, memory_available_kib={}, memory_used_estimate_kib={}",
            memory.total_kib,
            memory.available_kib,
            memory.used_estimate_kib()
        )?,
        Err(reason) => {
            writeln!(output, "memory=unavailable")?;
            writeln!(diagnostics, "memory: {reason}")?;
        }
    }
    match &snapshot.thermal {
        Ok(thermal) => writeln!(
            output,
            "thermal_zone={:?}, temperature_mC={}",
            thermal.zone, thermal.millidegrees
        )?,
        Err(reason) => {
            writeln!(output, "thermal=unavailable")?;
            writeln!(diagnostics, "thermal: {reason}")?;
        }
    }
    output.flush()?;
    diagnostics.flush()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn memory_units_and_estimate_are_explicit() {
        let memory =
            parse_memory("MemAvailable: 1024 kB\nOther: 123\nMemTotal: 4096 kB\n").unwrap();
        assert_eq!(memory.used_estimate_kib(), 3072);
    }

    #[test]
    fn missing_duplicate_units_and_inconsistent_memory_are_errors() {
        for input in [
            "MemTotal: 4096 kB\n",
            "MemTotal: 4096 kB\nMemTotal: 4096 kB\nMemAvailable: 0 kB\n",
            "MemTotal: 4096 MB\nMemAvailable: 0 kB\n",
            "MemTotal: 1 kB\nMemAvailable: 2 kB\n",
            "MemTotal: 0 kB\nMemAvailable: 0 kB\n",
            "MemTotal: -1 kB\nMemAvailable: 0 kB\n",
        ] {
            assert!(parse_memory(input).is_err(), "{input}");
        }
    }

    #[test]
    fn zero_negative_and_trimmed_temperature_are_values() {
        assert_eq!(
            parse_thermal("cpu-thermal\n", "0\n").unwrap().millidegrees,
            0
        );
        assert_eq!(
            parse_thermal("fixture", " -500 ").unwrap().millidegrees,
            -500
        );
        assert!(parse_thermal("", "46700").is_err());
        assert!(parse_thermal("cpu-thermal", "invalid").is_err());
    }

    #[test]
    fn fixture_collection_uses_relative_paths_under_root() {
        let root = Path::new("fixture");
        let snapshot = collect(root, |path| {
            match path.strip_prefix(root).unwrap().to_str().unwrap() {
                "proc/meminfo" => Ok("MemTotal: 4096 kB\nMemAvailable: 1024 kB\n".into()),
                "sys/class/thermal/thermal_zone0/type" => Ok("cpu-thermal\n".into()),
                "sys/class/thermal/thermal_zone0/temp" => Ok("46700\n".into()),
                _ => panic!("unexpected path"),
            }
        });
        assert!(snapshot.complete());
        let mut output = Vec::new();
        let mut errors = Vec::new();
        write_snapshot(&snapshot, &mut output, &mut errors).unwrap();
        assert!(errors.is_empty());
        assert_eq!(
            String::from_utf8(output).unwrap(),
            "memory_total_kib=4096, memory_available_kib=1024, memory_used_estimate_kib=3072\nthermal_zone=\"cpu-thermal\", temperature_mC=46700\n"
        );
    }

    #[test]
    fn unavailable_is_not_a_zero_measurement() {
        let snapshot = collect(
            Path::new("fixture"),
            |_| Err(io::ErrorKind::NotFound.into()),
        );
        assert!(!snapshot.complete());
        let mut output = Vec::new();
        let mut errors = Vec::new();
        write_snapshot(&snapshot, &mut output, &mut errors).unwrap();
        assert_eq!(output, b"memory=unavailable\nthermal=unavailable\n");
        assert!(!errors.is_empty());
    }

    #[test]
    fn denied_thermal_read_does_not_discard_valid_memory() {
        let snapshot = collect(Path::new("fixture"), |path| {
            if path.ends_with("proc/meminfo") {
                Ok("MemTotal: 4096 kB\nMemAvailable: 1024 kB\n".into())
            } else {
                Err(io::ErrorKind::PermissionDenied.into())
            }
        });
        assert!(snapshot.memory.is_ok());
        assert!(snapshot.thermal.is_err());
        assert!(!snapshot.complete());
    }

    #[test]
    fn options_preserve_root_and_reject_duplicate_flags() {
        let config = parse_args(["--strict", "--root", "fixture"].map(OsString::from))
            .unwrap()
            .unwrap();
        assert!(config.strict);
        assert_eq!(config.root, Path::new("fixture"));
        assert!(parse_args(["--strict", "--strict"].map(OsString::from)).is_err());
        assert!(parse_args(["--root"].map(OsString::from)).is_err());
        assert!(
            parse_args(["--help"].map(OsString::from))
                .unwrap()
                .is_none()
        );
    }

    #[test]
    fn output_and_flush_failures_propagate() {
        struct Broken(bool);
        impl Write for Broken {
            fn write(&mut self, bytes: &[u8]) -> io::Result<usize> {
                if self.0 {
                    Err(io::ErrorKind::BrokenPipe.into())
                } else {
                    Ok(bytes.len())
                }
            }
            fn flush(&mut self) -> io::Result<()> {
                Err(io::ErrorKind::BrokenPipe.into())
            }
        }
        let snapshot = Snapshot {
            memory: parse_memory("MemTotal: 100 kB\nMemAvailable: 40 kB\n").map_err(str::to_owned),
            thermal: parse_thermal("fixture", "0").map_err(str::to_owned),
        };
        for fail_write in [true, false] {
            assert!(write_snapshot(&snapshot, &mut Broken(fail_write), &mut Vec::new()).is_err());
        }
    }
}
