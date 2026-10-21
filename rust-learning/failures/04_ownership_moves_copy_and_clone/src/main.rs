fn main() {
    let original = String::from("Pi 4B");
    let moved = original;
    println!("{original}"); // Error: use after move.
    println!("{moved}");
}
