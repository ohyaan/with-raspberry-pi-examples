use std::sync::mpsc;

fn main() {
    let (sender, receiver) = mpsc::channel();
    let label = String::from("Pi 4B");
    sender.send(label).unwrap();
    println!("{label}");
    drop(receiver);
}
