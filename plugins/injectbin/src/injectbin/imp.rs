use gst::glib;
use gst::prelude::*;
use gst::subclass::prelude::*;

use once_cell::sync::Lazy;
use parking_lot::Mutex;

static CAT: Lazy<gst::DebugCategory> = Lazy::new(|| {
    gst::DebugCategory::new(
        "injectbin",
        gst::DebugColorFlags::empty(),
        Some("Fluendo dynamic pipeline rebuild bin"),
    )
});

#[derive(Default)]
struct State {
    generation: u64,
    current_element: Option<gst::Element>,
    requested_element: Option<gst::Element>,
    last_probe_id: Option<gst::PadProbeId>,
}

pub struct InjectBin {
    sinkpad: gst::GhostPad,
    srcpad: gst::GhostPad,
    input_identity: gst::Element,
    identity_sinkpad: gst::Pad,
    identity_srcpad: gst::Pad,
    state: Mutex<State>,
}

impl InjectBin {
    fn handle_block_probe(&self, generation: u64, probe_id: Option<&gst::PadProbeId>) {
        let (current, requested) = {
            let mut state = self.state.lock();

            // A newer property update has installed its own probe; this one is
            // stale and must not perform the switch.
            if state.generation != generation {
                return;
            }

            if state.last_probe_id.as_ref() == probe_id {
                state.last_probe_id = None;
            }

            (state.current_element.take(), state.requested_element.take())
        };

        self.switch_element(current, requested);
    }

    fn switch_element(&self, current: Option<gst::Element>, requested: Option<gst::Element>) {
        if let Some(current) = &current {
            gst::debug!(CAT, "Removing the current element {}", current.name());

            current.set_locked_state(true);
            if let Err(err) = current.set_state(gst::State::Null) {
                gst::debug!(CAT, "Failed to set current element to NULL: {err}");
            }
            self.input_identity.unlink(current);
            let _ = self.srcpad.set_target(None::<&gst::Pad>);

            if let Err(err) = self.obj().remove(current) {
                gst::error!(CAT, "Failed to remove the current element: {err}");
            }
        }

        if let Some(requested) = &requested {
            gst::debug!(CAT, "Making requested element the current one");

            if let Err(err) = self.obj().add(requested) {
                gst::error!(CAT, "Failed to add the requested element: {err}");
            }

            match requested.static_pad("src") {
                Some(srcpad) => {
                    if let Err(err) = self.srcpad.set_target(Some(&srcpad)) {
                        gst::error!(CAT, "Failed to set the src ghost pad target: {err}");
                    }
                }
                None => gst::error!(CAT, "The requested element has no src pad"),
            }

            if let Err(err) = self.input_identity.link(requested) {
                gst::error!(
                    CAT,
                    "Failed to link the input identity to the requested element: {err}"
                );
            }

            if let Err(err) = requested.sync_state_with_parent() {
                gst::error!(CAT, "Failed to sync the requested element's state: {err}");
            }
        } else {
            gst::debug!(CAT, "No requested element; using the input identity");
            if let Err(err) = self.srcpad.set_target(Some(&self.identity_srcpad)) {
                gst::error!(
                    CAT,
                    "Failed to set the src ghost pad target to identity: {err}"
                );
            }
        }

        {
            let mut state = self.state.lock();
            state.current_element = requested;
        }

        self.obj().notify("element");
    }
}

#[glib::object_subclass]
impl ObjectSubclass for InjectBin {
    const NAME: &'static str = "GstInjectBin";
    type Type = super::InjectBin;
    type ParentType = gst::Bin;

    fn with_class(klass: &Self::Class) -> Self {
        let sink_template = klass.pad_template("sink").unwrap();
        let src_template = klass.pad_template("src").unwrap();

        let sinkpad = gst::GhostPad::from_template(&sink_template);
        let srcpad = gst::GhostPad::from_template(&src_template);

        let input_identity = gst::ElementFactory::make("identity").build().unwrap();
        let identity_sinkpad = input_identity.static_pad("sink").unwrap();
        let identity_srcpad = input_identity.static_pad("src").unwrap();

        Self {
            sinkpad,
            srcpad,
            input_identity,
            identity_sinkpad,
            identity_srcpad,
            state: Mutex::new(State::default()),
        }
    }
}

impl ObjectImpl for InjectBin {
    fn constructed(&self) {
        self.parent_constructed();

        let obj = self.obj();

        let identity_name = format!("{}_input_identity", obj.name());
        self.input_identity.set_property("name", identity_name);

        if let Err(err) = obj.add(&self.input_identity) {
            gst::error!(CAT, "Failed to add the input identity: {err}");
        }
        if let Err(err) = self.sinkpad.set_target(Some(&self.identity_sinkpad)) {
            gst::error!(CAT, "Failed to set the sink ghost pad target: {err}");
        }
        if let Err(err) = self.srcpad.set_target(Some(&self.identity_srcpad)) {
            gst::error!(CAT, "Failed to set the src ghost pad target: {err}");
        }
        if let Err(err) = obj.add_pad(&self.sinkpad) {
            gst::error!(CAT, "Failed to add the sink ghost pad: {err}");
        }
        if let Err(err) = obj.add_pad(&self.srcpad) {
            gst::error!(CAT, "Failed to add the src ghost pad: {err}");
        }
    }

    fn properties() -> &'static [glib::ParamSpec] {
        static PROPERTIES: Lazy<Vec<glib::ParamSpec>> = Lazy::new(|| {
            vec![glib::ParamSpecObject::builder::<gst::Element>("element")
                .nick("Element")
                .blurb("Injected element")
                .flags(glib::ParamFlags::READWRITE | glib::ParamFlags::EXPLICIT_NOTIFY)
                .build()]
        });

        PROPERTIES.as_ref()
    }

    fn set_property(&self, _id: usize, value: &glib::Value, pspec: &glib::ParamSpec) {
        match pspec.name() {
            "element" => {
                let requested: Option<gst::Element> =
                    value.get::<Option<gst::Element>>().unwrap_or(None);

                let (old_probe, generation) = {
                    let mut state = self.state.lock();
                    state.generation = state.generation.wrapping_add(1);
                    state.requested_element = requested;
                    (state.last_probe_id.take(), state.generation)
                };

                if let Some(id) = old_probe {
                    self.identity_sinkpad.remove_probe(id);
                }

                let self_weak = self.obj().downgrade();
                let probe_id = self.identity_sinkpad.add_probe(
                    gst::PadProbeType::BLOCK_DOWNSTREAM,
                    move |_pad, info| {
                        let Some(obj) = self_weak.upgrade() else {
                            return gst::PadProbeReturn::Remove;
                        };

                        obj.imp().handle_block_probe(generation, info.id.as_ref());
                        gst::PadProbeReturn::Remove
                    },
                );

                match probe_id {
                    Some(id) => {
                        let mut state = self.state.lock();
                        if state.generation == generation {
                            state.last_probe_id = Some(id);
                        } else {
                            drop(state);
                            self.identity_sinkpad.remove_probe(id);
                        }
                    }
                    None => gst::error!(CAT, "Failed to add the blocking pad probe"),
                }
            }
            _ => unimplemented!(),
        }
    }

    fn property(&self, _id: usize, pspec: &glib::ParamSpec) -> glib::Value {
        match pspec.name() {
            "element" => self.state.lock().current_element.clone().to_value(),
            _ => unimplemented!(),
        }
    }
}

impl GstObjectImpl for InjectBin {}

impl ElementImpl for InjectBin {
    fn metadata() -> Option<&'static gst::subclass::ElementMetadata> {
        static ELEMENT_METADATA: Lazy<gst::subclass::ElementMetadata> = Lazy::new(|| {
            gst::subclass::ElementMetadata::new(
                "injectbin",
                "Generic/Bin",
                "Inject or replace an element in the pipeline",
                "Alexander Slobodeniuk <aslobodeniuk@fluendo.com>",
            )
        });

        Some(&*ELEMENT_METADATA)
    }

    fn pad_templates() -> &'static [gst::PadTemplate] {
        static PAD_TEMPLATES: Lazy<Vec<gst::PadTemplate>> = Lazy::new(|| {
            let caps = gst::Caps::new_any();

            let sink_template = gst::PadTemplate::new(
                "sink",
                gst::PadDirection::Sink,
                gst::PadPresence::Always,
                &caps,
            )
            .unwrap();

            let src_template = gst::PadTemplate::new(
                "src",
                gst::PadDirection::Src,
                gst::PadPresence::Always,
                &caps,
            )
            .unwrap();

            vec![sink_template, src_template]
        });

        PAD_TEMPLATES.as_ref()
    }
}

impl BinImpl for InjectBin {}
