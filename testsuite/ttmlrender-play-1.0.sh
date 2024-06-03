gst-launch-1.0 filesrc location=$1 ! ttmlrender ! videoconvert ! timeoverlay ! autovideosink
