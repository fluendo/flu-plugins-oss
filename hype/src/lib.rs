use gst::glib;

mod hype;
mod outputselector;
mod scenecollector;
mod scenedetector;

#[derive(Debug)]
pub struct SceneNewHypeEvent {
    pub gop_index: u32,
    pub gop_size: u32,
}

impl SceneNewHypeEvent {
    const EVENT_NAME: &'static str = "scene-new-hype-event";

    #[allow(clippy::new_ret_no_self)]
    pub fn new(gop_index: u32, gop_size: u32) -> gst::Event {
        let s = gst::Structure::builder(Self::EVENT_NAME)
            .field("gop_index", gop_index)
            .field("gop_size", gop_size)
            .build();
        gst::event::CustomDownstream::new(s)
    }

    pub fn parse(ev: &gst::EventRef) -> Option<SceneNewHypeEvent> {
        match ev.view() {
            gst::EventView::CustomDownstream(e) => {
                let s = match e.structure() {
                    Some(s) if s.name() == Self::EVENT_NAME => s,
                    _ => return None, // No structure in this event, or the name didn't match
                };

                let gop_index = s.get::<u32>("gop_index").unwrap();
                let gop_size = s.get::<u32>("gop_size").unwrap();
                Some(SceneNewHypeEvent {
                    gop_index,
                    gop_size,
                })
            }
            _ => None, // Not a custom event
        }
    }
}

fn plugin_init(plugin: &gst::Plugin) -> Result<(), glib::BoolError> {
    scenedetector::register(plugin)?;
    outputselector::register(plugin)?;
    scenecollector::register(plugin)?;
    hype::register(plugin)?;
    Ok(())
}

gst::plugin_define!(
    hype,
    env!("CARGO_PKG_DESCRIPTION"),
    plugin_init,
    concat!(env!("CARGO_PKG_VERSION"), "-", env!("COMMIT_ID")),
    "LGPL",
    env!("CARGO_PKG_NAME"),
    env!("CARGO_PKG_NAME"),
    env!("CARGO_PKG_REPOSITORY"),
    env!("BUILD_REL_DATE")
);
