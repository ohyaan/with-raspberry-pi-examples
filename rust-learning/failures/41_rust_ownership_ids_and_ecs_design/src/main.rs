struct World {
    values: Vec<i32>,
}
fn main() {
    let world = World { values: vec![0] };
    let transferred = world;
    assert_eq!(world.values, transferred.values);
}
