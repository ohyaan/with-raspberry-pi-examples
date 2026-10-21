use std::future::Future;
use std::marker::PhantomPinned;
use std::pin::{Pin, pin};
use std::sync::Arc;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::task::{Context, Poll, Wake, Waker};

struct StableLabel {
    text: String,
    _pinned: PhantomPinned,
}

impl StableLabel {
    fn new(text: &str) -> Self {
        Self {
            text: text.to_owned(),
            _pinned: PhantomPinned,
        }
    }

    fn text(self: Pin<&Self>) -> &str {
        &self.get_ref().text
    }
}

// The wrapper owns a movable handle to an independently pinned future.
struct Observed<F> {
    inner: Pin<Box<F>>,
    polls: usize,
}

impl<F> Observed<F> {
    fn new(inner: F) -> Self {
        Self {
            inner: Box::pin(inner),
            polls: 0,
        }
    }
}

impl<F: Future> Future for Observed<F> {
    type Output = F::Output;

    fn poll(self: Pin<&mut Self>, context: &mut Context<'_>) -> Poll<Self::Output> {
        let this = self.get_mut();
        this.polls += 1;
        this.inner.as_mut().poll(context)
    }
}

struct YieldOnce(bool);

impl Future for YieldOnce {
    type Output = ();

    fn poll(mut self: Pin<&mut Self>, context: &mut Context<'_>) -> Poll<()> {
        if self.0 {
            Poll::Ready(())
        } else {
            self.0 = true;
            context.waker().wake_by_ref();
            Poll::Pending
        }
    }
}

#[derive(Default)]
struct CountWake(AtomicUsize);

impl Wake for CountWake {
    fn wake(self: Arc<Self>) {
        self.0.fetch_add(1, Ordering::Relaxed);
    }
}

// Only for futures known to finish on their first poll; not an executor.
fn ready_value<F: Future>(future: F) -> F::Output {
    let waker = Waker::from(Arc::new(CountWake::default()));
    let mut context = Context::from_waker(&waker);
    let mut pinned = pin!(future);
    match pinned.as_mut().poll(&mut context) {
        Poll::Ready(value) => value,
        Poll::Pending => panic!("fixture unexpectedly suspended"),
    }
}

fn shorten<'long: 'short, 'short>(value: &'long str) -> &'short str {
    value
}

fn main() {
    let label = Box::pin(StableLabel::new("Pi 4B"));
    let address = std::ptr::from_ref(label.as_ref().get_ref());
    // This moves the handle, not the heap allocation's StableLabel.
    let labels = vec![label];
    let unchanged = address == std::ptr::from_ref(labels[0].as_ref().get_ref());
    println!(
        "same pointee={unchanged}, label={}",
        labels[0].as_ref().text()
    );

    let mut text = String::from("Pi");
    Pin::new(&mut text).get_mut().push_str(" 4B");
    println!("Unpin text={text}");

    let mut observed = Observed::new(async {
        YieldOnce(false).await;
        46_700
    });
    let notifications = Arc::new(CountWake::default());
    let waker = Waker::from(Arc::clone(&notifications));
    let mut context = Context::from_waker(&waker);
    assert_eq!(Pin::new(&mut observed).poll(&mut context), Poll::Pending);
    assert_eq!(notifications.0.load(Ordering::Relaxed), 1);
    let value = match Pin::new(&mut observed).poll(&mut context) {
        Poll::Ready(value) => value,
        Poll::Pending => panic!("fixture should finish on second poll"),
    };
    println!("observed={value}, polls={}", observed.polls);

    let futures: Vec<Pin<Box<dyn Future<Output = i32>>>> =
        vec![Box::pin(async { 46_700 }), Box::pin(async { 0 })];
    let values: Vec<_> = futures.into_iter().map(ready_value).collect();
    println!("erased values={values:?}");

    let owner = String::from("Pi 4B");
    let borrowed: Pin<Box<dyn Future<Output = usize> + '_>> =
        Box::pin(async { shorten(&owner).len() });
    println!("borrowed bytes={}", ready_value(borrowed));
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::Cell;
    use std::rc::Rc;

    struct DropFlag(Rc<Cell<usize>>);

    impl Drop for DropFlag {
        fn drop(&mut self) {
            self.0.set(self.0.get() + 1);
        }
    }

    #[test]
    fn moving_and_growing_handles_preserves_pointee() {
        let first = Box::pin(StableLabel::new("Pi 4B"));
        let address = std::ptr::from_ref(first.as_ref().get_ref());
        let mut handles = vec![first];
        for _ in 0..32 {
            handles.push(Box::pin(StableLabel::new("fixture")));
        }
        assert_eq!(address, std::ptr::from_ref(handles[0].as_ref().get_ref()));
        assert_eq!(handles[0].as_ref().text(), "Pi 4B");
    }

    #[test]
    fn unpin_allows_ordinary_mutation_and_extraction() {
        let mut text = String::from("Pi");
        let pinned = Pin::new(&mut text);
        Pin::into_inner(pinned).push_str(" 4B");
        assert_eq!(text, "Pi 4B");
    }

    #[test]
    fn wrapper_is_unpin_even_when_inner_future_is_not() {
        fn require_unpin<T: Unpin>(_: &T) {}
        let observed = Observed::new(async { 46_700 });
        require_unpin(&observed);
        assert_eq!(ready_value(observed), 46_700);
    }

    #[test]
    fn pending_wrapper_counts_notification_and_completion() {
        let mut observed = Observed::new(async {
            YieldOnce(false).await;
            0
        });
        let count = Arc::new(CountWake::default());
        let waker = Waker::from(Arc::clone(&count));
        let mut context = Context::from_waker(&waker);
        assert_eq!(Pin::new(&mut observed).poll(&mut context), Poll::Pending);
        assert_eq!(count.0.load(Ordering::Relaxed), 1);
        assert_eq!(Pin::new(&mut observed).poll(&mut context), Poll::Ready(0));
        assert_eq!(observed.polls, 2);
    }

    #[test]
    fn different_async_types_share_an_explicit_interface() {
        let futures: Vec<Pin<Box<dyn Future<Output = i32>>>> =
            vec![Box::pin(async { -500 }), Box::pin(async { 0 })];
        assert_eq!(
            futures.into_iter().map(ready_value).collect::<Vec<_>>(),
            [-500, 0]
        );
    }

    #[test]
    fn boxed_future_can_borrow_a_local_owner() {
        let owner = String::from("Pi 4B");
        let future: Pin<Box<dyn Future<Output = usize> + '_>> = Box::pin(async { owner.len() });
        assert_eq!(ready_value(future), 5);
        assert_eq!(owner, "Pi 4B");
    }

    #[test]
    fn borrowed_pin_is_not_the_cancellation_owner() {
        let drops = Rc::new(Cell::new(0));
        let guard = DropFlag(Rc::clone(&drops));
        let mut owner = Box::pin(async move {
            let _guard = guard;
            std::future::pending::<()>().await;
        });
        {
            let borrowed = owner.as_mut();
            drop(borrowed);
        }
        assert_eq!(drops.get(), 0);
        drop(owner);
        assert_eq!(drops.get(), 1);
    }

    #[test]
    fn pinned_does_not_mean_not_send() {
        fn require_send<T: Send>(_: &T) {}
        let label = Box::pin(StableLabel::new("Pi 4B"));
        require_send(&label);
    }

    #[test]
    fn shortening_reference_does_not_extend_owner_lifetime() {
        let owner = String::from("Pi 4B");
        let shorter = shorten(&owner);
        assert_eq!(shorter, "Pi 4B");
    }
}
