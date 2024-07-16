use gst::glib;
use gst::prelude::*;

mod imp;

glib::wrapper! {
    pub struct Hype(ObjectSubclass<imp::Hype>) @extends gst::Bin, gst::Element, gst::Object;
}

pub fn register(plugin: &gst::Plugin) -> Result<(), glib::BoolError> {
    gst::Element::register(
        Some(plugin),
        "hype",
        gst::Rank::NONE,
        Hype::static_type(),
    )
}
