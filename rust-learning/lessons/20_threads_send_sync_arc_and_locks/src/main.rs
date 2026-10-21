use std::sync::{Arc, Mutex, RwLock};
use std::thread;

#[derive(Debug, Default, PartialEq, Eq)]
struct Summary {
    measured: usize,
    missing: usize,
    matching: usize,
}

fn summarize(values: &[Option<i32>], threshold: i32) -> Summary {
    let mut report = Summary::default();
    for value in values {
        match value {
            Some(value) => {
                report.measured += 1;
                report.matching += usize::from(*value >= threshold);
            }
            None => report.missing += 1,
        }
    }
    report
}

fn parallel_report(
    batches: Vec<Vec<Option<i32>>>,
    threshold: i32,
) -> Result<Summary, &'static str> {
    let shared = Arc::new(Mutex::new(Summary::default()));
    let handles: Vec<_> = batches
        .into_iter()
        .map(|batch| {
            let destination = Arc::clone(&shared);
            thread::spawn(move || -> Result<(), &'static str> {
                // Compute privately before holding the shared lock.
                let local = summarize(&batch, threshold);
                let mut total = destination.lock().map_err(|_| "summary poisoned")?;
                total.measured += local.measured;
                total.missing += local.missing;
                total.matching += local.matching;
                Ok(())
            })
        })
        .collect();

    let mut first_error = None;
    for handle in handles {
        // Even after one failure, join every remaining worker.
        let result = match handle.join() {
            Ok(result) => result,
            Err(_) => Err("worker panicked"),
        };
        if let Err(error) = result {
            first_error.get_or_insert(error);
        }
    }
    if let Some(error) = first_error {
        return Err(error);
    }

    let total = shared.lock().map_err(|_| "summary poisoned")?;
    Ok(Summary {
        measured: total.measured,
        missing: total.missing,
        matching: total.matching,
    })
}

fn scoped_sum(values: &[i32]) -> i64 {
    let (left, right) = values.split_at(values.len() / 2);
    thread::scope(|scope| {
        let first = scope.spawn(|| left.iter().map(|value| i64::from(*value)).sum::<i64>());
        let second = scope.spawn(|| right.iter().map(|value| i64::from(*value)).sum::<i64>());
        let first_result = first.join();
        let second_result = second.join();
        first_result.expect("left fixture worker panicked")
            + second_result.expect("right fixture worker panicked")
    })
}

fn main() {
    let label = String::from("Pi 4B");
    let worker = thread::spawn(move || format!("{label}: owned worker"));
    println!("{}", worker.join().expect("label worker panicked"));

    let batches = vec![vec![Some(46_700), None], vec![Some(0), Some(60_000)]];
    let report = parallel_report(batches, 60_000).expect("fixture report failed");
    println!(
        "measured={}, missing={}, matching={}",
        report.measured, report.missing, report.matching
    );

    let values = vec![46_700, 0, -500];
    println!(
        "scoped sum={}, retained={}",
        scoped_sum(&values),
        values.len()
    );

    let settings = RwLock::new(String::from("fixture"));
    {
        let first = settings.read().expect("settings poisoned");
        let second = settings.read().expect("settings poisoned");
        println!("readers={},{}", &*first, &*second);
    }
    *settings.write().expect("settings poisoned") = String::from("ready");
    println!("setting={}", &*settings.read().expect("settings poisoned"));
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::Cell;
    use std::sync::TryLockError;

    #[test]
    fn shared_report_matches_sequential_accounting() {
        let input = vec![Some(46_700), None, Some(0), Some(60_000), Some(-500)];
        let batches = vec![input[..2].to_vec(), input[2..].to_vec()];
        assert_eq!(
            parallel_report(batches, 60_000),
            Ok(summarize(&input, 60_000))
        );
    }

    #[test]
    fn empty_batches_and_missing_values_remain_distinct_from_zero() {
        assert_eq!(parallel_report(vec![], 0), Ok(Summary::default()));
        assert_eq!(parallel_report(vec![vec![]], 0), Ok(Summary::default()));
        assert_eq!(
            parallel_report(vec![vec![None, Some(0)]], 0),
            Ok(Summary {
                measured: 1,
                missing: 1,
                matching: 1,
            })
        );
    }

    #[test]
    fn scoped_workers_borrow_empty_singleton_and_negative_input() {
        assert_eq!(scoped_sum(&[]), 0);
        assert_eq!(scoped_sum(&[0]), 0);
        assert_eq!(scoped_sum(&[-500, 46_700]), 46_200);
    }

    #[test]
    fn cell_can_be_transferred_without_being_shared() {
        let value = Cell::new(0);
        let handle = thread::spawn(move || {
            value.set(7);
            value.get()
        });
        assert_eq!(handle.join().unwrap(), 7);
    }

    #[test]
    fn mutex_guard_blocks_try_lock_until_dropped() {
        let value = Mutex::new(0);
        let mut guard = value.lock().unwrap();
        *guard = 7;
        assert!(matches!(value.try_lock(), Err(TryLockError::WouldBlock)));
        drop(guard);
        assert_eq!(*value.try_lock().unwrap(), 7);
    }

    #[test]
    fn rwlock_allows_readers_but_excludes_a_writer() {
        let value = RwLock::new(0);
        let first = value.read().unwrap();
        let second = value.read().unwrap();
        assert!(matches!(value.try_write(), Err(TryLockError::WouldBlock)));
        drop(first);
        drop(second);
        let mut writer = value.write().unwrap();
        *writer = 7;
        assert!(matches!(value.try_read(), Err(TryLockError::WouldBlock)));
        drop(writer);
        assert_eq!(*value.read().unwrap(), 7);
    }

    #[test]
    fn join_reports_a_worker_panic() {
        let worker = thread::spawn(|| panic!("intentional fixture failure"));
        assert!(worker.join().is_err());
    }

    #[test]
    fn returned_error_is_not_a_thread_panic() {
        let worker = thread::spawn(|| Err::<(), _>("invalid fixture"));
        assert_eq!(worker.join().unwrap(), Err("invalid fixture"));
    }

    #[test]
    fn poisoned_state_is_repaired_before_clearing_the_flag() {
        let value = Arc::new(Mutex::new(0));
        let destination = Arc::clone(&value);
        let worker = thread::spawn(move || {
            let mut guard = destination.lock().unwrap();
            *guard = -1; // Invalid for this test's nonnegative-state contract.
            panic!("intentional interrupted update");
        });
        assert!(worker.join().is_err());
        assert!(value.is_poisoned());
        let mut guard = value.lock().unwrap_err().into_inner();
        assert_eq!(*guard, -1);
        *guard = 0; // Explicit repair of this known fixture invariant.
        value.clear_poison();
        drop(guard);
        assert_eq!(*value.lock().unwrap(), 0);
        assert!(!value.is_poisoned());
    }
}
