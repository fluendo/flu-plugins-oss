gst-launch-0.10 filesrc location=$1 ! ttmlparse ! textoverlay name=o ! ffmpegcolorspace ! autovideosink videotestsrc ! video/x-raw-rgb,width=640,height=480 ! timeoverlay ! o.
