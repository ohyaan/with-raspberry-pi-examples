fn make(minimum: i32) -> fn(i32) -> bool {
    move |reading| reading >= minimum
}

fn main() {
    let _ = make(0);
}
