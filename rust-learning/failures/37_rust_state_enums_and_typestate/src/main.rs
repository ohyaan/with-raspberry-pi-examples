struct Idle;
struct Ready;
struct Session<S> {
    state: S,
}

impl Session<Ready> {
    fn send(&mut self, _reading: i32) {}
}

fn main() {
    let mut session = Session { state: Idle };
    session.send(0);
}
