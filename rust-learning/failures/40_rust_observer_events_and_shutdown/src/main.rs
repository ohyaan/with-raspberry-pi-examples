fn register<F: FnMut(i32) + 'static>(mut callback: F) {
    callback(0);
}
fn main() {
    let label = String::from("Pi 4B");
    register(|_| assert_eq!(label, "Pi 4B"));
}
