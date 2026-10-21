fn parse(text: &str) -> i32 {
    let value = text.parse::<i32>()?;
    value
}

fn main() {
    let _ = parse("0");
}
