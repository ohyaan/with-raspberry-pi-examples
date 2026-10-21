fn temporary_label<'a>() -> &'a str {
    let label = String::from("Pi 4B");
    &label
}

fn main() {
    println!("{}", temporary_label());
}
