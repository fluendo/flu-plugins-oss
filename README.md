# Fluendo GStreamer OSS

Fluendo OSS repository
## Available plugins

### Whisper

Plugin to transcribe audio to text based on WhisperC++ from OpenAI:

```
meson builddir -Dwhisper builddir -Dwhispercpp-path=<whispercpp-library-path>
ninja -C builddir
```
