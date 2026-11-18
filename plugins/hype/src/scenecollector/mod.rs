use gst::glib;
use gst::prelude::*;

mod imp;

glib::wrapper! {
    pub struct SceneCollector(ObjectSubclass<imp::SceneCollector>) @extends gst::Element, gst::Object;
}

pub fn register(plugin: &gst::Plugin) -> Result<(), glib::BoolError> {
    gst::Element::register(
        Some(plugin),
        "scenecollector",
        gst::Rank::NONE,
        SceneCollector::static_type(),
    )
}
