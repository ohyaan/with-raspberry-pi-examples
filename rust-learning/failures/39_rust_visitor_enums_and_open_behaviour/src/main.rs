enum Item {
    Value(i32),
    Missing,
    Pending,
}
fn score(item: &Item) -> i64 {
    match item {
        Item::Value(value) => i64::from(*value),
        Item::Missing => 0,
    }
}
fn main() {
    let _ = score(&Item::Pending);
}
