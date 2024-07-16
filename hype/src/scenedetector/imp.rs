use gst::glib;
use gst::prelude::*;
use gst::subclass::prelude::*;
use gst_video::prelude::*;
use gst_video::subclass::prelude::*;

use crate::SceneNewHypeEvent;

use once_cell::sync::Lazy;
use std::sync::Mutex;

static CAT: Lazy<gst::DebugCategory> = Lazy::new(|| {
    gst::DebugCategory::new(
        "hypescenedetector",
        gst::DebugColorFlags::empty(),
        Some("Fluendo Scene Splitter"),
    )
});

const DEFAULT_GOP_SIZE: u32 = 10;

#[derive(Default)]
pub struct SceneDetector {
    settings: Mutex<Settings>,
}

#[derive(Debug, Clone, Copy)]
struct Settings {
    gop_size: u32,
}

impl Default for Settings {
    fn default() -> Self {
        Settings {
            gop_size: DEFAULT_GOP_SIZE,
        }
    }
}

impl SceneDetector {}

#[glib::object_subclass]
impl ObjectSubclass for SceneDetector {
    const NAME: &'static str = "GstHypeSceneDetector";
    type Type = super::SceneDetector;
    type ParentType = gst_video::VideoFilter;
}

impl ObjectImpl for SceneDetector {
    fn properties() -> &'static [glib::ParamSpec] {
        static PROPERTIES: Lazy<Vec<glib::ParamSpec>> = Lazy::new(|| {
            vec![glib::ParamSpecUInt::builder("gop-size")
                .nick("GOP size")
                .blurb("Send a event each gop-size number of buffers")
                .maximum(u32::MAX)
                .default_value(DEFAULT_GOP_SIZE)
                .build()]
        });

        PROPERTIES.as_ref()
    }

    fn set_property(&self, _id: usize, value: &glib::Value, pspec: &glib::ParamSpec) {
        match pspec.name() {
            "gop-size" => {
                let mut settings = self.settings.lock().unwrap();
                let gop_size = value.get().expect("type checked upstream");
                gst::info!(
                    CAT,
                    "GOP size changed from  {} to {}",
                    settings.gop_size,
                    gop_size
                );
                settings.gop_size = gop_size;
            }
            _ => unimplemented!(),
        }
    }

    fn property(&self, _id: usize, pspec: &glib::ParamSpec) -> glib::Value {
        match pspec.name() {
            "gop-size" => {
                let settings = self.settings.lock().unwrap();
                settings.gop_size.to_value()
            }
            _ => unimplemented!(),
        }
    }
}

impl GstObjectImpl for SceneDetector {}

impl ElementImpl for SceneDetector {
    fn metadata() -> Option<&'static gst::subclass::ElementMetadata> {
        static ELEMENT_METADATA: Lazy<gst::subclass::ElementMetadata> = Lazy::new(|| {
            gst::subclass::ElementMetadata::new(
                "Hype Scene Detector",
                "Video/Filter",
                "Detects scenes and push an event for each one of them",
                "Carlos Falgueras García <cfalgueras@fluendo.com>, Ruben Gonzalez <rgonzalez@fluendo.com>",
            )
        });

        Some(&*ELEMENT_METADATA)
    }

    fn pad_templates() -> &'static [gst::PadTemplate] {
        static PAD_TEMPLATES: Lazy<Vec<gst::PadTemplate>> = Lazy::new(|| {
            let caps = gst_video::VideoCapsBuilder::new().build();
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
}

impl BaseTransformImpl for SceneDetector {
    const MODE: gst_base::subclass::BaseTransformMode =
        gst_base::subclass::BaseTransformMode::AlwaysInPlace;
    const PASSTHROUGH_ON_SAME_CAPS: bool = true;
    const TRANSFORM_IP_ON_PASSTHROUGH: bool = true;
}

impl VideoFilterImpl for SceneDetector {
    fn transform_frame_ip_passthrough(
        &self,
        frame: &gst_video::VideoFrameRef<&gst::BufferRef>,
    ) -> Result<gst::FlowSuccess, gst::FlowError> {
        let gop_size = self.settings.lock().unwrap().gop_size;
        let buffer_offset = frame.buffer().offset() as u32;
        let gop_index = buffer_offset / gop_size;

        if (buffer_offset % gop_size) != 0 {
            return Ok(gst::FlowSuccess::Ok);
        }

        gst::debug!(CAT, "Send SceneNewHypeEvent {gop_index} {gop_size} ");
        let event = SceneNewHypeEvent::new(gop_index, gop_size);
        if !self.send_event(event) {
            //Err(gst::FlowSuccess::Ok)
            unimplemented!();
        }

        Ok(gst::FlowSuccess::Ok)
    }
}
