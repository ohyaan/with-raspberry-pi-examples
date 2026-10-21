fn main() {
    let label = String::from("Pi 4B");
    std::mem::drop(label);
    println!("{label}");
}
