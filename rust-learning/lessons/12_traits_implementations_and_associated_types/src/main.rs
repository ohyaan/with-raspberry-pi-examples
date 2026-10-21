mod sources {
    use std::fmt::Display;

    pub trait Source {
        type Value: Display;
        const UNIT: &'static str;

        fn read(&self) -> Option<Self::Value>;

        fn summary(&self) -> String {
            match self.read() {
                Some(value) => format!("{value} {}", Self::UNIT),
                None => format!("missing ({})", Self::UNIT),
            }
        }
    }

    #[derive(Debug, Clone, PartialEq, Eq)]
    pub struct Temperature {
        pub millidegrees: Option<i32>,
    }

    impl Source for Temperature {
        type Value = i32;
        const UNIT: &'static str = "mC";

        fn read(&self) -> Option<Self::Value> {
            self.millidegrees
        }
    }

    pub struct Status {
        pub online: bool,
    }

    impl Source for Status {
        type Value = String;
        const UNIT: &'static str = "status";

        fn read(&self) -> Option<Self::Value> {
            Some(String::from(if self.online { "online" } else { "offline" }))
        }
    }
}

use sources::{Source, Status, Temperature};

trait Named {
    fn name(&self) -> &str;
}

struct Device<S> {
    name: String,
    source: S,
}

impl<S> Named for Device<S> {
    fn name(&self) -> &str {
        &self.name
    }
}

// Delegate to the contained source while retaining its chosen value type.
impl<S: Source> Source for Device<S> {
    type Value = S::Value;
    const UNIT: &'static str = S::UNIT;

    fn read(&self) -> Option<Self::Value> {
        self.source.read()
    }
}

trait Report: Source + Named {
    fn report_line(&self) -> String {
        format!("{}: {}", self.name(), self.summary())
    }
}

// One implementation covers every type satisfying both requirements.
impl<T: Source + Named> Report for T {}

fn read_integer<S: Source<Value = i32>>(source: &S) -> Option<i32> {
    source.read()
}

fn main() {
    let device = Device {
        name: String::from("Pi 4B"),
        source: Temperature {
            millidegrees: Some(46_700),
        },
    };
    let status = Device {
        name: String::from("Pi 4B"),
        source: Status { online: true },
    };
    let missing = Temperature { millidegrees: None };

    println!("{}", device.report_line());
    println!("{}", status.report_line());
    println!("{}", missing.summary());
    println!("integer={:?}", read_integer(&device));
    println!("unit={}", <Temperature as Source>::UNIT);
}

#[cfg(test)]
mod tests {
    use super::{Device, Named, Report, Source, Status, Temperature, read_integer};

    #[test]
    fn required_method_preserves_zero() {
        let source = Temperature {
            millidegrees: Some(0),
        };
        assert_eq!(source.read(), Some(0));
        assert_eq!(source.summary(), "0 mC");
    }

    #[test]
    fn default_method_handles_absence() {
        let source = Temperature { millidegrees: None };
        assert_eq!(source.read(), None);
        assert_eq!(source.summary(), "missing (mC)");
    }

    #[test]
    fn associated_value_can_be_owned_text() {
        let source = Status { online: false };
        assert_eq!(source.read(), Some(String::from("offline")));
        assert_eq!(source.summary(), "offline status");
    }

    #[test]
    fn generic_device_delegates_integer_source() {
        let device = Device {
            name: String::from("fixture"),
            source: Temperature {
                millidegrees: Some(-500),
            },
        };
        assert_eq!(device.name(), "fixture");
        assert_eq!(read_integer(&device), Some(-500));
        assert_eq!(device.report_line(), "fixture: -500 mC");
    }

    #[test]
    fn blanket_report_also_works_for_text_source() {
        let device = Device {
            name: String::from("network"),
            source: Status { online: true },
        };
        assert_eq!(device.report_line(), "network: online status");
    }

    #[test]
    fn derived_clone_and_equality_preserve_fixture() {
        let source = Temperature {
            millidegrees: Some(46_700),
        };
        let cloned = source.clone();
        assert_eq!(cloned, source);
        assert_eq!(
            format!("{source:?}"),
            "Temperature { millidegrees: Some(46700) }"
        );
    }

    #[test]
    fn associated_constant_is_implementation_specific() {
        assert_eq!(<Temperature as Source>::UNIT, "mC");
        assert_eq!(<Status as Source>::UNIT, "status");
        assert_eq!(<Device<Temperature> as Source>::UNIT, "mC");
    }
}
