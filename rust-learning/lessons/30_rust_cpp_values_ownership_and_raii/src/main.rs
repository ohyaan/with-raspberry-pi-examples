use std::cell::Cell;

#[derive(Default)]
struct Trace {
    opened: Cell<usize>,
    closed: Cell<usize>,
}

struct Lease<'a> {
    trace: &'a Trace,
}

impl<'a> Lease<'a> {
    fn new(trace: &'a Trace) -> Self {
        trace.opened.set(trace.opened.get() + 1);
        Self { trace }
    }
}

impl Drop for Lease<'_> {
    fn drop(&mut self) {
        self.trace.closed.set(self.trace.closed.get() + 1);
    }
}

fn run(trace: &Trace, reading: Option<i32>) -> Result<i32, &'static str> {
    let _lease = Lease::new(trace);
    let value = reading.ok_or("missing")?;
    Ok(value)
}

fn main() {
    let trace = Trace::default();
    let normal = run(&trace, Some(46_700)).expect("present fixture");
    let missing = run(&trace, None).is_err();
    assert_eq!(trace.opened.get(), 2);
    assert_eq!(trace.closed.get(), 2);
    println!("normal={normal}");
    println!("missing={missing}");
    println!(
        "opened={}, closed={}",
        trace.opened.get(),
        trace.closed.get()
    );
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn success_releases_once() {
        let trace = Trace::default();
        assert_eq!(run(&trace, Some(46_700)), Ok(46_700));
        assert_eq!((trace.opened.get(), trace.closed.get()), (1, 1));
    }

    #[test]
    fn missing_releases_once() {
        let trace = Trace::default();
        assert_eq!(run(&trace, None), Err("missing"));
        assert_eq!((trace.opened.get(), trace.closed.get()), (1, 1));
    }

    #[test]
    fn zero_and_negative_are_present() {
        let trace = Trace::default();
        assert_eq!(run(&trace, Some(0)), Ok(0));
        assert_eq!(run(&trace, Some(-500)), Ok(-500));
        assert_eq!(trace.closed.get(), 2);
    }

    #[test]
    fn explicit_drop_consumes_the_owner() {
        let trace = Trace::default();
        let guard = Lease::new(&trace);
        assert_eq!(trace.closed.get(), 0);
        drop(guard);
        assert_eq!(trace.closed.get(), 1);
    }
}
