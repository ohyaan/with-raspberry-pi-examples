use std::cell::{Cell, RefCell};
use std::rc::{Rc, Weak};

struct Device {
    label: String,
    visits: Cell<u32>,
    readings: RefCell<Vec<i32>>,
    parent: RefCell<Weak<Device>>,
    children: RefCell<Vec<Rc<Device>>>,
}

impl Device {
    fn new(label: &str) -> Rc<Self> {
        Rc::new(Self {
            label: String::from(label),
            visits: Cell::new(0),
            readings: RefCell::new(Vec::new()),
            parent: RefCell::new(Weak::new()),
            children: RefCell::new(Vec::new()),
        })
    }

    fn record(&self, value: i32) {
        self.readings.borrow_mut().push(value);
    }

    fn visit(&self) {
        // This tiny fixture does not reach the u32 limit.
        self.visits.set(self.visits.get() + 1);
    }
}

fn main() {
    let device = Device::new("Pi 4B");
    let observer = Rc::clone(&device);
    let weak = Rc::downgrade(&device);
    println!("same allocation={}", Rc::ptr_eq(&device, &observer));
    println!("strong owners={}", Rc::strong_count(&device));
    observer.visit();
    observer.record(46_700);
    device.record(0);
    println!("visits={}", device.visits.get());

    let readings = device.readings.borrow();
    println!("readings={:?}", &*readings);
    println!(
        "write blocked={}",
        device.readings.try_borrow_mut().is_err()
    );
    drop(readings);
    device.record(-500);
    println!("after release={:?}", &*device.readings.borrow());

    drop(observer);
    println!("remaining owners={}", Rc::strong_count(&device));
    drop(device);
    println!("value gone={}", weak.upgrade().is_none());

    let root = Device::new("root");
    let child = Device::new("child");
    *child.parent.borrow_mut() = Rc::downgrade(&root);
    root.children.borrow_mut().push(Rc::clone(&child));
    {
        // Upgrade temporarily owns the parent. Release it before dropping root.
        let parent = child.parent.borrow().upgrade().expect("live fixture root");
        println!("parent={}", parent.label);
    }
    drop(root);
    println!("parent gone={}", child.parent.borrow().upgrade().is_none());
    println!("child survives={}", child.label);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn clone_shares_mutations_without_cloning_the_payload() {
        let device = Device::new("Pi 4B");
        let alias = Rc::clone(&device);
        assert!(Rc::ptr_eq(&device, &alias));
        alias.record(0);
        alias.record(-500);
        alias.visit();
        assert_eq!(&*device.readings.borrow(), &[0, -500]);
        assert_eq!(device.visits.get(), 1);
    }

    #[test]
    fn empty_and_shared_read_borrows_are_valid() {
        let readings = RefCell::new(Vec::<i32>::new());
        let first = readings.borrow();
        let second = readings.borrow();
        assert!(first.is_empty() && second.is_empty());
        assert!(readings.try_borrow_mut().is_err());
    }

    #[test]
    fn releasing_guards_restores_exclusive_access() {
        let readings = RefCell::new(vec![0]);
        let first = readings.borrow();
        let second = readings.borrow();
        drop(first);
        assert!(readings.try_borrow_mut().is_err());
        drop(second);
        let mut writer = readings.borrow_mut();
        writer.push(46_700);
        assert!(readings.try_borrow().is_err());
        assert!(readings.try_borrow_mut().is_err());
        drop(writer);
        assert_eq!(&*readings.borrow(), &[0, 46_700]);
    }

    #[test]
    fn weak_does_not_keep_the_value_alive() {
        let value = Rc::new(String::from("Pi 4B"));
        let weak = Rc::downgrade(&value);
        let upgraded = weak.upgrade().unwrap();
        drop(value);
        assert!(weak.upgrade().is_some());
        drop(upgraded);
        assert!(weak.upgrade().is_none());
    }

    #[test]
    fn weak_parent_allows_root_destruction_while_child_survives() {
        let root = Device::new("root");
        let child = Device::new("child");
        let root_watch = Rc::downgrade(&root);
        let child_watch = Rc::downgrade(&child);
        *child.parent.borrow_mut() = Rc::downgrade(&root);
        root.children.borrow_mut().push(Rc::clone(&child));
        drop(root);
        assert!(root_watch.upgrade().is_none());
        assert!(child.parent.borrow().upgrade().is_none());
        assert_eq!(child.label, "child");
        drop(child);
        assert!(child_watch.upgrade().is_none());
    }

    #[test]
    fn unique_rc_can_be_mutated_without_a_cell() {
        let mut value = Rc::new(String::from("Pi"));
        Rc::get_mut(&mut value).unwrap().push_str(" 4B");
        let alias = Rc::clone(&value);
        assert!(Rc::get_mut(&mut value).is_none());
        drop(alias);
        let weak = Rc::downgrade(&value);
        assert!(Rc::get_mut(&mut value).is_none());
        drop(weak);
        assert_eq!(Rc::get_mut(&mut value).unwrap(), "Pi 4B");
    }

    #[test]
    fn cell_can_replace_non_copy_owned_values() {
        let label = Cell::new(String::from("Pi"));
        assert_eq!(label.replace(String::from("Pi 4B")), "Pi");
        assert_eq!(label.take(), "Pi 4B");
        assert_eq!(label.into_inner(), "");
    }

    #[test]
    #[should_panic(expected = "already borrowed")]
    fn infallible_borrow_panics_on_conflict() {
        let readings = RefCell::new(vec![0]);
        let _reader = readings.borrow();
        let _writer = readings.borrow_mut();
    }
}
