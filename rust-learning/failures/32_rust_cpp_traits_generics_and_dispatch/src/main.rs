trait Source {
    fn read(&self) -> Option<i32>;
}
struct Fixed;
struct Missing;
impl Source for Fixed {
    fn read(&self) -> Option<i32> {
        Some(0)
    }
}
impl Source for Missing {
    fn read(&self) -> Option<i32> {
        None
    }
}
fn choose(present: bool) -> impl Source {
    if present { Fixed } else { Missing }
}
fn main() {
    let _ = choose(true);
}
