use gst::glib;
use gst::prelude::*;

mod imp;

glib::wrapper! {
    pub struct InjectBin(ObjectSubclass<imp::InjectBin>)
        @extends gst::Bin, gst::Element, gst::Object;
}

pub fn register(plugin: &gst::Plugin) -> Result<(), glib::BoolError> {
    gst::Element::register(
        Some(plugin),
        "injectbin",
        gst::Rank::NONE,
        InjectBin::static_type(),
    )
}
