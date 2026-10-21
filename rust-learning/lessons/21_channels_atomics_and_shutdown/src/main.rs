use std::sync::Arc;
use std::sync::atomic::{AtomicBool, AtomicI32, AtomicUsize, Ordering};
use std::sync::mpsc::{self, Receiver};
use std::thread;

#[derive(Debug, PartialEq, Eq)]
struct Collected {
    readings: Vec<(usize, Option<i32>)>,
    sent: usize,
}

fn collect_readings(
    batches: Vec<Vec<(usize, Option<i32>)>>,
    capacity: usize,
) -> Result<Collected, &'static str> {
    let (sender, receiver) = mpsc::sync_channel(capacity);
    let sent = Arc::new(AtomicUsize::new(0));
    let handles: Vec<_> = batches
        .into_iter()
        .map(|batch| {
            let sender = sender.clone();
            let counter = Arc::clone(&sent);
            thread::spawn(move || -> Result<(), &'static str> {
                for reading in batch {
                    sender.send(reading).map_err(|_| "receiver closed")?;
                    // This is a statistic, not a payload-publication signal.
                    counter.fetch_add(1, Ordering::Relaxed);
                }
                Ok(())
            })
        })
        .collect();
    drop(sender); // Do not retain a sender that prevents end-of-stream.

    // Drain before joining: bounded producers may be waiting for space.
    let mut readings: Vec<_> = receiver.into_iter().collect();
    let mut first_error = None;
    for handle in handles {
        let result = match handle.join() {
            Ok(result) => result,
            Err(_) => Err("producer panicked"),
        };
        if let Err(error) = result {
            first_error.get_or_insert(error);
        }
    }
    if let Some(error) = first_error {
        return Err(error);
    }
    readings.sort_by_key(|(index, _)| *index);
    Ok(Collected {
        readings,
        sent: sent.load(Ordering::Relaxed),
    })
}

enum Command {
    Record(i32),
    Stop,
}

fn command_worker(receiver: Receiver<Command>) -> Vec<i32> {
    let mut values = Vec::new();
    for command in receiver {
        match command {
            Command::Record(value) => values.push(value),
            Command::Stop => break,
        }
    }
    values
}

fn published_reading() -> i32 {
    let payload = Arc::new(AtomicI32::new(0));
    let ready = Arc::new(AtomicBool::new(false));
    let worker_payload = Arc::clone(&payload);
    let worker_ready = Arc::clone(&ready);
    let worker = thread::spawn(move || {
        worker_payload.store(46_700, Ordering::Relaxed);
        worker_ready.store(true, Ordering::Release);
    });
    // A tiny one-shot ordering demonstration, not a production waiting API.
    while !ready.load(Ordering::Acquire) {
        thread::yield_now();
    }
    let value = payload.load(Ordering::Relaxed);
    worker.join().expect("publication fixture panicked");
    value
}

fn main() {
    let batches = vec![vec![(0, Some(46_700)), (1, None)], vec![(2, Some(0))]];
    let report = collect_readings(batches, 1).expect("collection failed");
    println!("readings={:?}", report.readings);
    println!("sent={}", report.sent);

    let (sender, receiver) = mpsc::channel();
    let worker = thread::spawn(move || command_worker(receiver));
    sender.send(Command::Record(0)).unwrap();
    sender.send(Command::Record(-500)).unwrap();
    sender.send(Command::Stop).unwrap();
    drop(sender);
    println!("commands={:?}", worker.join().unwrap());
    println!("published={}", published_reading());
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::mpsc::{TryRecvError, TrySendError};

    #[test]
    fn bounded_collection_preserves_missing_zero_and_negative_values() {
        let batches = vec![vec![(2, Some(-500))], vec![(0, None), (1, Some(0))]];
        assert_eq!(
            collect_readings(batches, 1),
            Ok(Collected {
                readings: vec![(0, None), (1, Some(0)), (2, Some(-500))],
                sent: 3,
            })
        );
    }

    #[test]
    fn empty_and_rendezvous_collection_terminate() {
        assert_eq!(collect_readings(vec![], 0).unwrap().sent, 0);
        let report = collect_readings(vec![vec![], vec![(0, Some(0))]], 0).unwrap();
        assert_eq!(report.readings, vec![(0, Some(0))]);
        assert_eq!(report.sent, 1);
    }

    #[test]
    fn empty_is_not_disconnected_until_every_sender_is_dropped() {
        let (sender, receiver) = mpsc::channel::<i32>();
        let retained = sender.clone();
        drop(sender);
        assert_eq!(receiver.try_recv(), Err(TryRecvError::Empty));
        drop(retained);
        assert_eq!(receiver.try_recv(), Err(TryRecvError::Disconnected));
    }

    #[test]
    fn buffered_values_are_drained_before_disconnection() {
        let (sender, receiver) = mpsc::channel();
        sender.send(0).unwrap();
        drop(sender);
        assert_eq!(receiver.recv(), Ok(0));
        assert!(receiver.recv().is_err());
    }

    #[test]
    fn bounded_try_send_reports_full_and_returns_the_value() {
        let (sender, receiver) = mpsc::sync_channel(1);
        sender.send(String::from("first")).unwrap();
        let error = sender.try_send(String::from("second")).unwrap_err();
        assert!(matches!(error, TrySendError::Full(ref value) if value == "second"));
        assert_eq!(receiver.recv().unwrap(), "first");
        drop(receiver);
        let error = sender.send(String::from("third")).unwrap_err();
        assert_eq!(error.0, "third");
    }

    #[test]
    fn stop_and_disconnection_have_explicit_drain_policies() {
        let (sender, receiver) = mpsc::channel();
        sender.send(Command::Record(0)).unwrap();
        sender.send(Command::Stop).unwrap();
        sender.send(Command::Record(7)).unwrap();
        drop(sender);
        assert_eq!(command_worker(receiver), vec![0]);

        let (sender, receiver) = mpsc::channel();
        sender.send(Command::Record(7)).unwrap();
        drop(sender);
        assert_eq!(command_worker(receiver), vec![7]);
    }

    #[test]
    fn atomic_fetch_add_keeps_all_joined_increments() {
        let counter = Arc::new(AtomicUsize::new(0));
        let handles: Vec<_> = (0..4)
            .map(|_| {
                let counter = Arc::clone(&counter);
                thread::spawn(move || {
                    for _ in 0..250 {
                        counter.fetch_add(1, Ordering::Relaxed);
                    }
                })
            })
            .collect();
        for handle in handles {
            handle.join().unwrap();
        }
        assert_eq!(counter.load(Ordering::Relaxed), 1_000);
    }

    #[test]
    fn release_acquire_one_shot_publication_observes_payload() {
        assert_eq!(published_reading(), 46_700);
    }

    #[test]
    fn compare_exchange_returns_the_observed_previous_value() {
        let state = AtomicUsize::new(0);
        assert_eq!(
            state.compare_exchange(0, 1, Ordering::AcqRel, Ordering::Acquire),
            Ok(0)
        );
        assert_eq!(
            state.compare_exchange(0, 2, Ordering::AcqRel, Ordering::Acquire),
            Err(1)
        );
        assert_eq!(state.load(Ordering::Relaxed), 1);
    }
}
