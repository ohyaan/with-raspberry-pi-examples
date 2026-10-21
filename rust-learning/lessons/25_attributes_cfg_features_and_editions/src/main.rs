#![forbid(unsafe_code)]

fn main() {
    println!("platform={}", rust_config::PLATFORM);
    println!("detailed={}", cfg!(feature = "detailed"));
    println!("{}", rust_config::render(46_700).unwrap());
}
