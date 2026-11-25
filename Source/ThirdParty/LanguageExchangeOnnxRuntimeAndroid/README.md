# ONNX Runtime for Android

Used to run the detection model on the Quest, because NNE in UE 5.8 has no CPU runtime for Android.

- Version: **1.32.0** (the same version as the Python `onnxruntime` package in `Tools/ObjectDetection`).
- Source: Microsoft's Android package on Maven Central,
  `https://repo1.maven.org/maven2/com/microsoft/onnxruntime/onnxruntime-android/1.32.0/onnxruntime-android-1.32.0.aar`
  (SHA1 checked against Maven's `.sha1` file).
- Taken from the `.aar` (a zip file): `headers/*` → `Include/`, `jni/arm64-v8a/libonnxruntime.so` → `Lib/arm64-v8a/`.
  SHA256 of `libonnxruntime.so`: `CDB41E8B7D0E3CB5E000C705E9904979C3B11DE6E4614D2E4398E04AE8DCB1BB`.
- The library only depends on Android system libraries (`libc`, `libm`, `libdl`, `liblog`, `libandroid`) and links
  the C++ runtime statically.
- Licence: MIT, see `LICENSE.txt`.

To update: download the new `.aar`, replace `Include/` and `Lib/arm64-v8a/libonnxruntime.so`, and update this file.
