fn main() {
    let mut label = String::from("Pi");
    let shared = &label;
    label.push_str(" online"); // Error: mutable access overlaps shared access.
    println!("{shared}");
}
