gst-launch-0.10 filesrc location=$1 ! ttmlparse ! gdppay ! filesink location=$1.gdp
