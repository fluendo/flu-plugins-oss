# Hype

HYbrid Parallel Encoder.

# Usage

    encoder="x264enc threads=1 b-adapt=false byte-stream=true speed-preset=1"
    gst-launch-1.0 -ve \
        videotestsrc \
            num-buffers=200 \
        ! hype \
            gop-size=5 \
            encoder-1='${encoder}' \
            encoder-2='${encoder}' \
            encoder-3='${encoder}' \
        ! fakesink



## Links

* Fluendo Blog post: https://fluendo.com/en/blog/hype-hybrid-parallel-encoder/
* GStreamer conference 2023 recording: https://gstconf.ubicast.tv/videos/hype-hybrid-parallel-encoder/
* GStreamer conference 2023 presentation: https://indico.freedesktop.org/event/5/contributions/235/
