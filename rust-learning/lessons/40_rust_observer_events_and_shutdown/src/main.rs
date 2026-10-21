use std::cell::RefCell;
use std::rc::Rc;

#[derive(Debug, PartialEq, Eq)]
enum Action {
    Keep,
    Remove,
}
#[derive(Debug, PartialEq, Eq)]
enum Error {
    Closed,
    Exhausted,
}

struct Entry {
    id: u64,
    callback: Box<dyn FnMut(i32) -> Action>,
}
struct Bus {
    entries: Vec<Entry>,
    next: u64,
    closed: bool,
}
impl Bus {
    fn new() -> Self {
        Self {
            entries: Vec::new(),
            next: 1,
            closed: false,
        }
    }
    fn subscribe<F>(&mut self, callback: F) -> Result<u64, Error>
    where
        F: FnMut(i32) -> Action + 'static,
    {
        if self.closed {
            return Err(Error::Closed);
        }
        let next = self.next.checked_add(1).ok_or(Error::Exhausted)?;
        let id = self.next;
        self.entries.push(Entry {
            id,
            callback: Box::new(callback),
        });
        self.next = next;
        Ok(id)
    }
    fn unsubscribe(&mut self, id: u64) -> bool {
        let before = self.entries.len();
        self.entries.retain(|entry| entry.id != id);
        before != self.entries.len()
    }
    fn publish(&mut self, reading: i32) -> Result<usize, Error> {
        if self.closed {
            return Err(Error::Closed);
        }
        let mut delivered = 0;
        self.entries.retain_mut(|entry| {
            delivered += 1;
            (entry.callback)(reading) == Action::Keep
        });
        Ok(delivered)
    }
    fn close(&mut self) -> bool {
        if self.closed {
            return false;
        }
        self.closed = true;
        self.entries.clear();
        true
    }
}

fn main() {
    let log = Rc::new(RefCell::new(Vec::<String>::new()));
    let mut bus = Bus::new();
    let a_log = Rc::clone(&log);
    let a = bus
        .subscribe(move |value| {
            a_log.borrow_mut().push(format!("A:{value}"));
            Action::Keep
        })
        .unwrap();
    let b_log = Rc::clone(&log);
    let b = bus
        .subscribe(move |value| {
            b_log.borrow_mut().push(format!("B:{value}"));
            Action::Remove
        })
        .unwrap();
    let c_log = Rc::clone(&log);
    let c = bus
        .subscribe(move |value| {
            c_log.borrow_mut().push(format!("C:{value}"));
            Action::Keep
        })
        .unwrap();
    let first = bus.publish(0).unwrap();
    assert_eq!(first, 3);
    assert!(!bus.unsubscribe(b));
    assert!(bus.unsubscribe(a));
    assert!(!bus.unsubscribe(a));
    let second = bus.publish(-500).unwrap();
    assert_eq!(second, 1);
    println!("first={first}, second={second}");
    println!("log={}", log.borrow().join("|"));
    let closed = bus.close();
    let repeated = bus.close();
    assert!(closed && !repeated && !bus.unsubscribe(c));
    assert_eq!(bus.publish(46700), Err(Error::Closed));
    assert_eq!(bus.subscribe(|_| Action::Keep), Err(Error::Closed));
    assert_eq!(log.borrow().len(), 4);
    println!(
        "closed={}, repeated={}",
        u8::from(closed),
        u8::from(repeated)
    );
    println!("after close=Closed");
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::Cell;

    #[test]
    fn self_removal_preserves_order_and_remaining_delivery() {
        let log = Rc::new(RefCell::new(Vec::new()));
        let mut bus = Bus::new();
        for (label, action) in [
            ("A", Action::Keep),
            ("B", Action::Remove),
            ("C", Action::Keep),
        ] {
            let sink = Rc::clone(&log);
            bus.subscribe(move |value| {
                sink.borrow_mut().push(format!("{label}:{value}"));
                if action == Action::Remove {
                    Action::Remove
                } else {
                    Action::Keep
                }
            })
            .unwrap();
        }
        assert_eq!(bus.publish(0), Ok(3));
        assert_eq!(bus.publish(-500), Ok(2));
        assert_eq!(*log.borrow(), ["A:0", "B:0", "C:0", "A:-500", "C:-500"]);
    }

    #[test]
    fn explicit_unsubscribe_prevents_calls_and_does_not_reuse_ids() {
        let mut bus = Bus::new();
        let removed = bus.subscribe(|_| panic!("removed callback ran")).unwrap();
        assert!(bus.unsubscribe(removed));
        assert!(!bus.unsubscribe(removed));
        let replacement = bus.subscribe(|_| Action::Keep).unwrap();
        assert_ne!(removed, replacement);
        assert!(!bus.unsubscribe(removed));
        assert_eq!(bus.publish(0), Ok(1));
    }

    #[test]
    fn shutdown_rejects_new_work_without_delivering_a_final_event() {
        let calls = Rc::new(Cell::new(0));
        let seen = Rc::clone(&calls);
        let mut bus = Bus::new();
        bus.subscribe(move |_| {
            seen.set(seen.get() + 1);
            Action::Keep
        })
        .unwrap();
        assert!(bus.close());
        assert!(!bus.close());
        assert_eq!(bus.publish(0), Err(Error::Closed));
        assert_eq!(bus.subscribe(|_| Action::Keep), Err(Error::Closed));
        assert_eq!(calls.get(), 0);
    }

    struct DropProbe(Rc<Cell<usize>>);
    impl Drop for DropProbe {
        fn drop(&mut self) {
            self.0.set(self.0.get() + 1);
        }
    }

    #[test]
    fn removal_and_close_release_owned_callback_state() {
        let drops = Rc::new(Cell::new(0));
        let mut bus = Bus::new();
        let first = DropProbe(Rc::clone(&drops));
        let id = bus
            .subscribe(move |_| {
                let _alive = &first;
                Action::Keep
            })
            .unwrap();
        assert_eq!(drops.get(), 0);
        assert!(bus.unsubscribe(id));
        assert_eq!(drops.get(), 1);
        let second = DropProbe(Rc::clone(&drops));
        bus.subscribe(move |_| {
            let _alive = &second;
            Action::Keep
        })
        .unwrap();
        assert!(bus.close());
        assert_eq!(drops.get(), 2);
    }

    #[test]
    fn empty_delivery_and_id_exhaustion_are_explicit() {
        let mut bus = Bus::new();
        assert_eq!(bus.publish(0), Ok(0));
        bus.next = u64::MAX - 1;
        assert_eq!(bus.subscribe(|_| Action::Keep), Ok(u64::MAX - 1));
        assert_eq!(bus.subscribe(|_| Action::Keep), Err(Error::Exhausted));
        assert_eq!(bus.publish(i32::MIN), Ok(1));
    }

    #[test]
    fn discarding_id_keeps_subscription_until_bus_drop() {
        let drops = Rc::new(Cell::new(0));
        let calls = Rc::new(Cell::new(0));
        {
            let mut bus = Bus::new();
            let owned = DropProbe(Rc::clone(&drops));
            let seen = Rc::clone(&calls);
            bus.subscribe(move |_| {
                let _alive = &owned;
                seen.set(seen.get() + 1);
                Action::Keep
            })
            .unwrap(); // Do not retain the returned ID.
            assert_eq!(bus.publish(0), Ok(1));
            assert_eq!(drops.get(), 0);
        }
        assert_eq!(drops.get(), 1);
        assert_eq!(calls.get(), 1); // Destruction did not send another event.
    }
}
