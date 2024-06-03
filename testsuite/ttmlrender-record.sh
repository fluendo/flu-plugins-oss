# ./ttmlrender-record.sh infile outfile
echo $1 -\> $2
gst-launch-0.10 filesrc location=$1 ! ttmlrender force-buffer-clear=1 ! video/x-raw-rgb,width=320,height=240 ! gdppay ! filesink location=$2 > /dev/null
