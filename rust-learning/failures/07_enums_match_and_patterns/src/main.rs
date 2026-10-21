enum State {
    Ready,
    Missing,
}

fn label(state: State) -> &'static str {
    match state {
        State::Ready => "ready",
    }
}

fn main() {
    println!("{}", label(State::Ready));
}
