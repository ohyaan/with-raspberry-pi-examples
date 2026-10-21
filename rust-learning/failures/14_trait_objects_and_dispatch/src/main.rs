trait Read {
    fn read(&self) -> Option<i32>;
}

struct Fixed(i32);
impl Read for Fixed {
    fn read(&self) -> Option<i32> {
        Some(self.0)
    }
}

struct Unavailable;
impl Read for Unavailable {
    fn read(&self) -> Option<i32> {
        None
    }
}

fn choose(available: bool) -> impl Read {
    if available {
        Fixed(46_700)
    } else {
        Unavailable
    }
}

fn main() {
    println!("{:?}", choose(true).read());
}
