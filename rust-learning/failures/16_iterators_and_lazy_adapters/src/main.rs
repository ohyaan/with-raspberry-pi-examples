fn main() {
    let readings = vec![46_700, 60_000, 0];
    let total: i32 = readings.into_iter().sum();
    println!("sum={total}, length={}", readings.len());
}
