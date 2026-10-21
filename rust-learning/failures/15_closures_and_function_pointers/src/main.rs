fn main() {
    let label = String::from("Pi 4B");
    let finish = move || label;
    let first = finish();
    let second = finish();
    println!("{first}, {second}");
}
