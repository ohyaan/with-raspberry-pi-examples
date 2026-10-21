fn larger<T>(left: T, right: T) -> T {
    if left > right { left } else { right }
}

fn main() {
    println!("{}", larger(46_700, 60_000));
}
