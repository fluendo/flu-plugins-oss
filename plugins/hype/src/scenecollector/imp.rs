use gst::glib;
use gst::prelude::*;
use gst::subclass::prelude::*;

use once_cell::sync::Lazy;
use parking_lot::Mutex;
use std::collections::HashMap;

use crate::SceneNewHypeEvent;

static CAT: Lazy<gst::DebugCategory> = Lazy::new(|| {
    gst::DebugCategory::new(
        "hypescenecollector",
        gst::DebugColorFlags::empty(),
        Some("SceneCollector Element"),
    )
});

struct SceneBuffers {
    buffers: gst::BufferList,
    scene_completed: bool,
}

struct Scenes {
    pad_scenes: Mutex<HashMap<String, u32>>,
    scene_buffers: Mutex<HashMap<u32, SceneBuffers>>,
    next_scene_to_send: Mutex<u32>,
}

pub struct SceneCollector {
    srcpad: gst::Pad,
    sinkpad_counter: Mutex<usize>,
    scenes: Scenes,
}

impl SceneBuffers {
    fn new() -> Self {
        Self {
            buffers: gst::BufferList::new(),
            scene_completed: false,
        }
    }
}

impl Scenes {
    fn new() -> Self {
        Self {
            pad_scenes: Mutex::new(HashMap::new()),
            scene_buffers: Mutex::new(HashMap::new()),
            next_scene_to_send: Mutex::new(0),
        }
    }

    fn push_buffer(&self, pad_name: String, buffer: gst::Buffer) {
        let pad_scenes = self.pad_scenes.lock();
        let mut scene_buffers = self.scene_buffers.lock();

        let current_scene = pad_scenes.get(&pad_name).unwrap();

        gst::debug!(CAT, "Adding buffer in bufferlist of scene {current_scene}");

        scene_buffers
            .get_mut(current_scene)
            .unwrap()
            .buffers
            .get_mut()
            .unwrap()
            .add(buffer);
    }

    fn pop_buffers(&self) -> Vec<gst::BufferList> {
        let mut scene_buffers = self.scene_buffers.lock();
        let mut next_scene_to_send = self.next_scene_to_send.lock();
        let aux = *next_scene_to_send;

        gst::debug!(
            CAT,
            "Checking buffers to send from scene {next_scene_to_send}"
        );

        let mut ret = vec![];
        for scene in aux.. {
            if let Some(sc) = scene_buffers.get(&scene) {
                if !sc.scene_completed {
                    gst::debug!(CAT, "Scene {scene} is not completed");
                    break;
                }
                gst::debug!(CAT, "Scene {scene} is completed. It will be sent");
                let sc = scene_buffers.remove(&scene).unwrap();
                ret.push(sc.buffers);
                *next_scene_to_send = scene + 1;
            } else {
                gst::debug!(CAT, "Scene {scene} don't exit");
                break;
            }
        }

        ret
    }

    // TODO: make this method more roboust
    // new_scene("sink_0", 0)
    // new_scene("sink_0", 0)
    //
    // new_scene("sink_0", 0)
    // new_scene("sink_1", 0)
    fn new_scene(&self, pad_name: String, new_scene: u32) {
        gst::debug!(CAT, "New scene {new_scene} in {pad_name}");
        let mut pad_scenes = self.pad_scenes.lock();
        let mut scene_buffers = self.scene_buffers.lock();

        if let Some(prev_scene) = pad_scenes.insert(pad_name, new_scene) {
            // If there was a previous scene, mark it as completed
            gst::debug!(CAT, "Set scene {prev_scene} as completed");
            scene_buffers.get_mut(&prev_scene).unwrap().scene_completed = true;
        }
        scene_buffers.insert(new_scene, SceneBuffers::new());
    }

    fn finish_scene(&self, pad_name: String) {
        gst::debug!(CAT, "Finish scene in {pad_name}");
        let mut pad_scenes = self.pad_scenes.lock();
        let mut scene_buffers = self.scene_buffers.lock();

        if let Some(prev_scene) = pad_scenes.remove(&pad_name) {
            // If there was a previous scene, mark it as completed
            gst::debug!(CAT, "Set scene {prev_scene} as completed");
            scene_buffers.get_mut(&prev_scene).unwrap().scene_completed = true;
        }
    }

    fn pending_scene_len(&self) -> usize {
        let scene_buffers = self.scene_buffers.lock();
        scene_buffers.len()
    }
}

impl SceneCollector {
    fn sink_chain(
        &self,
        pad: &gst::Pad,
        buffer: gst::Buffer,
    ) -> Result<gst::FlowSuccess, gst::FlowError> {
        gst::log!(CAT, obj: pad, "Handling buffer {:?}", buffer);
        self.scenes.push_buffer(pad.name().to_string(), buffer);
        for buffer_list in self.scenes.pop_buffers() {
            // TODO: handle error here
            self.srcpad.push_list(buffer_list).unwrap();
        }

        Ok(gst::FlowSuccess::Ok)
    }

    fn sink_event(&self, pad: &gst::Pad, event: gst::Event) -> bool {
        gst::log!(CAT, obj: pad, "Handling event {:?}", event);
        let pad_name = pad.name().to_string();
        if let Some(event) = SceneNewHypeEvent::parse(&event) {
            self.scenes.new_scene(pad_name, event.gop_index);
            return true;
        }

        if let gst::EventView::Caps(_) = &event.view() {
            //TODO rethink
            gst::debug!(CAT, "Push caps event downstream");
            return self.srcpad.push_event(event);
        }

        if let gst::EventView::Eos(_) = &event.view() {
            gst::debug!(CAT, "Handled EOS in pad {pad_name}");
            self.scenes.finish_scene(pad_name);
            for buffer_list in self.scenes.pop_buffers() {
                // TODO: handle error here
                self.srcpad.push_list(buffer_list).unwrap();
            }

            // TOOD check if is a good idea
            if self.scenes.pending_scene_len() != 0 {
                return true;
            }
        }
        gst::Pad::event_default(pad, Some(&*self.obj()), event)
    }

    fn sink_query(&self, pad: &gst::Pad, query: &mut gst::QueryRef) -> bool {
        gst::log!(CAT, obj: pad, "Handling query {:?}", query);
        self.srcpad.peer_query(query)
    }
}

#[glib::object_subclass]
impl ObjectSubclass for SceneCollector {
    const NAME: &'static str = "GstSceneCollector";
    type Type = super::SceneCollector;
    type ParentType = gst::Element;

    fn with_class(klass: &Self::Class) -> Self {
        let templ = klass.pad_template("src").unwrap();
        let srcpad = gst::Pad::from_template(&templ);

        Self {
            srcpad,
            sinkpad_counter: Mutex::new(0),
            scenes: Scenes::new(),
        }
    }
}

impl ObjectImpl for SceneCollector {
    fn constructed(&self) {
        self.parent_constructed();

        let obj = self.obj();
        obj.add_pad(&self.srcpad).unwrap();
    }
}

impl GstObjectImpl for SceneCollector {}

impl ElementImpl for SceneCollector {
    fn metadata() -> Option<&'static gst::subclass::ElementMetadata> {
        static ELEMENT_METADATA: Lazy<gst::subclass::ElementMetadata> = Lazy::new(|| {
            gst::subclass::ElementMetadata::new(
                "Hype SceneCollector",
                "Video/Generic",
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
                "sink_%u",
                gst::PadDirection::Sink,
                gst::PadPresence::Request,
                &caps,
            )
            .unwrap();

            vec![src_pad_template, sink_pad_template]
        });

        PAD_TEMPLATES.as_ref()
    }

    fn request_new_pad(
        &self,
        _templ: &gst::PadTemplate,
        _name: Option<&str>,
        _caps: Option<&gst::Caps>,
    ) -> Option<gst::Pad> {
        let mut sinkpad_counter = self.sinkpad_counter.lock();
        let sinkpad_id = *sinkpad_counter;
        *sinkpad_counter += 1;

        //TODO use template of the argument
        let templ = self.obj().pad_template("sink_%u").unwrap();
        let sinkpad = gst::Pad::builder_from_template(&templ)
            .name(format!("sink_{sinkpad_id}").as_str())
            .chain_function(|pad, parent, buffer| {
                SceneCollector::catch_panic_pad_function(
                    parent,
                    || Err(gst::FlowError::Error),
                    |scenecollector| scenecollector.sink_chain(pad, buffer),
                )
            })
            .event_function(|pad, parent, event| {
                SceneCollector::catch_panic_pad_function(
                    parent,
                    || false,
                    |scenecollector| scenecollector.sink_event(pad, event),
                )
            })
            .query_function(|pad, parent, query| {
                SceneCollector::catch_panic_pad_function(
                    parent,
                    || false,
                    |scenecollector| scenecollector.sink_query(pad, query),
                )
            })
            .build();

        sinkpad.set_active(true).unwrap();

        self.obj().add_pad(&sinkpad).unwrap();

        Some(sinkpad)
    }
}
