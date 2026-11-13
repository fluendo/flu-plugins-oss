use gst::glib;
use gst::prelude::*;
use gst::subclass::prelude::*;
use parking_lot::Mutex;

use once_cell::sync::Lazy;

const DEFAULT_GOP_SIZE: u32 = 10;
const NUM_ENC: usize = 5; // TODO: Make it dynamic
const ENC_PREFIX: &'static str = "encoder-";

static CAT: Lazy<gst::DebugCategory> = Lazy::new(|| {
    gst::DebugCategory::new(
        "hype",
        gst::DebugColorFlags::empty(),
        Some("Fluendo Hype Encoder"),
    )
});

pub struct Hype {
    srcpad: gst::GhostPad,
    sinkpad: gst::GhostPad,

    scenedetector: gst::Element,
    outputselector: gst::Element,
    scenecollector: gst::Element,
    capsfilter: gst::Element,

    // TODO: Maybe it's possible to access to the "numchildren" attribute of GstBin
    numchildren: Mutex<usize>,
}

impl Hype {
    fn create_pipeline(&self) -> bool {
        let mut intersected_caps = gst::Caps::new_any();
        // TODO: Return false instead of unwrap()
        for i in 0..NUM_ENC {
            let Some(enc) = self.obj().by_name(&format!("{ENC_PREFIX}{i}")) else {
                continue;
            };

            let src_pad = self.outputselector.request_pad_simple("src_%u").unwrap();
            src_pad.link(&enc.static_pad("sink").unwrap()).unwrap();

            let sink_pad = self.scenecollector.request_pad_simple("sink_%u").unwrap();
            enc.static_pad("src").unwrap().link(&sink_pad).unwrap();

            if let Some(pad_template) = enc.pad_template("src") {
                intersected_caps = intersected_caps.intersect(pad_template.caps());
            }

            *self.numchildren.lock() += 1;
        }

        if intersected_caps.is_empty() {
            gst::error!(CAT, "Intersected caps are empty");
            false
        } else {
            gst::debug!(CAT, "Intersected caps = {}", intersected_caps);
            self.capsfilter.set_property("caps", intersected_caps);
            true
        }
    }
}

#[glib::object_subclass]
impl ObjectSubclass for Hype {
    const NAME: &'static str = "GstHype";
    type Type = super::Hype;
    type ParentType = gst::Bin;
    type Interfaces = (gst::ChildProxy,);

    fn with_class(klass: &Self::Class) -> Self {
        let templ = klass.pad_template("sink").unwrap();
        let sinkpad = gst::GhostPad::from_template(&templ);
        let templ = klass.pad_template("src").unwrap();
        let srcpad = gst::GhostPad::from_template(&templ);

        let scenedetector = gst::ElementFactory::make("scenedetector").build().unwrap();
        let outputselector = gst::ElementFactory::make("outputselector").build().unwrap();
        let scenecollector = gst::ElementFactory::make("scenecollector").build().unwrap();
        let capsfilter = gst::ElementFactory::make("capsfilter").build().unwrap();

        scenedetector.set_property("name", "scenedetector");
        outputselector.set_property("name", "outputselector");
        scenecollector.set_property("name", "scenecollector");
        capsfilter.set_property("name", "capsfilter");

        Self {
            sinkpad,
            srcpad,
            scenedetector,
            outputselector,
            scenecollector,
            capsfilter,
            numchildren: Mutex::new(0),
        }
    }
}

impl ObjectImpl for Hype {
    fn properties() -> &'static [glib::ParamSpec] {
        static PROPERTIES: Lazy<Vec<glib::ParamSpec>> = Lazy::new(|| {
            let mut properties = vec![glib::ParamSpecUInt::builder("gop-size")
                .nick("GOP size")
                .blurb("Send a event each gop-size number of buffers")
                .maximum(u32::MAX)
                .default_value(DEFAULT_GOP_SIZE)
                .build()];

            for i in 0..NUM_ENC {
                properties.push(
                    glib::ParamSpecObject::builder::<gst::Element>(&format!("{ENC_PREFIX}{i}"))
                        .nick(&format!("Video encoder {i}"))
                        .blurb(&format!("Video encoder {i}"))
                        .construct_only()
                        .build(),
                );
            }

            properties
        });

        PROPERTIES.as_ref()
    }

    fn set_property(&self, _id: usize, value: &glib::Value, pspec: &glib::ParamSpec) {
        match pspec.name() {
            "gop-size" => {
                self.scenedetector.set_property("gop-size", value);
            }
            enc if enc.starts_with(ENC_PREFIX) => {
                if let Some(_) = self.obj().by_name(enc) {
                    gst::warning!(
                        CAT,
                        "The element {enc} already exists in the bin. Not adding it."
                    );
                } else {
                    if let Ok(Some(enc_obj)) = value.get::<Option<gst::Element>>() {
                        let factory = enc_obj
                            .factory()
                            .expect("The element has not type. Not adding it.");
                        if !factory.has_type(gst::ElementFactoryType::VIDEO_ENCODER)
                            && factory.name() != "identity"
                        {
                            gst::error!(CAT, "The element is not a video encoder");
                            panic!("The element is not a video encoder");
                        }

                        enc_obj.set_property("name", enc);
                        self.obj().add(&enc_obj).unwrap();
                    }
                }
            }
            _ => unimplemented!(),
        }
    }

    fn property(&self, _id: usize, pspec: &glib::ParamSpec) -> glib::Value {
        match pspec.name() {
            "gop-size" => self.scenedetector.property("gop-size"),
            enc if enc.starts_with(ENC_PREFIX) => self.obj().by_name(enc).to_value(),
            _ => unimplemented!(),
        }
    }

    fn constructed(&self) {
        self.parent_constructed();

        let obj = self.obj();

        obj.add(&self.scenedetector).unwrap();
        obj.add(&self.outputselector).unwrap();
        obj.add(&self.scenecollector).unwrap();
        obj.add(&self.capsfilter).unwrap();
        *self.numchildren.lock() += 4;

        self.scenedetector.link(&self.outputselector).unwrap();
        self.scenecollector.link(&self.capsfilter).unwrap();

        self.sinkpad
            .set_target(Some(&self.scenedetector.static_pad("sink").unwrap()))
            .unwrap();
        self.srcpad
            .set_target(Some(&self.capsfilter.static_pad("src").unwrap()))
            .unwrap();

        obj.add_pad(&self.sinkpad).unwrap();
        obj.add_pad(&self.srcpad).unwrap();
    }
}

impl GstObjectImpl for Hype {}

impl ElementImpl for Hype {
    fn metadata() -> Option<&'static gst::subclass::ElementMetadata> {
        static ELEMENT_METADATA: Lazy<gst::subclass::ElementMetadata> = Lazy::new(|| {
            gst::subclass::ElementMetadata::new(
                "Hype Video Encoder Bin",
                "Video/Encoder",
                "TODO",
                "Carlos Falgueras García <cfalgueras@fluendo.com>, Ruben Gonzalez <rgonzalez@fluendo.com>",
            )
        });

        Some(&*ELEMENT_METADATA)
    }

    fn pad_templates() -> &'static [gst::PadTemplate] {
        static PAD_TEMPLATES: Lazy<Vec<gst::PadTemplate>> = Lazy::new(|| {
            let caps = gst::Caps::new_any();
            let src_pad_template = gst::PadTemplate::new(
                "src",
                gst::PadDirection::Src,
                gst::PadPresence::Always,
                &caps,
            )
            .unwrap();

            let sink_pad_template = gst::PadTemplate::new(
                "sink",
                gst::PadDirection::Sink,
                gst::PadPresence::Always,
                &caps,
            )
            .unwrap();

            vec![src_pad_template, sink_pad_template]
        });

        PAD_TEMPLATES.as_ref()
    }

    fn change_state(
        &self,
        transition: gst::StateChange,
    ) -> Result<gst::StateChangeSuccess, gst::StateChangeError> {
        gst::trace!(CAT, imp: self, "Changing state {:?}", transition);

        if let gst::StateChange::NullToReady = transition {
            if !self.create_pipeline() {
                return Err(gst::StateChangeError);
            }
        }

        self.parent_change_state(transition)
    }
}

impl BinImpl for Hype {}

impl ChildProxyImpl for Hype {
    fn child_by_index(&self, index: u32) -> Option<glib::Object> {
        match index {
            0 => Some(self.scenedetector.clone().upcast()),
            1 => Some(self.outputselector.clone().upcast()),
            2 => Some(self.scenecollector.clone().upcast()),
            3 => Some(self.capsfilter.clone().upcast()),
            i => Some(
                self.obj()
                    .by_name(&format!("{ENC_PREFIX}{i}"))?
                    .clone()
                    .upcast(),
            ),
        }
    }

    fn children_count(&self) -> u32 {
        (*self.numchildren.lock()).try_into().unwrap()
    }

    fn child_by_name(&self, name: &str) -> Option<glib::Object> {
        Some(self.obj().by_name(name)?.clone().upcast())
    }
}
