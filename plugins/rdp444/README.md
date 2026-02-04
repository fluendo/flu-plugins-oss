# RDP 4:4:4 GStreamer Elements (AI Generated)

## Overview

This plugin provides two complementary GStreamer elements that implement the Microsoft RDP AVC444v2 encoding scheme for splitting and combining 4:4:4 chroma video streams. This encoding is used in Remote Desktop Protocol (RDP) to efficiently transmit full chroma video over two separate 4:2:0 video streams.

### Elements

- **rdp444split** - Splits Y444 (4:4:4) video into two I420 (4:2:0) streams
- **rdp444combine** - Combines two I420 (4:2:0) streams back into Y444 (4:4:4) video

## rdp444split

### Description

The `rdp444split` element takes a single Y444 input stream and splits it into two I420 outputs according to the Microsoft RDP AVC444v2 specification. The element distributes the chroma information across two 4:2:0 streams in a way that allows perfect reconstruction.

### Pads

- **Sink pad**: `sink`
  - Caps: `video/x-raw, format=Y444`

- **Source pads**:
  - `src_yuv420` - Main view containing full luma and even-pixel chroma
  - `src_chroma420` - Auxiliary view containing odd-pixel chroma information
  - Caps: `video/x-raw, format=I420`

### Encoding Scheme

The element implements the Microsoft RDP AVC444v2 encoding specification:

**Main View (src_yuv420):**
- B1: Y₄₂₀(x,y) = Y₄₄₄(x,y) - Full luma plane
- B2: U₄₂₀(x,y) = U₄₄₄(2x, 2y) - Even pixel U samples
- B3: V₄₂₀(x,y) = V₄₄₄(2x, 2y) - Even pixel V samples

**Auxiliary View (src_chroma420):**
- B4: Y₄₂₀(x,y) = U₄₄₄(2x+1, y) - U odd columns (left half)
- B5: Y₄₂₀(W/2+x,y) = V₄₄₄(2x+1, y) - V odd columns (right half)
- B6: U₄₂₀(x,y) = U₄₄₄(4x, 2y+1) - U odd rows (left quarter)
- B7: U₄₂₀(W/4+x,y) = V₄₄₄(4x, 2y+1) - V odd rows (right quarter)
- B8: V₄₂₀(x,y) = U₄₄₄(4x+2, 2y+1) - U odd rows offset (left quarter)
- B9: V₄₂₀(W/4+x,y) = V₄₄₄(4x+2, 2y+1) - V odd rows offset (right quarter)

### Properties

None

## rdp444combine

### Description

The `rdp444combine` element performs the inverse operation of `rdp444split`. It takes two I420 input streams and reconstructs the original Y444 output by reversing the AVC444v2 encoding scheme.

### Pads

- **Sink pads**:
  - `sink_yuv420` - Main view (full luma + even chroma)
  - `sink_chroma420` - Auxiliary view (odd chroma samples)
  - Caps: `video/x-raw, format=I420`

- **Source pad**: `src`
  - Caps: `video/x-raw, format=Y444`

### Decoding Scheme

The element reconstructs Y444 by reversing the encoding:

- Y₄₄₄(x,y) = Y₄₂₀_main(x,y) - From B1: Full luma
- U₄₄₄(2x, 2y) = U₄₂₀_main(x,y) - From B2: Even pixel U
- V₄₄₄(2x, 2y) = V₄₂₀_main(x,y) - From B3: Even pixel V
- U₄₄₄(2x+1, y) = Y₄₂₀_aux(x,y) - From B4: U odd columns
- V₄₄₄(2x+1, y) = Y₄₂₀_aux(W/2+x,y) - From B5: V odd columns
- U₄₄₄(4x, 2y+1) = U₄₂₀_aux(x,y) - From B6: U odd rows
- V₄₄₄(4x, 2y+1) = U₄₂₀_aux(W/4+x,y) - From B7: V odd rows
- U₄₄₄(4x+2, 2y+1) = V₄₂₀_aux(x,y) - From B8: U odd rows offset
- V₄₄₄(4x+2, 2y+1) = V₄₂₀_aux(W/4+x,y) - From B9: V odd rows offset

### Properties

None

## Example Pipelines

### Round-trip Test

Complete pipeline that splits and recombines Y444 video:

```bash
gst-launch-1.0 -v videotestsrc ! \
  "video/x-raw,format=Y444,width=640,height=480,framerate=30/1" ! \
  rdp444split name=split \
  split.src_yuv420 ! queue ! combine.sink_yuv420 \
  split.src_chroma420 ! queue ! combine.sink_chroma420 \
  rdp444combine name=combine ! \
  "video/x-raw,format=Y444,width=640,height=480" ! \
  videoconvert ! autovideosink
```

This pipeline demonstrates:
1. Generating Y444 test pattern (640x480)
2. Splitting into two I420 streams (main + auxiliary)
3. Routing through queues for synchronization
4. Recombining back to Y444
5. Displaying the reconstructed video

### Viewing Split Output

To visualize the split streams (note: auxiliary view will appear corrupted as shown in the example image):

```bash
# View main stream
gst-launch-1.0 videotestsrc ! \
  "video/x-raw,format=Y444,width=640,height=480" ! \
  rdp444split name=split \
  split.src_yuv420 ! videoconvert ! autovideosink

# View auxiliary stream (chroma data packed into I420)
gst-launch-1.0 videotestsrc ! \
  "video/x-raw,format=Y444,width=640,height=480" ! \
  rdp444split name=split \
  split.src_chroma420 ! videoconvert ! autovideosink
```

**Note**: When viewing the auxiliary stream directly, the output will appear corrupted/fragmented because it contains packed chroma samples that are not meant to be displayed as a normal image. This is expected behavior - the auxiliary stream only makes sense when recombined with the main stream.

## Technical Details

### Implementation

Both elements are implemented as GStreamer bin elements:
- `rdp444split` extends `GstElement` with custom pad management
- `rdp444combine` extends `GstAggregator` for synchronized input handling

### Performance Considerations

- The split operation uses `memcpy()` for luma plane copying (efficient)
- Chroma resampling involves per-pixel operations
- Combine operation reconstructs full chroma resolution
- Both elements preserve timestamps and metadata

### Use Cases

1. **Remote Desktop Protocol**: Transmit 4:4:4 video over RDP using two H.264 streams
2. **High-quality video transmission**: Leverage existing 4:2:0 encoders for 4:4:4 content
3. **Video conferencing**: Maintain full chroma resolution in bandwidth-constrained scenarios
4. **Testing**: Verify codec implementations and video processing pipelines

## Building

The plugin is built as part of the flu-plugins-oss project:

```bash
meson setup builddir
ninja -C builddir
```

## References

- [MS-RDPEGFX]: Remote Desktop Protocol: Graphics Pipeline Extension
## License

Copyright (C) 2026 Fluendo <engineering@fluendo.com>

See LICENSE file for details.
