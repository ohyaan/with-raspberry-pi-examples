mod config {
    use std::num::NonZeroU32;

    #[derive(Debug, PartialEq, Eq)]
    pub enum Error {
        MissingLower,
        MissingUpper,
        Bounds,
        Interval,
    }

    #[derive(Debug, PartialEq, Eq)]
    pub struct Config {
        lower: i32,
        upper: i32,
        period: NonZeroU32,
    }

    impl Config {
        pub fn try_new(lower: i32, upper: i32, period: u32) -> Result<Self, Error> {
            if lower > upper {
                return Err(Error::Bounds);
            }
            let period = NonZeroU32::new(period).ok_or(Error::Interval)?;
            if period.get() > 60000 {
                return Err(Error::Interval);
            }
            Ok(Self {
                lower,
                upper,
                period,
            })
        }

        pub fn lower(&self) -> i32 {
            self.lower
        }

        pub fn upper(&self) -> i32 {
            self.upper
        }

        pub fn period(&self) -> u32 {
            self.period.get()
        }
    }

    #[derive(Default)]
    pub struct Builder {
        lower: Option<i32>,
        upper: Option<i32>,
        period: Option<u32>,
    }

    impl Builder {
        pub fn lower(mut self, value: i32) -> Self {
            self.lower = Some(value);
            self
        }

        pub fn upper(mut self, value: i32) -> Self {
            self.upper = Some(value);
            self
        }

        pub fn period(mut self, value: u32) -> Self {
            self.period = Some(value);
            self
        }

        pub fn build(self) -> Result<Config, Error> {
            let lower = self.lower.ok_or(Error::MissingLower)?;
            let upper = self.upper.ok_or(Error::MissingUpper)?;
            Config::try_new(lower, upper, self.period.unwrap_or(1000))
        }
    }
}

use config::{Builder, Config, Error};

fn main() {
    let direct = Config::try_new(0, 46700, 1000).unwrap();
    println!(
        "direct={}..{}, period={}",
        direct.lower(),
        direct.upper(),
        direct.period()
    );
    let built = Builder::default()
        .lower(-500)
        .upper(0)
        .period(250)
        .build()
        .unwrap();
    println!(
        "built={}..{}, period={}",
        built.lower(),
        built.upper(),
        built.period()
    );
    let missing = Builder::default().build().unwrap_err();
    println!("missing={missing:?}");
    let zero = Builder::default()
        .lower(0)
        .upper(46700)
        .period(0)
        .build()
        .unwrap_err();
    println!("zero period={zero:?}");
    let bounds = Config::try_new(1, 0, 1000).unwrap_err();
    println!("invalid bounds={bounds:?}");
    let repeated = Builder::default()
        .lower(0)
        .upper(0)
        .period(250)
        .period(500)
        .build()
        .unwrap();
    assert_eq!(repeated.period(), 500);
    println!("last period={}", repeated.period());
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn missing_bounds_are_not_zero() {
        assert_eq!(Builder::default().build(), Err(Error::MissingLower));
        assert_eq!(
            Builder::default().lower(0).build(),
            Err(Error::MissingUpper)
        );
        let value = Builder::default().lower(0).upper(0).build().unwrap();
        assert_eq!((value.lower(), value.upper(), value.period()), (0, 0, 1000));
    }

    #[test]
    fn direct_and_builder_paths_share_value_validation() {
        for (lower, upper) in [(-500, 0), (0, 46700), (i32::MIN, i32::MAX)] {
            let direct = Config::try_new(lower, upper, 250);
            let built = Builder::default()
                .lower(lower)
                .upper(upper)
                .period(250)
                .build();
            assert_eq!(direct, built);
            assert!(direct.is_ok());
        }
        assert_eq!(Config::try_new(1, 0, 250), Err(Error::Bounds));
        assert_eq!(
            Builder::default().lower(1).upper(0).build(),
            Err(Error::Bounds)
        );
    }

    #[test]
    fn period_boundaries_and_explicit_zero_are_checked() {
        for period in [1, 60000] {
            assert_eq!(Config::try_new(0, 0, period).unwrap().period(), period);
        }
        for period in [0, 60001, u32::MAX] {
            assert_eq!(Config::try_new(0, 0, period), Err(Error::Interval));
            assert_eq!(
                Builder::default().lower(0).upper(0).period(period).build(),
                Err(Error::Interval)
            );
        }
    }

    #[test]
    fn error_precedence_is_explicit() {
        assert_eq!(
            Builder::default().period(0).build(),
            Err(Error::MissingLower)
        );
        assert_eq!(
            Builder::default().lower(0).period(0).build(),
            Err(Error::MissingUpper)
        );
        assert_eq!(Config::try_new(1, 0, 0), Err(Error::Bounds));
    }

    #[test]
    fn later_setters_replace_earlier_choices() {
        let value = Builder::default()
            .lower(10)
            .lower(-500)
            .upper(0)
            .period(0)
            .period(500)
            .build()
            .unwrap();
        assert_eq!(
            (value.lower(), value.upper(), value.period()),
            (-500, 0, 500)
        );
    }
}
