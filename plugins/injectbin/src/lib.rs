use gst::glib;

mod injectbin;

fn plugin_init(plugin: &gst::Plugin) -> Result<(), glib::BoolError> {
    injectbin::register(plugin)
}

gst::plugin_define!(
    injectbin,
    env!("CARGO_PKG_DESCRIPTION"),
    plugin_init,
    concat!(env!("CARGO_PKG_VERSION"), "-", env!("COMMIT_ID")),
    "LGPL",
    env!("CARGO_PKG_NAME"),
    env!("CARGO_PKG_NAME"),
    env!("CARGO_PKG_REPOSITORY"),
    env!("BUILD_REL_DATE")
);
