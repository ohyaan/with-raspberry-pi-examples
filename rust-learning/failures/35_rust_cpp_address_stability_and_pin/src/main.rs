use std::marker::PhantomPinned;

struct Sample {
    _pin: PhantomPinned,
}

fn main() {
    let mut owner = Box::pin(Sample { _pin: PhantomPinned });
    let _unrestricted = owner.as_mut().get_mut();
}
