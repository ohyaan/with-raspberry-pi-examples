use std::ops::Deref;

enum List {
    Nil,
    Cons(i32, Box<List>),
}

impl List {
    fn len(&self) -> usize {
        match self {
            Self::Nil => 0,
            Self::Cons(_, tail) => 1 + tail.len(),
        }
    }

    fn sum(&self) -> i64 {
        match self {
            Self::Nil => 0,
            Self::Cons(value, tail) => i64::from(*value) + tail.sum(),
        }
    }
}

struct Label(String);

impl Deref for Label {
    type Target = str;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl Drop for Label {
    fn drop(&mut self) {
        // A visible teaching hook, not production I/O cleanup.
        println!("drop={}", self.0);
    }
}

fn byte_length(label: &str) -> usize {
    label.len()
}

fn main() {
    let value = Box::new(46_700);
    println!("boxed={}", *value);

    let readings = List::Cons(46_700, Box::new(List::Cons(0, Box::new(List::Nil))));
    println!("list length={}, sum={}", readings.len(), readings.sum());

    let text: Box<str> = String::from("Pi 4B").into_boxed_str();
    println!("boxed text bytes={}", byte_length(&text));

    {
        let first = Label(String::from("first"));
        let second = Label(String::from("second"));
        println!("label bytes={}", byte_length(&first));
        std::mem::drop(first);
        println!("after early drop");
        println!("second bytes={}", second.len());
    }
    println!("after scope");

    {
        let _outer = Label(String::from("outer"));
        let _inner = Label(String::from("inner"));
        println!("both alive");
    }
    println!("finished");
}

#[cfg(test)]
mod tests {
    use super::{Label, List, byte_length};

    struct Marker<'a>(&'a mut bool);

    impl Drop for Marker<'_> {
        fn drop(&mut self) {
            *self.0 = true;
        }
    }

    #[test]
    fn box_can_transfer_an_owned_non_copy_value() {
        let boxed = Box::new(String::from("Pi 4B"));
        let owned: String = *boxed;
        assert_eq!(owned, "Pi 4B");
    }

    #[test]
    fn empty_recursive_value_has_zero_length_and_sum() {
        assert_eq!(List::Nil.len(), 0);
        assert_eq!(List::Nil.sum(), 0);
    }

    #[test]
    fn recursive_fixture_preserves_zero_and_negative_values() {
        let readings = List::Cons(-500, Box::new(List::Cons(0, Box::new(List::Nil))));
        assert_eq!(readings.len(), 2);
        assert_eq!(readings.sum(), -500);
    }

    #[test]
    fn deref_coercion_borrows_utf8_text() {
        let label = Label(String::from("π"));
        assert_eq!(byte_length(&label), 2);
        assert_eq!(label.len(), 2);
        assert_eq!(&*label, "π");
    }

    #[test]
    fn boxed_unsized_text_can_be_borrowed() {
        let label: Box<str> = String::from("Pi 4B").into_boxed_str();
        assert_eq!(byte_length(&label), 5);
    }

    #[test]
    fn leaving_scope_invokes_the_destructor() {
        let mut dropped = false;
        {
            let _marker = Marker(&mut dropped);
        }
        assert!(dropped);
    }

    #[test]
    fn consuming_drop_invokes_the_destructor_early() {
        let mut dropped = false;
        let marker = Marker(&mut dropped);
        std::mem::drop(marker);
        assert!(dropped);
    }
}
