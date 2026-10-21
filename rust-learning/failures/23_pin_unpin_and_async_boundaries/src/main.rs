use std::marker::PhantomPinned;
use std::pin::Pin;

struct StableLabel {
    _pinned: PhantomPinned,
}

fn main() {
    let mut label = StableLabel {
        _pinned: PhantomPinned,
    };
    let _borrow = Pin::new(&mut label);
}
