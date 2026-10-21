use std::rc::Rc;
use std::thread;

fn main() {
    let label = Rc::new(String::from("Pi 4B"));
    let worker = thread::spawn(move || label.len());
    println!("{}", worker.join().unwrap());
}
