use fixture_macros::{FixtureName, add_debug, fixture_value};
use rust_macros as renamed;
use rust_macros::FixtureName;

#[derive(FixtureName)]
struct Pi;

#[add_debug]
struct Status {
    reading: i32,
}

fn main() {
    let values = renamed::readings!(fixture_value!(), 0, -500,);
    println!("readings={values:?}");
    println!("checked={:?}", renamed::bounded_reading!(0));
    println!("name={}", Pi::fixture_name());
    let status = Status {
        reading: fixture_value!(),
    };
    assert_eq!(status.reading, 46_700);
    println!("debug={status:?}");
}
