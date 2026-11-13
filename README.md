# flu-plugins-oss

Open-source GStreamer plugins provided by Fluendo

## Plugins

 - [hype](plugins/hype/README.md): HYbrid Parallel Encoder.
 - [fluttml](plugins/ttml/README.md): Parses and renders TTML files.

## Building

The Rust plugins are built with Cargo:

    cargo build --workspace --release
    make install

The C plugins are built with Meson:

    meson setup builddir
    meson compile -C builddir
    meson install -C builddir

Both builds are independent while the migration is in progress.
