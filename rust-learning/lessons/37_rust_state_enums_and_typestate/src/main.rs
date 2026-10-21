mod session {
    #[derive(Debug, PartialEq, Eq)]
    pub enum Error {
        WrongState,
    }

    enum State {
        Idle,
        Ready(Vec<i32>),
        Closed(Vec<i32>),
    }

    pub struct Runtime {
        state: State,
    }

    impl Runtime {
        pub fn new() -> Self {
            Self { state: State::Idle }
        }

        pub fn open(&mut self) -> Result<(), Error> {
            if matches!(self.state, State::Idle) {
                self.state = State::Ready(Vec::new());
                Ok(())
            } else {
                Err(Error::WrongState)
            }
        }

        pub fn send(&mut self, reading: i32) -> Result<(), Error> {
            match &mut self.state {
                State::Ready(values) => {
                    values.push(reading);
                    Ok(())
                }
                _ => Err(Error::WrongState),
            }
        }

        pub fn close(&mut self) -> Result<(), Error> {
            if let State::Ready(values) = &mut self.state {
                let retained = std::mem::take(values);
                self.state = State::Closed(retained);
                Ok(())
            } else {
                Err(Error::WrongState)
            }
        }

        pub fn values(&self) -> &[i32] {
            match &self.state {
                State::Idle => &[],
                State::Ready(values) | State::Closed(values) => values,
            }
        }
    }

    pub struct Idle;
    pub struct Ready {
        readings: Vec<i32>,
    }
    pub struct Closed {
        readings: Vec<i32>,
    }
    pub struct Session<S> {
        state: S,
    }

    impl Session<Idle> {
        pub fn new() -> Self {
            Self { state: Idle }
        }

        pub fn open(self) -> Session<Ready> {
            Session {
                state: Ready {
                    readings: Vec::new(),
                },
            }
        }
    }

    impl Session<Ready> {
        pub fn send(&mut self, reading: i32) {
            self.state.readings.push(reading);
        }

        pub fn close(self) -> Session<Closed> {
            Session {
                state: Closed {
                    readings: self.state.readings,
                },
            }
        }
    }

    impl Session<Closed> {
        pub fn values(&self) -> &[i32] {
            &self.state.readings
        }
    }
}

fn main() {
    let mut runtime = session::Runtime::new();
    let before = runtime.send(46700).is_err();
    assert!(before && runtime.values().is_empty());
    println!("before ready rejected={before}");
    runtime.open().unwrap();
    runtime.send(0).unwrap();
    runtime.send(-500).unwrap();
    runtime.close().unwrap();
    assert_eq!(runtime.values(), [0, -500]);
    println!("runtime closed count={}", runtime.values().len());
    let after = runtime.send(46700).is_err();
    assert!(after);
    assert_eq!(runtime.values(), [0, -500]);
    println!("after close rejected={after}");

    let idle = session::Session::new();
    let mut ready = idle.open();
    ready.send(0);
    ready.send(-500);
    let closed = ready.close();
    assert_eq!(closed.values(), [0, -500]);
    println!("typed closed count={}", closed.values().len());
    println!("zero retained={}", closed.values()[0] == 0);
    println!("negative retained={}", closed.values()[1] == -500);
}

#[cfg(test)]
mod tests {
    use super::session::{Error, Runtime, Session};

    #[test]
    fn idle_rejects_send_and_close_without_data_changes() {
        let mut runtime = Runtime::new();
        assert_eq!(runtime.send(0), Err(Error::WrongState));
        assert_eq!(runtime.close(), Err(Error::WrongState));
        assert!(runtime.values().is_empty());
    }

    #[test]
    fn duplicate_open_preserves_ready_recordings() {
        let mut runtime = Runtime::new();
        runtime.open().unwrap();
        runtime.send(0).unwrap();
        assert_eq!(runtime.open(), Err(Error::WrongState));
        assert_eq!(runtime.values(), [0]);
    }

    #[test]
    fn closed_rejects_every_transition_and_preserves_data() {
        let mut runtime = Runtime::new();
        runtime.open().unwrap();
        runtime.send(0).unwrap();
        runtime.send(-500).unwrap();
        runtime.close().unwrap();
        assert_eq!(runtime.send(46700), Err(Error::WrongState));
        assert_eq!(runtime.open(), Err(Error::WrongState));
        assert_eq!(runtime.close(), Err(Error::WrongState));
        assert_eq!(runtime.values(), [0, -500]);
    }

    #[test]
    fn typed_and_runtime_paths_retain_the_same_ordered_values() {
        let mut runtime = Runtime::new();
        runtime.open().unwrap();
        let mut ready = Session::new().open();
        for reading in [0, -500, 46700] {
            runtime.send(reading).unwrap();
            ready.send(reading);
        }
        runtime.close().unwrap();
        assert_eq!(runtime.values(), ready.close().values());
    }

    #[test]
    fn an_empty_ready_session_can_close() {
        let mut runtime = Runtime::new();
        runtime.open().unwrap();
        runtime.close().unwrap();
        assert!(runtime.values().is_empty());
        assert!(Session::new().open().close().values().is_empty());
    }
}
