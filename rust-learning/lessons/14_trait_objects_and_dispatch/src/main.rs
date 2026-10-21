trait Read {
    type Value;
    fn read(&self) -> Option<Self::Value>;
}

struct Fixed(i32);

impl Read for Fixed {
    type Value = i32;

    fn read(&self) -> Option<Self::Value> {
        Some(self.0)
    }
}

struct Unavailable;

impl Read for Unavailable {
    type Value = i32;

    fn read(&self) -> Option<Self::Value> {
        None
    }
}

struct Borrowed<'a>(&'a i32);

impl Read for Borrowed<'_> {
    type Value = i32;

    fn read(&self) -> Option<Self::Value> {
        Some(*self.0)
    }
}

fn read_generic<T: Read<Value = i32> + ?Sized>(source: &T) -> Option<i32> {
    source.read()
}

fn read_impl(source: &impl Read<Value = i32>) -> Option<i32> {
    source.read()
}

fn read_dynamic(source: &dyn Read<Value = i32>) -> Option<i32> {
    source.read()
}

fn make_fixed(value: i32) -> impl Read<Value = i32> {
    Fixed(value)
}

// The Box owns the Borrowed wrapper, not the integer it points to.
fn boxed_borrow<'a>(value: &'a i32) -> Box<dyn Read<Value = i32> + 'a> {
    Box::new(Borrowed(value))
}

fn main() {
    let fixed = Fixed(46_700);
    let unavailable = Unavailable;
    println!("generic={:?}", read_generic(&fixed));
    println!("impl argument={:?}", read_impl(&fixed));
    println!("dynamic={:?}", read_dynamic(&fixed));

    // No Box is needed to borrow differently typed, existing values.
    let sources: [&dyn Read<Value = i32>; 2] = [&fixed, &unavailable];
    for source in sources {
        println!("mixed={:?}", read_dynamic(source));
    }

    let hidden = make_fixed(0);
    println!("opaque return={:?}", read_generic(&hidden));

    let value = -500;
    let boxed = boxed_borrow(&value);
    println!("boxed borrow={:?}", read_dynamic(boxed.as_ref()));
}

#[cfg(test)]
mod tests {
    use super::{Fixed, Read, Unavailable, boxed_borrow, make_fixed};
    use super::{read_dynamic, read_generic, read_impl};

    #[test]
    fn concrete_calls_agree_for_a_present_value() {
        let source = Fixed(46_700);
        assert_eq!(read_generic(&source), Some(46_700));
        assert_eq!(read_impl(&source), Some(46_700));
        assert_eq!(read_dynamic(&source), Some(46_700));
    }

    #[test]
    fn concrete_calls_agree_for_absence() {
        let source = Unavailable;
        assert_eq!(read_generic(&source), None);
        assert_eq!(read_impl(&source), None);
        assert_eq!(read_dynamic(&source), None);
    }

    #[test]
    fn mixed_objects_preserve_each_implementation() {
        let fixed = Fixed(0);
        let unavailable = Unavailable;
        let sources: [&dyn Read<Value = i32>; 2] = [&fixed, &unavailable];
        assert_eq!(read_dynamic(sources[0]), Some(0));
        assert_eq!(read_dynamic(sources[1]), None);
    }

    #[test]
    fn relaxed_sized_bound_accepts_an_object() {
        let fixed = Fixed(7);
        let object: &dyn Read<Value = i32> = &fixed;
        assert_eq!(read_generic(object), Some(7));
    }

    #[test]
    fn opaque_return_exposes_the_promised_trait() {
        let source = make_fixed(0);
        assert_eq!(source.read(), Some(0));
    }

    #[test]
    fn boxed_object_can_borrow_local_data() {
        let value = -500;
        let source = boxed_borrow(&value);
        assert_eq!(source.read(), Some(-500));
    }

    #[test]
    fn borrowed_object_does_not_consume_the_integer() {
        let value = 60_000;
        {
            let source = boxed_borrow(&value);
            assert_eq!(source.read(), Some(60_000));
        }
        assert_eq!(value, 60_000);
    }
}
