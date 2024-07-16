use gst::prelude::*;
use pretty_assertions::assert_eq;

const NUM_BUFFERS: u64 = 20;

fn init() {
    use std::sync::Once;
    static INIT: Once = Once::new();

    INIT.call_once(|| {
        gst::init().unwrap();
        gsthype::plugin_register_static().unwrap();
    });
}

fn test_order(pipeline: &str) {
    init();

    let mut h = gst_check::Harness::new_parse(pipeline);

    let caps = "video/x-raw, format=RGB, width=1, height=1, framerate=30/1";
    h.set_src_caps_str(caps);
    h.set_sink_caps_str(caps);

    let buffers: Vec<gst::Buffer> = (0..NUM_BUFFERS)
        .map(|i| {
            let mut buf = gst::Buffer::with_size(27).unwrap();
            let buf_ref = buf.get_mut().unwrap();
            buf_ref.set_pts(i * gst::ClockTime::MSECOND);
            buf_ref.set_duration(1 * gst::ClockTime::MSECOND);
            buf_ref.set_offset(i);
            buf
        })
        .collect();

    for buffer in &buffers {
        assert_eq!(h.push(buffer.clone()), Ok(gst::FlowSuccess::Ok));
    }
    assert!(h.push_event(gst::event::Eos::new()));

    for in_buffer in &buffers {
        let out_buffer = h.pull().unwrap();
        dbg!(out_buffer.pts());
        assert_eq!(in_buffer.pts(), out_buffer.pts());
    }
}

//TODO tests to check caps negotiation

#[test]
fn test_one_identity() {
    test_order(
        " \
        scenedetector \
            gop-size=2 \
        ! outputselector name=os \
        ! identity \
        ! scenecollector name=col \
    ",
    );
}

#[test]
fn test_identity() {
    test_order(
        " \
        scenedetector \
            gop-size=2 \
        ! outputselector name=os \
        \
        os.src_0  \
        ! identity \
        ! col. \
        \
        os.src_1  \
        ! identity \
        ! col. \
        \
        scenecollector name=col \
    ",
    );
}

#[test]
fn test_identity_sleep() {
    test_order(
        " \
        scenedetector \
            gop-size=5 \
        ! outputselector name=os \
        \
        os.src_0  \
        ! identity \
           sleep-time=10000 \
           silent=false \
        ! col. \
        \
        os.src_1  \
        ! identity \
           sleep-time=100 \
           silent=false \
        ! col. \
        \
        scenecollector name=col \
    ",
    );
}

#[test]
fn test_identity1() {
    test_order(
        " \
        hype \
          gop-size=5 \
          encoder-1=identity \
    ",
    );
}

#[test]
fn test_identity3() {
    test_order(
        " \
        hype \
          gop-size=5 \
          encoder-1=identity \
          encoder-2=identity \
          encoder-3=identity \
    ",
    );
}

#[test]
fn test_incompatible_encoders() {
    init();
    let pipeline =
        gst::parse_launch("videotestsrc ! hype encoder-0=x264enc encoder-1=x265enc ! fakesink")
            .unwrap();
    assert!(pipeline.set_state(gst::State::Playing).is_err());
}
