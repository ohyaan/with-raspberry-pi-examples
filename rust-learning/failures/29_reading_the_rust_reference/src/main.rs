struct Milli(i32);
struct Celsius(i32);

fn accept_milli(value: Milli) -> i32 {
    value.0
}

fn main() {
    let degrees = Celsius(46);
    let _ = accept_milli(degrees);
}
