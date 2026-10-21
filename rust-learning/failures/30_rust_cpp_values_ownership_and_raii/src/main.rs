struct Owner(String);

fn main() {
    let first = Owner(String::from("lease"));
    let second = first;
    println!("{}", second.0);
    println!("{}", first.0);
}
