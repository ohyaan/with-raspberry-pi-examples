use pi_report::{analyze, parse_reading};

fn main() {
    let threshold = 60_000;
    let summary = analyze(["46700", "60000", "", "bad", "125001", "0"], |value| {
        value >= threshold
    });
    println!(
        "measured={}, missing={}, invalid={}, matching={}",
        summary.measured, summary.missing, summary.invalid, summary.matching
    );
    if let Err(error) = parse_reading("125001") {
        println!("error={error}");
    }
}
