use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use std::sync::{Arc, Mutex};

use gst::glib::WeakRef;
use gst::prelude::*;

#[derive(Default)]
struct TestState {
    captured_element_name: Mutex<Option<String>>,
    num_notifications: AtomicUsize,
    switch_from_streaming_thread: AtomicBool,
    srcpad_target: Mutex<Option<String>>,
    weak_elements: Mutex<Vec<WeakRef<gst::Element>>>,
}

fn init() {
    use std::sync::Once;
    static INIT: Once = Once::new();

    INIT.call_once(|| {
        gst::init().unwrap();
        gstinjectbin::plugin_register_static().unwrap();
    });
}

fn make_identity(
    name: &str,
    injectbin: &WeakRef<gst::Element>,
    state: &Arc<TestState>,
) -> gst::Element {
    let identity = gst::ElementFactory::make("identity")
        .name(name)
        .property("signal-handoffs", true)
        .build()
        .unwrap();

    state
        .weak_elements
        .lock()
        .unwrap()
        .push(identity.downgrade());

    let state_for_callback = state.clone();
    let injectbin_for_callback = injectbin.clone();
    let _ = identity.connect("handoff", false, move |values| {
        if let Ok(element) = values[0].get::<gst::Element>() {
            *state_for_callback.captured_element_name.lock().unwrap() =
                Some(element.name().to_string());
        }

        if state_for_callback
            .switch_from_streaming_thread
            .swap(false, Ordering::SeqCst)
        {
            if let Some(injectbin) = injectbin_for_callback.upgrade() {
                let new_identity =
                    make_identity("i1", &injectbin_for_callback, &state_for_callback);
                injectbin.set_property("element", &new_identity);
            }
        }

        None
    });

    identity
}

fn push_buffer(harness: &mut gst_check::Harness) {
    let input = harness.create_buffer(123).unwrap();
    let input_ptr = input.as_ptr();

    assert_eq!(harness.push(input).unwrap(), gst::FlowSuccess::Ok);

    let output = harness.pull().unwrap();
    assert_eq!(output.as_ptr(), input_ptr);
}

#[test]
fn switch_elements() {
    init();

    let state = Arc::new(TestState::default());
    let injectbin = gst::ElementFactory::make("injectbin").build().unwrap();

    let notification_state = state.clone();
    let _ = injectbin.connect("notify::element", false, move |_| {
        notification_state
            .num_notifications
            .fetch_add(1, Ordering::SeqCst);
        None
    });

    let bus = gst::Bus::new();
    injectbin.set_bus(Some(&bus));

    let injectbin_weak = injectbin.downgrade();

    let mut harness = gst_check::Harness::with_element(&injectbin, Some("sink"), Some("src"));
    harness.set_src_caps(gst::Caps::builder("application/x-something").build());

    push_buffer(&mut harness);
    assert!(injectbin
        .property::<Option<gst::Element>>("element")
        .is_none());

    let i0 = make_identity("i0", &injectbin_weak, &state);
    injectbin.set_property("element", &i0);
    drop(i0);

    push_buffer(&mut harness);
    assert_eq!(
        state.captured_element_name.lock().unwrap().as_deref(),
        Some("i0")
    );

    state
        .switch_from_streaming_thread
        .store(true, Ordering::SeqCst);
    push_buffer(&mut harness);
    push_buffer(&mut harness);

    let current = injectbin
        .property::<Option<gst::Element>>("element")
        .expect("element property");
    assert_eq!(current.name(), "i1");
    drop(current);
    assert_eq!(
        state.captured_element_name.lock().unwrap().as_deref(),
        Some("i1")
    );

    injectbin.set_property("element", Option::<gst::Element>::None);
    push_buffer(&mut harness);
    assert!(injectbin
        .property::<Option<gst::Element>>("element")
        .is_none());

    let i2 = make_identity("i2", &injectbin_weak, &state);
    injectbin.set_property("element", &i2);
    drop(i2);

    let i3 = make_identity("i3", &injectbin_weak, &state);
    injectbin.set_property("element", &i3);
    drop(i3);

    let sinkpad = injectbin.static_pad("sink").unwrap();
    let srcpad = injectbin.static_pad("src").unwrap();

    let sink_state = state.clone();
    let sink_injectbin = injectbin_weak.clone();
    let _ = sinkpad.add_probe(gst::PadProbeType::EVENT_DOWNSTREAM, move |_pad, info| {
        if let Some(gst::PadProbeData::Event(ref event)) = info.data {
            if event.type_() == gst::EventType::Caps {
                if let Some(injectbin) = sink_injectbin.upgrade() {
                    let identity = make_identity("i_caps", &sink_injectbin, &sink_state);
                    injectbin.set_property("element", &identity);
                }
            }
        }

        gst::PadProbeReturn::Ok
    });

    let src_state = state.clone();
    let _ = srcpad.add_probe(gst::PadProbeType::EVENT_DOWNSTREAM, move |pad, info| {
        if let Some(gst::PadProbeData::Event(ref event)) = info.data {
            if event.type_() == gst::EventType::Caps {
                let target_name = pad
                    .downcast_ref::<gst::GhostPad>()
                    .and_then(|ghost| ghost.target())
                    .and_then(|target| target.parent())
                    .map(|parent| parent.name().to_string());

                *src_state.srcpad_target.lock().unwrap() = target_name;
            }
        }

        gst::PadProbeReturn::Ok
    });

    harness.set_src_caps(gst::Caps::builder("application/x-omg").build());

    while let Some(message) = bus.pop() {
        if matches!(message.view(), gst::MessageView::Error(_)) {
            panic!("unexpected error message: {message:?}");
        }
    }

    assert_eq!(
        state.srcpad_target.lock().unwrap().as_deref(),
        Some("i_caps")
    );

    drop(sinkpad);
    drop(srcpad);
    drop(harness);

    assert_eq!(state.num_notifications.load(Ordering::SeqCst), 4);

    bus.set_flushing(true);
    assert_eq!(injectbin.ref_count(), 1);
    drop(injectbin);
    assert!(injectbin_weak.upgrade().is_none());
    assert_eq!(bus.ref_count(), 1);
    drop(bus);

    for weak in state.weak_elements.lock().unwrap().iter() {
        if let Some(element) = weak.upgrade() {
            panic!("element was leaked: {}", element.name());
        }
    }
}
