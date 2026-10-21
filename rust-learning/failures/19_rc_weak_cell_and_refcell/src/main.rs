use std::rc::Rc;

fn main() {
    let mut label = Rc::new(String::from("Pi"));
    let _alias = Rc::clone(&label);
    label.push_str(" 4B");
}
