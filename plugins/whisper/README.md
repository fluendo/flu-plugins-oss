# Whisper

Plugin to transcribe audio to text based on WhisperC++ from OpenAI:

# Build
```
meson setup builddir -Dwhisper=enabled --auto-features=disabled
ninja -C builddir
```

# Download model
```
./subprojects/whispercpp/models/download-ggml-model.sh base
```

# Usage
Showing the dump transcribed data
```
gst-launch-1.0 --gst-plugin-path builddir/plugins/whisper/ \
    filesrc location=subprojects/whispercpp/samples/jfk.wav ! \
    decodebin ! \
    audioconvert ! \
    audio/x-raw,format=F32LE ! \
    whisper silent=FALSE model-path=./subprojects/whispercpp/models/ggml-base.bin
```

Transcribe as captions
```
gst-launch-1.0 --gst-plugin-path builddir/plugins/whisper/ \
    filesrc location=subprojects/whispercpp/samples/jfk.wav ! \
    decodebin ! \
    audioconvert ! \
    audio/x-raw,format=F32LE ! \
    whisper model-path=./subprojects/whispercpp/models/ggml-base.bin ! \
    textrender ! \
    videoconvert ! \
    autovideosink
```
