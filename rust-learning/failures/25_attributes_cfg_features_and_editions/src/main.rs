fn main() {
    if cfg!(feature = "detailed") {
        println!("{}", rust_config::detail(0));
    }
}
