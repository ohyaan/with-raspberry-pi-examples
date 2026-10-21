use std::fmt::Display;

struct Reading<T> {
    value: T,
}

impl<T> Reading<T> {
    fn new(value: T) -> Self {
        Self { value }
    }

    fn value(&self) -> &T {
        &self.value
    }
}

// This method exists only for integer millidegree readings.
impl Reading<i32> {
    fn celsius(&self) -> f64 {
        f64::from(self.value) / 1_000.0
    }
}

type Millidegrees = Reading<i32>;

fn largest<T: Ord>(items: &[T]) -> Option<&T> {
    let mut best = items.first()?;
    for item in &items[1..] {
        if item > best {
            best = item;
        }
    }
    Some(best)
}

fn report<T>(label: &str, items: &[T])
where
    T: Ord + Display,
{
    match largest(items) {
        Some(value) => println!("{label}: {value}"),
        None => println!("{label}: empty"),
    }
}

struct Window<T, const N: usize> {
    values: [T; N],
}

impl<T, const N: usize> Window<T, N> {
    fn new(values: [T; N]) -> Self {
        Self { values }
    }

    fn as_slice(&self) -> &[T] {
        &self.values
    }

    fn len(&self) -> usize {
        N
    }
}

fn main() {
    let reading = Millidegrees::new(46_700);
    println!("reading={} ({:.1} C)", reading.value(), reading.celsius());

    let window = Window::<i32, 3>::new([46_700, 60_000, -500]);
    report("max", window.as_slice());
    println!("window length={}", window.len());

    // String is ordered but not Copy: largest only borrows these values.
    let labels = [String::from("Pi 4B"), String::from("Pi 5")];
    report("label", &labels);
    println!("still owned={}", labels[0]);

    let empty = Window::<i32, 0>::new([]);
    report("empty window", empty.as_slice());
}

#[cfg(test)]
mod tests {
    use super::{Millidegrees, Reading, Window, largest};

    #[test]
    fn empty_slice_has_no_maximum() {
        assert_eq!(largest::<i32>(&[]), None);
    }

    #[test]
    fn maximum_handles_negative_values_and_zero() {
        assert_eq!(largest(&[-500, 0, -100]), Some(&0));
    }

    #[test]
    fn equal_maxima_keep_the_first_element() {
        let values = [7, 7, 2];
        let chosen = largest(&values).expect("nonempty fixture");
        assert!(std::ptr::eq(chosen, &values[0]));
    }

    #[test]
    fn maximum_borrows_non_copy_strings() {
        let values = [String::from("Pi 4B"), String::from("Pi 5")];
        assert_eq!(largest(&values).expect("nonempty fixture"), "Pi 5");
        assert_eq!(values[0], "Pi 4B");
    }

    #[test]
    fn generic_reading_accepts_owned_text() {
        let reading = Reading::new(String::from("online"));
        assert_eq!(reading.value(), "online");
    }

    #[test]
    fn concrete_method_converts_millidegrees() {
        let reading = Millidegrees::new(-500);
        assert_eq!(reading.celsius(), -0.5);
    }

    #[test]
    fn const_lengths_include_zero_and_singleton() {
        let empty = Window::<i32, 0>::new([]);
        let one = Window::new([46_700]);
        assert_eq!(empty.len(), 0);
        assert_eq!(largest(empty.as_slice()), None);
        assert_eq!(one.len(), 1);
        assert_eq!(largest(one.as_slice()), Some(&46_700));
    }
}
