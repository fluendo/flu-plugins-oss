# ./ttmlparse-record.sh infile outfile
echo $1 -\> $2
gst-launch-0.10 filesrc location=$1 ! ttmlparse ! gdppay ! filesink location=$2 > /dev/null
