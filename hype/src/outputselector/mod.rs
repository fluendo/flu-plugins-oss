use gst::glib;
use gst::prelude::*;

mod imp;

glib::wrapper! {
    pub struct HypeOutputSelector(ObjectSubclass<imp::HypeOutputSelector>) @extends gst::Bin, gst::Element, gst::Object;
}

pub fn register(plugin: &gst::Plugin) -> Result<(), glib::BoolError> {
    gst::Element::register(
        Some(plugin),
        "outputselector",
        gst::Rank::NONE,
        HypeOutputSelector::static_type(),
    )
}
