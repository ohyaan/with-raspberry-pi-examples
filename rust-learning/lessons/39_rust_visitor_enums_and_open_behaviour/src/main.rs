#[derive(Debug)]
enum Item {
    Value(i32),
    Missing(String),
}

#[derive(Debug, PartialEq, Eq)]
struct Report {
    sum: i64,
    missing: usize,
    labels: String,
}

fn report(items: &[Item]) -> Report {
    let mut result = Report {
        sum: 0,
        missing: 0,
        labels: String::new(),
    };
    let mut labels = Vec::new();
    for item in items {
        match item {
            Item::Value(value) => {
                result.sum += i64::from(*value);
                labels.push(format!("value={value}"));
            }
            Item::Missing(reason) => {
                result.missing += 1;
                labels.push(format!("missing={reason}"));
            }
        }
    }
    result.labels = labels.join("|");
    result
}

fn zero_count(items: &[Item]) -> usize {
    items
        .iter()
        .filter(|item| match item {
            Item::Value(value) => *value == 0,
            Item::Missing(_) => false,
        })
        .count()
}

trait Describe {
    fn describe(&self) -> String;
}

struct Custom {
    label: String,
}
impl Describe for Custom {
    fn describe(&self) -> String {
        format!("custom={}", self.label)
    }
}

fn display(value: &dyn Describe) -> String {
    value.describe()
}

fn main() {
    let items = [
        Item::Value(0),
        Item::Value(-500),
        Item::Missing("offline".into()),
        Item::Value(46700),
    ];
    let result = report(&items);
    println!("sum={}, missing={}", result.sum, result.missing);
    println!("labels={}", result.labels);
    println!("zero={}", zero_count(&items));
    let custom = Custom {
        label: "Pi 4B".into(),
    };
    println!("open={}", display(&custom));
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn zero_and_missing_have_different_meanings() {
        let items = [Item::Value(0), Item::Missing("offline".into())];
        assert_eq!(report(&items).sum, 0);
        assert_eq!(report(&items).missing, 1);
        assert_eq!(zero_count(&items), 1);
    }

    #[test]
    fn signed_values_are_accumulated_without_i32_overflow() {
        let items = [
            Item::Value(i32::MIN),
            Item::Value(i32::MAX),
            Item::Value(-500),
        ];
        assert_eq!(report(&items).sum, -501);
        assert_eq!(zero_count(&items), 0);
    }

    #[test]
    fn labels_preserve_input_order_and_blank_reason() {
        let items = [Item::Missing(String::new()), Item::Value(0)];
        assert_eq!(report(&items).labels, "missing=|value=0");
        assert_eq!(report(&items).missing, 1);
    }

    #[test]
    fn empty_input_has_no_phantom_reading() {
        assert_eq!(
            report(&[]),
            Report {
                sum: 0,
                missing: 0,
                labels: String::new()
            }
        );
        assert_eq!(zero_count(&[]), 0);
    }

    #[test]
    fn display_borrows_an_independently_owned_custom_type() {
        let custom = Custom {
            label: "Pi 4B".into(),
        };
        assert_eq!(display(&custom), "custom=Pi 4B");
        assert_eq!(custom.label, "Pi 4B");
    }
}
