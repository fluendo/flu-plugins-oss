i:\gstreamer-sdk\1.0\x86\bin\gst-launch-1.0.exe filesrc location=%1 ! ttmlparse ! textoverlay name=o ! videoconvert ! autovideosink videotestsrc ! video/x-raw,format=RGB,width=640,height=480 ! timeoverlay ! o.
