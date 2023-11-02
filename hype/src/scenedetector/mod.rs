use gst::glib;
use gst::prelude::*;

mod imp;

glib::wrapper! {
    pub struct SceneDetector(ObjectSubclass<imp::SceneDetector>) @extends gst_base::BaseTransform, gst::Element, gst::Object;
}

pub fn register(plugin: &gst::Plugin) -> Result<(), glib::BoolError> {
    gst::Element::register(
        Some(plugin),
        "scenedetector",
        gst::Rank::None,
        SceneDetector::static_type(),
    )
}
