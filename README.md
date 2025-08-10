# QON - Unofficial "Quite OK Animation Format” for fast, lossless compressed images in an animation container

Header-only MIT licensed library for C/C++


## Why?

This project has the same rationale as QOI, the “Quite OK Image Format”. For a description of that format, see
the official site:

[qoiformat.org](https://qoiformat.org)

[github.com/phoboslab/qoi](https://github.com/phoboslab/qoi)

QOI is a fantastic lossless, light-weight image compression format, and a viable drop-in replacement for PNG,
with similar compression ratios, but much faster encoding and decoding, all in a few hundred lines of code.

This format, QON, the "Quite OK aNimation Format” (because [QOA was taken](https://qoaformat.org/)) applies the
work done by QOI to containers of images, with support for animation and full alpha transparency.

This format is NOT indended to replace video container formats like [MP4](https://en.wikipedia.org/wiki/MP4_file_format).
It has two key use-cases in mind:
- A simple, lightweight, viable alternative to animated image files like [GIF](https://en.wikipedia.org/wiki/GIF)
  and [APNG](https://en.wikipedia.org/wiki/APNG).
- An indexable frame store in memory or on disk, suitable for applications, games, or embedded systems.


## How?

Like QOI, this format is designed to be simple, lightweight, and easy to implement. It does this using a trimmed
and improved QOI variant to store frames, with a simple frame index at the start of the file.

To achieve good compression for animations, inter-frame compression is supported by optionally making the QOI
concept of the current value relative to the contents of the previous frame. This gives good inter-frame
compression results with very little effort, and almost no overhead on either compression or decompression.
Inter-frame compression can be enabled or disabled on a per-frame basis, allowing for key frames.

The leading frame index gives direct offset locations to the start of each frame, and also records the inter-frame
compression settings. This gives several key benefits:
- Allows direct "seeking" to a given frame or point in time without needing to scan through the file
- Allows preceding "key frames" to be easily located from a given frame without having to examine the frame data
- Allows frames to be re-used in the animation sequence
- Makes QON useful not just for animations, but also as a general-purpose keyed image store, such as in games
  or embedded environments.


## QOI changes

This repository contains an image format unofficially called QOI2, which is used by QON internally, but can also
be used as a drop-in replacement for QOI. This code is modified from the reference QOI implementation, with the
following changes:

- QOI header optional. Since there is a header for the QON container, each frame doesn't require its own header.
- QOI footer removed. The reference QOI encoder added a trailing 8 bytes after each file, which wasn't part of
  the QOI specification. This is removed.
- Compression improvements. The QOI format contained [key weaknesses](https://github.com/phoboslab/qoi/issues/191)
  with both long repeated runs and successive uncompressible pixels. These issues are addressed here, giving a
  4% size reduction over QOI on the [benchmark suite](https://qoiformat.org/benchmark) (from 28.2% to 27.2%),
  reducing the compression gap to PNG by 25%, with virtually no effect on encoding and decoding times.
- Reference image. Encoding and decoding can be tied back to a reference image, which is used for inter-frame
  compression by QON.
- Little endian. The original QOI spec used big endian notation in header fields. This has been changed to use
  little endian notation, making it easier to optimize on modern platforms.


## Future

I made this format because I needed it, and I felt it was best to share it with others. There are currently no
plans by me to heavily promote this format or try and push its adoption. If you find it useful however, feel free
to implement it wherever it is useful for your purposes to do so.
