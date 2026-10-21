use fixture_macros::{FixtureName, add_debug, fixture_value};
use rust_macros as renamed;
use rust_macros::FixtureName;

#[derive(FixtureName)]
struct Probe;

#[add_debug]
struct State;

#[test]
fn crate_path_survives_caller_alias_and_shadowed_helper() {
    fn checked_reading(_: i32) -> Option<i32> {
        None
    }
    assert_eq!(checked_reading(0), None);
    assert_eq!(renamed::bounded_reading!(0), Some(0));
}

#[test]
fn derive_adds_an_implementation_without_removing_type() {
    let _original_type_still_exists = Probe;
    assert_eq!(Probe::fixture_name(), "Probe");
}

#[test]
fn attribute_retains_item_and_function_macro_emits_typed_expression() {
    assert_eq!(format!("{:?}", State), "State");
    let value: i32 = fixture_value!();
    assert_eq!(value, 46_700);
}
