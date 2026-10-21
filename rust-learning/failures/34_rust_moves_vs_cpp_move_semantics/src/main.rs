fn extract(owner: &mut Box<i32>) -> Box<i32> {
    *owner
}

fn main() {
    let mut owner = Box::new(46700);
    let _ = extract(&mut owner);
}
