use std::fmt;

impl fmt::Display for Vec<i32> {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(formatter, "{:?}", self)
    }
}

fn main() {}
