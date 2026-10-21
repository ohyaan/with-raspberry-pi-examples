struct Reading {
    label: String,
}

impl Reading {
    fn into_label(self) -> String {
        self.label
    }
}

fn main() {
    let reading = Reading { label: String::from("Pi") };
    let label = reading.into_label();
    println!("{}", reading.label); // Error: Reading was consumed.
    println!("{label}");
}
