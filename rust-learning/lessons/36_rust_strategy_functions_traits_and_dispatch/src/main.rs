trait Policy {
    fn accept(&self, reading: i32) -> bool;
}

struct NonNegative;

impl Policy for NonNegative {
    fn accept(&self, reading: i32) -> bool {
        reading >= 0
    }
}

struct AtLeast {
    minimum: i32,
}

impl Policy for AtLeast {
    fn accept(&self, reading: i32) -> bool {
        reading >= self.minimum
    }
}

fn nonnegative(reading: i32) -> bool {
    reading >= 0
}

fn count_with<F: FnMut(i32) -> bool>(readings: &[i32], mut predicate: F) -> usize {
    let mut accepted = 0;
    for &reading in readings {
        if predicate(reading) {
            accepted += 1;
        }
    }
    accepted
}

fn count_policy<P: Policy + ?Sized>(readings: &[i32], policy: &P) -> usize {
    count_with(readings, |reading| policy.accept(reading))
}

fn main() {
    let readings = [-500, 0, 46700];
    let ordinary = NonNegative;
    let warm = AtLeast { minimum: 40000 };
    assert_eq!(count_with(&readings, nonnegative), 2);
    println!("nonnegative={}", count_with(&readings, nonnegative));
    assert_eq!(count_policy(&readings, &warm), 1);
    println!("warm={}", count_policy(&readings, &warm));
    let threshold = 0;
    let captured = move |reading| reading >= threshold;
    assert_eq!(count_with(&readings, captured), 2);
    println!("captured={}", count_with(&readings, captured));

    let candidates: [&dyn Policy; 2] = [&ordinary, &warm];
    let choose_warm = true;
    let selected = candidates[usize::from(choose_warm)];
    println!("dynamic={}", count_policy(&readings, selected));
    let changed = AtLeast { minimum: -1000 };
    assert_eq!(count_policy(&readings, &changed), 3);
    println!("changed={}", count_policy(&readings, &changed));

    let mut calls = 0;
    let counted = |reading| {
        calls += 1;
        reading >= 0
    };
    let accepted = count_with(&readings, counted);
    assert_eq!((accepted, calls), (2, readings.len()));
    println!("stateful={accepted}, calls={calls}");
    assert_eq!(readings, [-500, 0, 46700]);
    println!("retained={}", readings.len());
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn inclusive_threshold_retains_zero_and_boundary() {
        assert_eq!(count_with(&[-500, 0, 46700], nonnegative), 2);
        assert_eq!(
            count_policy(&[39999, 40000], &AtLeast { minimum: 40000 }),
            1
        );
        assert_eq!(count_policy(&[i32::MIN, 0, i32::MAX], &NonNegative), 2);
    }

    #[test]
    fn concrete_and_runtime_selected_policies_agree() {
        let readings = [-500, 0, 46700];
        let ordinary = NonNegative;
        let warm = AtLeast { minimum: 40000 };
        let candidates: [&dyn Policy; 2] = [&ordinary, &warm];
        for (choose_warm, expected) in [(false, 2), (true, 1)] {
            assert_eq!(
                count_policy(&readings, candidates[usize::from(choose_warm)]),
                expected
            );
        }
        assert_eq!(
            count_policy(&readings, &warm),
            count_with(&readings, |value| value >= 40000)
        );
    }

    #[test]
    fn runtime_threshold_data_changes_the_decision() {
        let readings = [-500, 0, 46700];
        for (minimum, expected) in [(-1000, 3), (0, 2), (40000, 1), (46701, 0)] {
            assert_eq!(count_policy(&readings, &AtLeast { minimum }), expected);
            assert_eq!(count_with(&readings, |value| value >= minimum), expected);
        }
    }

    #[test]
    fn stateful_decision_observes_each_input_once_in_order() {
        let readings = [-500, 0, 46700];
        let mut visited = Vec::new();
        let accepted = count_with(&readings, |value| {
            visited.push(value);
            value >= 0
        });
        assert_eq!(accepted, 2);
        assert_eq!(visited, readings);
        assert_eq!(readings, [-500, 0, 46700]);
    }

    #[test]
    fn empty_input_does_not_call_the_decision() {
        let mut calls = 0;
        assert_eq!(
            count_with(&[], |_| {
                calls += 1;
                true
            }),
            0
        );
        assert_eq!(calls, 0);
        assert_eq!(count_policy(&[], &NonNegative), 0);
    }
}
