trait Source {
    fn read(&self) -> Option<i32>;
}

struct Fixed(i32);
struct Missing;

impl Source for Fixed {
    fn read(&self) -> Option<i32> {
        Some(self.0)
    }
}

impl Source for Missing {
    fn read(&self) -> Option<i32> {
        None
    }
}

fn generic_read<T: Source>(source: &T) -> Option<i32> {
    source.read()
}

fn dynamic_read(source: &dyn Source) -> Option<i32> {
    source.read()
}

fn collect(sources: &[&dyn Source]) -> (usize, usize, i64) {
    let mut present = 0;
    let mut missing = 0;
    let mut sum = 0_i64;
    for source in sources {
        match source.read() {
            Some(value) => {
                present += 1;
                sum += i64::from(value);
            }
            None => missing += 1,
        }
    }
    (present, missing, sum)
}

fn make_fixed(value: i32) -> impl Source {
    Fixed(value)
}

fn main() {
    let fixed = Fixed(46_700);
    let missing = Missing;
    let negative = Fixed(-500);
    let sources: [&dyn Source; 3] = [&fixed, &missing, &negative];
    let (present, absent, sum) = collect(&sources);
    println!("generic={}", generic_read(&fixed).expect("present fixture"));
    println!("dynamic={}", dynamic_read(&fixed).expect("present fixture"));
    println!("present={present}, missing={absent}, sum={sum}");
    println!("factory={}", make_fixed(0).read().expect("zero fixture"));
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn generic_and_dynamic_calls_agree() {
        let source = Fixed(46_700);
        assert_eq!(generic_read(&source), Some(46_700));
        assert_eq!(dynamic_read(&source), generic_read(&source));
    }

    #[test]
    fn zero_and_missing_are_distinct() {
        assert_eq!(generic_read(&Fixed(0)), Some(0));
        assert_eq!(dynamic_read(&Missing), None);
        assert_eq!(make_fixed(0).read(), Some(0));
    }

    #[test]
    fn mixed_collection_accounts_for_every_source() {
        let first = Fixed(46_700);
        let second = Fixed(-500);
        assert_eq!(collect(&[&first, &Missing, &second]), (2, 1, 46_200));
        assert_eq!(first.0, 46_700);
    }

    #[test]
    fn empty_collection_has_no_readings() {
        assert_eq!(collect(&[]), (0, 0, 0));
    }
}
