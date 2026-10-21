mod config {
    pub struct Config {
        period: u32,
    }
}

fn main() {
    let _invalid = config::Config { period: 0 };
}
