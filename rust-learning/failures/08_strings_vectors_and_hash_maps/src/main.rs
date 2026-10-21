fn main() {
    let mut readings = vec![46_700, 60_000];
    let first = &readings[0];
    readings.push(63_200);
    println!("{first}"); // Shared access overlaps the mutable push.
}
