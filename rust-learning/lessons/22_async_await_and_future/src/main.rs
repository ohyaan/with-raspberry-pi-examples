use std::future::Future;
use std::num::ParseIntError;
use std::pin::{Pin, pin};
use std::sync::{Arc, Condvar, Mutex};
use std::task::{Context, Poll, Wake, Waker};

// One future, one waiting thread. This is not an I/O runtime or task scheduler.
struct Notification {
    notified: Mutex<bool>,
    changed: Condvar,
}

impl Wake for Notification {
    fn wake(self: Arc<Self>) {
        self.wake_by_ref();
    }

    fn wake_by_ref(self: &Arc<Self>) {
        *self.notified.lock().expect("notification poisoned") = true;
        self.changed.notify_one();
    }
}

fn block_on<F: Future>(future: F) -> F::Output {
    let notification = Arc::new(Notification {
        notified: Mutex::new(false),
        changed: Condvar::new(),
    });
    let waker = Waker::from(Arc::clone(&notification));
    let mut context = Context::from_waker(&waker);
    let mut future = pin!(future);
    loop {
        // Reset before polling, so a wake during poll is not overwritten.
        *notification.notified.lock().unwrap() = false;
        match future.as_mut().poll(&mut context) {
            Poll::Ready(value) => return value,
            Poll::Pending => {
                let notified = notification.notified.lock().unwrap();
                let _guard = notification
                    .changed
                    .wait_while(notified, |notified| !*notified)
                    .unwrap();
            }
        }
    }
}

struct YieldOnce {
    yielded: bool,
}

impl Future for YieldOnce {
    type Output = ();

    fn poll(mut self: Pin<&mut Self>, context: &mut Context<'_>) -> Poll<()> {
        if self.yielded {
            Poll::Ready(())
        } else {
            self.yielded = true;
            context.waker().wake_by_ref();
            Poll::Pending
        }
    }
}

async fn reading(text: &str) -> Result<Option<i32>, ParseIntError> {
    YieldOnce { yielded: false }.await;
    let text = text.trim();
    if text.is_empty() {
        Ok(None)
    } else {
        text.parse().map(Some)
    }
}

#[derive(Debug, PartialEq, Eq)]
struct Report {
    measured: usize,
    missing: usize,
}

async fn report(inputs: &[&str]) -> Result<Report, ParseIntError> {
    let mut report = Report {
        measured: 0,
        missing: 0,
    };
    for input in inputs {
        match reading(input).await? {
            Some(_) => report.measured += 1,
            None => report.missing += 1,
        }
    }
    Ok(report)
}

fn main() {
    let inputs = ["46700", "", "0"];
    let future = report(&inputs);
    println!("future created; input length={}", inputs.len());
    let result = block_on(future).expect("valid fixtures");
    println!("measured={}, missing={}", result.measured, result.missing);
    println!("zero={:?}", block_on(reading("0")).unwrap());
    println!("invalid rejected={}", block_on(reading("broken")).is_err());

    let label = String::from("Pi 4B");
    let owned = async move { label.len() };
    println!("owned label bytes={}", block_on(owned));
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::Cell;
    use std::rc::Rc;
    use std::sync::atomic::{AtomicUsize, Ordering};

    struct CountWake(AtomicUsize);

    impl Wake for CountWake {
        fn wake(self: Arc<Self>) {
            self.0.fetch_add(1, Ordering::Relaxed);
        }

        fn wake_by_ref(self: &Arc<Self>) {
            self.0.fetch_add(1, Ordering::Relaxed);
        }
    }

    struct DropFlag(Rc<Cell<bool>>);

    impl Drop for DropFlag {
        fn drop(&mut self) {
            self.0.set(true);
        }
    }

    #[test]
    fn zero_missing_negative_and_invalid_are_distinct() {
        assert_eq!(block_on(reading(" 0 ")).unwrap(), Some(0));
        assert_eq!(block_on(reading(" \t")).unwrap(), None);
        assert_eq!(block_on(reading("-500")).unwrap(), Some(-500));
        assert!(block_on(reading("broken")).is_err());
    }

    #[test]
    fn sequential_await_accounts_for_each_valid_record() {
        assert_eq!(
            block_on(report(&["46700", "", "0"])).unwrap(),
            Report {
                measured: 2,
                missing: 1,
            }
        );
        assert!(block_on(report(&["0", "broken", "7"])).is_err());
    }

    #[test]
    fn empty_input_completes_without_a_pending_read() {
        assert_eq!(
            block_on(report(&[])).unwrap(),
            Report {
                measured: 0,
                missing: 0,
            }
        );
    }

    #[test]
    fn unpolled_body_does_not_run() {
        let calls = Cell::new(0);
        let future = async {
            calls.set(calls.get() + 1);
            7
        };
        assert_eq!(calls.get(), 0);
        assert_eq!(block_on(future), 7);
        assert_eq!(calls.get(), 1);
    }

    #[test]
    fn dropping_an_unpolled_future_skips_its_body() {
        let calls = Cell::new(0);
        let future = async { calls.set(1) };
        drop(future);
        assert_eq!(calls.get(), 0);
    }

    #[test]
    fn pending_requests_a_wake_then_completes() {
        let count = Arc::new(CountWake(AtomicUsize::new(0)));
        let waker = Waker::from(Arc::clone(&count));
        let mut context = Context::from_waker(&waker);
        let mut future = pin!(YieldOnce { yielded: false });
        assert_eq!(future.as_mut().poll(&mut context), Poll::Pending);
        assert_eq!(count.0.load(Ordering::Relaxed), 1);
        assert_eq!(future.as_mut().poll(&mut context), Poll::Ready(()));
    }

    #[test]
    fn cancelling_pending_future_drops_locals_without_undoing_effects() {
        let dropped = Rc::new(Cell::new(false));
        let observed = Rc::clone(&dropped);
        let effects = Rc::new(Cell::new(0));
        let changed = Rc::clone(&effects);
        let mut future = Box::pin(async move {
            let guard = DropFlag(observed);
            changed.set(1);
            YieldOnce { yielded: false }.await;
            changed.set(2);
            drop(guard);
        });
        let count = Arc::new(CountWake(AtomicUsize::new(0)));
        let waker = Waker::from(count);
        let mut context = Context::from_waker(&waker);
        assert_eq!(future.as_mut().poll(&mut context), Poll::Pending);
        assert_eq!(effects.get(), 1);
        assert!(!dropped.get());
        drop(future); // Drop the owning pinned box, not merely a borrowed Pin.
        assert!(dropped.get());
        assert_eq!(effects.get(), 1);
    }

    #[test]
    fn local_borrow_and_non_send_state_work_on_this_executor() {
        let label = String::from("Pi 4B");
        let count = Rc::new(Cell::new(0));
        let future = async {
            count.set(1);
            YieldOnce { yielded: false }.await;
            label.len()
        };
        assert_eq!(block_on(future), 5);
        assert_eq!(label, "Pi 4B");
        assert_eq!(count.get(), 1);
    }

    #[test]
    fn async_closure_lends_mutable_capture_to_sequential_calls() {
        async fn call_twice(mut callback: impl std::ops::AsyncFnMut() -> usize) -> [usize; 2] {
            [callback().await, callback().await]
        }
        let mut calls = 0;
        let callback = async || {
            YieldOnce { yielded: false }.await;
            calls += 1;
            calls
        };
        assert_eq!(block_on(call_twice(callback)), [1, 2]);
        assert_eq!(calls, 2);
    }
}
