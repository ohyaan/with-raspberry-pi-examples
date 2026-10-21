mod design {
    use std::rc::Rc;

    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    pub struct Body {
        pub x: i32,
        pub velocity: Option<i32>,
    }
    pub fn step_records(bodies: &mut [Body; 2]) {
        for body in bodies {
            if let Some(velocity) = body.velocity {
                body.x += velocity;
            }
        }
    }
    #[derive(Debug, Clone)]
    pub struct Handle {
        owner: Rc<()>,
        slot: usize,
        generation: u8,
    }
    impl Handle {
        pub fn slot(&self) -> usize {
            self.slot
        }
        pub fn generation(&self) -> u8 {
            self.generation
        }
    }
    #[derive(Clone, Copy, Default)]
    struct Slot {
        generation: u8,
        retired: bool,
    }
    pub struct World {
        owner: Rc<()>,
        slots: [Slot; 2],
        positions: [Option<i32>; 2],
        velocities: [Option<i32>; 2],
    }
    impl World {
        pub fn new() -> Self {
            Self {
                owner: Rc::new(()),
                slots: [Slot::default(); 2],
                positions: [None; 2],
                velocities: [None; 2],
            }
        }
        fn valid(&self, handle: &Handle) -> bool {
            Rc::ptr_eq(&self.owner, &handle.owner)
                && handle.slot < self.slots.len()
                && self.positions[handle.slot].is_some()
                && self.slots[handle.slot].generation == handle.generation
        }
        pub fn insert(&mut self, body: Body) -> Option<Handle> {
            for index in 0..self.slots.len() {
                if self.positions[index].is_none() && !self.slots[index].retired {
                    self.positions[index] = Some(body.x);
                    self.velocities[index] = body.velocity;
                    return Some(Handle {
                        owner: Rc::clone(&self.owner),
                        slot: index,
                        generation: self.slots[index].generation,
                    });
                }
            }
            None
        }
        pub fn inspect(&self, handle: &Handle) -> Option<Body> {
            if !self.valid(handle) {
                return None;
            }
            Some(Body {
                x: self.positions[handle.slot]?,
                velocity: self.velocities[handle.slot],
            })
        }
        pub fn set_velocity(&mut self, handle: &Handle, velocity: Option<i32>) -> bool {
            if !self.valid(handle) {
                return false;
            }
            self.velocities[handle.slot] = velocity;
            true
        }
        pub fn remove(&mut self, handle: &Handle) -> bool {
            if !self.valid(handle) {
                return false;
            }
            let index = handle.slot;
            self.positions[index] = None;
            self.velocities[index] = None;
            match self.slots[index].generation.checked_add(1) {
                Some(next) => self.slots[index].generation = next,
                None => self.slots[index].retired = true,
            }
            true
        }
        pub fn step(&mut self) {
            for index in 0..self.slots.len() {
                if let (Some(position), Some(velocity)) =
                    (&mut self.positions[index], self.velocities[index])
                {
                    *position += velocity;
                }
            }
        }
    }
}

use design::{Body, World, step_records};

fn main() {
    let mut records = [
        Body {
            x: 0,
            velocity: Some(1),
        },
        Body {
            x: 10,
            velocity: None,
        },
    ];
    let mut world = World::new();
    let a = world.insert(records[0]).unwrap();
    let b = world.insert(records[1]).unwrap();
    step_records(&mut records);
    world.step();
    assert_eq!(world.inspect(&a), Some(records[0]));
    assert_eq!(world.inspect(&b), Some(records[1]));
    assert!(world.set_velocity(&b, Some(-2)));
    records[1].velocity = Some(-2);
    step_records(&mut records);
    world.step();
    assert_eq!(world.inspect(&a), Some(records[0]));
    assert_eq!(world.inspect(&b), Some(records[1]));
    assert_eq!((records[0].x, records[1].x), (2, 8));
    println!("matched positions={},{}", records[0].x, records[1].x);
    let snapshot = world.inspect(&a).unwrap();
    let old = a.clone();
    assert!(world.remove(&a));
    assert!(!world.remove(&old));
    let replacement = world
        .insert(Body {
            x: 99,
            velocity: None,
        })
        .unwrap();
    let reused = old.slot() == replacement.slot();
    let stale = world.inspect(&old).is_none();
    assert!(reused && stale);
    assert_eq!(replacement.generation(), 1);
    assert!(!world.set_velocity(&old, Some(7)));
    world.step();
    assert_eq!(
        world.inspect(&replacement),
        Some(Body {
            x: 99,
            velocity: None
        })
    );
    assert_eq!(snapshot.x, 2);
    println!("reused={}, stale={}", u8::from(reused), u8::from(stale));
    let mut foreign = World::new();
    let collision = foreign
        .insert(Body {
            x: 7,
            velocity: Some(0),
        })
        .unwrap();
    assert_eq!(
        (collision.slot(), collision.generation()),
        (old.slot(), old.generation())
    );
    assert!(world.inspect(&collision).is_none() && foreign.inspect(&old).is_none());
    println!("foreign rejected=1");
    let mut limited = World::new();
    let mut ancient = None;
    for generation in 0..=u8::MAX {
        let current = limited
            .insert(Body {
                x: 0,
                velocity: Some(0),
            })
            .unwrap();
        if generation == 0 {
            ancient = Some(current.clone());
        }
        assert_eq!((current.slot(), current.generation()), (0, generation));
        assert!(limited.remove(&current));
    }
    let after_retirement = limited
        .insert(Body {
            x: 5,
            velocity: None,
        })
        .unwrap();
    assert_eq!(after_retirement.slot(), 1);
    assert!(limited.inspect(ancient.as_ref().unwrap()).is_none());
    assert!(
        limited
            .insert(Body {
                x: 0,
                velocity: Some(0)
            })
            .is_none()
    );
    println!("retired slot={}", after_retirement.slot());
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn records_and_components_match_across_capability_changes() {
        for velocity in [None, Some(0), Some(-2), Some(3)] {
            let mut records = [
                Body { x: 0, velocity },
                Body {
                    x: 10,
                    velocity: None,
                },
            ];
            let mut world = World::new();
            let a = world.insert(records[0]).unwrap();
            let b = world.insert(records[1]).unwrap();
            for changed in [None, Some(0), Some(-1)] {
                assert!(world.set_velocity(&b, changed));
                records[1].velocity = changed;
                step_records(&mut records);
                world.step();
                assert_eq!(world.inspect(&a), Some(records[0]));
                assert_eq!(world.inspect(&b), Some(records[1]));
            }
        }
    }

    #[test]
    fn stale_copies_cannot_resolve_or_modify_reused_slots() {
        let mut world = World::new();
        let first = world
            .insert(Body {
                x: 0,
                velocity: Some(9),
            })
            .unwrap();
        let copied = first.clone();
        assert!(world.remove(&first));
        assert!(!world.remove(&copied));
        let replacement = world
            .insert(Body {
                x: 5,
                velocity: None,
            })
            .unwrap();
        assert_eq!(first.slot(), replacement.slot());
        assert!(world.inspect(&copied).is_none());
        assert!(!world.set_velocity(&copied, Some(7)));
        world.step();
        assert_eq!(
            world.inspect(&replacement),
            Some(Body {
                x: 5,
                velocity: None
            })
        );
    }

    #[test]
    fn foreign_identity_and_former_world_are_not_new_owners() {
        let old = {
            let mut world = World::new();
            world
                .insert(Body {
                    x: 0,
                    velocity: None,
                })
                .unwrap()
        };
        let mut world = World::new();
        let local = world
            .insert(Body {
                x: 0,
                velocity: None,
            })
            .unwrap();
        assert_eq!(
            (old.slot(), old.generation()),
            (local.slot(), local.generation())
        );
        assert!(world.inspect(&old).is_none());
        assert!(!world.remove(&old));
        assert!(world.inspect(&local).is_some());
    }

    #[test]
    fn last_generation_retires_instead_of_resurrecting_ancient_id() {
        let mut world = World::new();
        let ancient = world
            .insert(Body {
                x: 0,
                velocity: None,
            })
            .unwrap();
        assert!(world.remove(&ancient));
        for generation in 1..=u8::MAX {
            let current = world
                .insert(Body {
                    x: 0,
                    velocity: None,
                })
                .unwrap();
            assert_eq!((current.slot(), current.generation()), (0, generation));
            assert!(world.remove(&current));
        }
        let next = world
            .insert(Body {
                x: 0,
                velocity: None,
            })
            .unwrap();
        assert_eq!(next.slot(), 1);
        assert!(world.inspect(&ancient).is_none());
        assert!(
            world
                .insert(Body {
                    x: 0,
                    velocity: None
                })
                .is_none()
        );
    }

    #[test]
    fn snapshots_are_past_values_and_world_transfer_preserves_identity() {
        let mut world = World::new();
        let handle = world
            .insert(Body {
                x: 0,
                velocity: Some(1),
            })
            .unwrap();
        let snapshot = world.inspect(&handle).unwrap();
        let mut transferred = world;
        transferred.step();
        assert_eq!(snapshot.x, 0);
        assert_eq!(transferred.inspect(&handle).unwrap().x, 1);
    }

    #[test]
    fn full_world_preserves_existing_values() {
        let mut world = World::new();
        let a = world
            .insert(Body {
                x: 0,
                velocity: Some(0),
            })
            .unwrap();
        world
            .insert(Body {
                x: 7,
                velocity: None,
            })
            .unwrap();
        assert!(
            world
                .insert(Body {
                    x: 99,
                    velocity: Some(8)
                })
                .is_none()
        );
        assert_eq!(
            world.inspect(&a),
            Some(Body {
                x: 0,
                velocity: Some(0)
            })
        );
        assert!(world.set_velocity(&a, None));
        world.step();
        assert_eq!(
            world.inspect(&a),
            Some(Body {
                x: 0,
                velocity: None
            })
        );
    }
}
