async fn reading() -> i32 {
    46_700
}

fn main() {
    let value = reading().await;
    println!("{value}");
}
