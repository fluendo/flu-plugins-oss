gst-launch-0.10 filesrc location=$1 ! ttmlrender ! ffmpegcolorspace ! timeoverlay ! autovideosink
