use gst::glib;
use gst::prelude::*;
use gst::subclass::prelude::*;
use parking_lot::Mutex;

use once_cell::sync::Lazy;

use crate::SceneNewHypeEvent;
use gst_video::video_event::DownstreamForceKeyUnitEvent;

const SCENE_BUFFER_SIZE: u32 = 20;

static CAT: Lazy<gst::DebugCategory> = Lazy::new(|| {
    gst::DebugCategory::new(
        "hypeoutputselector",
        gst::DebugColorFlags::empty(),
        Some("Hype output selector"),
    )
});

pub struct HypeOutputSelector {
    srcpad_counter: Mutex<usize>,
    output_selector: gst::Element,
    sinkpad: gst::GhostPad,
    queues: Mutex<Vec<(gst::Element, gst::GhostPad, gst::Pad)>>,
}

impl HypeOutputSelector {
    fn new_scene_event(&self, ev: SceneNewHypeEvent) {
        gst::info!(CAT, "Scene Event = {:?}", ev);

        let queues = self.queues.lock();
        let active_index = (ev.gop_index as usize) % queues.len();
        self.output_selector
            .set_property("active-pad", &queues[active_index].2);
        gst::info!(CAT, "active index = {}", active_index);
        gst::info!(CAT, "active pad = {}", queues[active_index].2.name());

        gst::debug!(CAT, "Send DownstreamForceKeyUnitEvent");
        let event = DownstreamForceKeyUnitEvent::builder().build();
        if !&self.output_selector.send_event(event) {
            gst::debug!(CAT, "Error sending DownstreamForceKeyUnitEvent");
        }
    }
}

#[glib::object_subclass]
impl ObjectSubclass for HypeOutputSelector {
    const NAME: &'static str = "GstHypeOutputSelector";
    type Type = super::HypeOutputSelector;
    type ParentType = gst::Bin;

    fn with_class(klass: &Self::Class) -> Self {
        let templ = klass.pad_template("sink").unwrap();
        let sinkpad = gst::GhostPad::from_template(&templ);

        let output_selector = gst::ElementFactory::make("output-selector")
            .name("hype-output-selector")
            .property("resend-latest", false)
            .build()
            .unwrap();

        let queues = Mutex::new(Vec::new());

        Self {
            srcpad_counter: Mutex::new(0),
            sinkpad,
            output_selector,
            queues,
        }
    }
}

impl ObjectImpl for HypeOutputSelector {
    fn constructed(&self) {
        gst::info!(CAT, "OutputSelector constructed");
        self.parent_constructed();

        let obj = self.obj();

        obj.add(&self.output_selector).unwrap();

        let os_sink_pad = self.output_selector.static_pad("sink").unwrap();

        self.sinkpad.set_target(Some(&os_sink_pad)).unwrap();
        obj.add_pad(&self.sinkpad).unwrap();

        // passing ownership of self to the probe callback would introduce a reference cycle as the
        // self is owning the sinkpad
        let self_weak = self.downgrade();

        os_sink_pad.add_probe(
            gst::PadProbeType::EVENT_DOWNSTREAM,
            move |_pad, probe_info| {
                // Just interested in SceneNewHypeEvent
                let Some(gst::PadProbeData::Event(ref ev)) = probe_info.data else {
                    return gst::PadProbeReturn::Ok;
                };
                let gst::EventView::CustomDownstream(ev) = ev.view() else {
                    return gst::PadProbeReturn::Ok;
                };
                let Some(scene_new_event) = SceneNewHypeEvent::parse(ev) else {
                    return gst::PadProbeReturn::Ok;
                };

                self_weak
                    .upgrade()
                    .unwrap()
                    .new_scene_event(scene_new_event);
                gst::PadProbeReturn::Ok
            },
        );
    }
}

impl GstObjectImpl for HypeOutputSelector {}

impl ElementImpl for HypeOutputSelector {
    fn metadata() -> Option<&'static gst::subclass::ElementMetadata> {
        static ELEMENT_METADATA: Lazy<gst::subclass::ElementMetadata> = Lazy::new(|| {
            gst::subclass::ElementMetadata::new(
                "Hype Output Selector",
                "Filter/Effect/Converter/Video",
                "TODO",
                "Carlos Falgueras García <cfalgueras@fluendo.com>, Ruben Gonzalez <rgonzalez@fluendo.com>",
            )
        });

        Some(&*ELEMENT_METADATA)
    }

    fn pad_templates() -> &'static [gst::PadTemplate] {
        static PAD_TEMPLATES: Lazy<Vec<gst::PadTemplate>> = Lazy::new(|| {
            // Our element can accept any possible caps on both pads
            let caps = gst::Caps::new_any();

            let sink_pad_template = gst::PadTemplate::new(
                "sink",
                gst::PadDirection::Sink,
                gst::PadPresence::Always,
                &caps,
            )
            .unwrap();

            let src_pad_template = gst::PadTemplate::new(
                "src_%u",
                gst::PadDirection::Src,
                gst::PadPresence::Request,
                &caps,
            )
            .unwrap();

            vec![src_pad_template, sink_pad_template]
        });

        PAD_TEMPLATES.as_ref()
    }

    fn request_new_pad(
        &self,
        templ: &gst::PadTemplate,
        _name: Option<&str>,
        _caps: Option<&gst::Caps>,
    ) -> Option<gst::Pad> {
        let mut srcpad_counter = self.srcpad_counter.lock();
        let srcpad_id = *srcpad_counter;
        *srcpad_counter += 1;

        //TODO use property for max-size-buffers
        let queue = gst::ElementFactory::make("queue")
            .property("max-size-bytes", 0u32)
            .property("max-size-time", 0u64)
            .property("max-size-buffers", SCENE_BUFFER_SIZE * 2)
            .build()
            .unwrap();
        //queue.connect("underrun", self._queue_underrun)
        self.obj().add(&queue).unwrap();

        let srcpad = gst::GhostPad::builder_from_template_with_target(
            &templ,
            &queue.static_pad("src").unwrap(),
        )
        .unwrap()
        .name(format!("src_{srcpad_id}").as_str())
        .build();

        srcpad.set_active(true).unwrap();
        self.obj().add_pad(&srcpad).unwrap();

        let os_src_pad = self.output_selector.request_pad_simple("src_%u").unwrap();
        os_src_pad.link(&queue.static_pad("sink").unwrap()).unwrap();

        let mut queues = self.queues.lock();
        queues.push((queue, srcpad.clone(), os_src_pad));

        Some(srcpad.upcast())
    }

    //TODO release_pad
}

impl BinImpl for HypeOutputSelector {}
