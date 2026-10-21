use pi_modules::parse_reading;

fn main() {
    for input in ["46700", "", "bad"] {
        match parse_reading(input) {
            Ok(Some(value)) => println!("measured={value}"),
            Ok(None) => println!("missing"),
            Err(_error) => println!("invalid"),
        }
    }
}
