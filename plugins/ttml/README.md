# ttml

fluttml is a GStreamer plugin with elements to parse and render ttml formatted streams.

## Building

Follow these instructions to compile the project:

    meson -Dauto_features=disabled -Dttml=enabled -Dttml_build_ttmlparse=enabled -Dttml_build_ttmlrender=enabled builddir

## Sample files

Sample files tested with this plugin are provided in:

 - https://github.com/w3c/ttml1
 - https://github.com/IRT-Open-Source/irt-ebu-tt-d-application-samples

### Pipeline examples

The following examples use the  [W3C TTML1 testsuite](https://github.com/w3c/ttml1):

#### Animations

    gst-launch-1.0 filesrc location=testsuite/Animation/Animation001.xml ! ttmlrender ! videoconvert ! xvimagesink

#### Content
    gst-launch-1.0 filesrc location=testsuite/Content/Span004.xml ! ttmlrender ! videoconvert ! xvimagesink

#### Metadata

    gst-launch-1.0 -v filesrc location=testsuite/Metadata/Desc002.xml ! ttmlrender ! videoconvert ! xvimagesink

#### Parameters

    gst-launch-1.0 filesrc location=testsuite/Parameters/PixelAspectRatio003.xml ! ttmlrender ! videoconvert ! xvimagesink

#### Styling

    gst-launch-1.0 filesrc location=testsuite/Styling/Direction003.xml ! ttmlrender ! videoconvert ! xvimagesink

#### Timing

    gst-launch-1.0 filesrc location=testsuite/Timing/BeginEnd002.xml ! ttmlrender ! videoconvert ! xvimagesink

#### Mixing video stream and tttml stream

    gst-launch-1.0 videotestsrc pattern=ball ! video/x-raw,width=640,height=480 ! compositor name=comp ! videoconvert ! autovideosink \
    filesrc location=testsuite/Animation/Animation001.xml ! ttmlrender ! video/x-raw,width=640,height=480 ! comp.

#### Segmenting ttml DOM element into multiple ones

The *ttmlsegmentedparse* element allows to segment a single `<tt>...</tt>` (ttml) into multiple `<tt>..</tt>` DOM elements, each one a `GstBuffer`.

    gst-launch-1.0 filesrc location=testsuite/Styling/ZIndex002.xml  ! ttmlsegmentedparse ! fdsink


#### Outputting pango markup

The *ttmlparse* outputs `pango-markup` formatted text. The following shows an example on how to output plain data as pango markup format:

Parsing the entire ttml document, and showing up on standard ouptut:

    gst-launch-1.0 filesrc location=testsuite/Styling/ZIndex002.xml ! ttmlparse ! fdsink

Parsing the entire ttml document, and render it:

    gst-launch-1.0 filesrc location=testsuite/Styling/ZIndex002.xml ! ttmlparse ! textrender ! videoconvert ! autovideosink

#### Text overlaying

    gst-launch-1.0 filesrc location=testsuite/Styling/ZIndex002.xml ! ttmlparse ! text/x-raw,format=pango-markup ! txt.  \
    videotestsrc ! video/x-raw,height=768,width=1024 ! timeoverlay ! textoverlay name=txt ! autovideosink