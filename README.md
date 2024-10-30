# CameraXSDL3

CameraXSDL3 is an Android application that integrates Android's CameraX API with SDL3 (Simple DirectMedia Layer) to capture, process, and render YUV images in real-time. This project leverages the Java Native Interface (JNI) to pass YUV data from the CameraX API in Java to native C code, where the images are processed and displayed using SDL3. CameraXSDL3 is free to use, modify, and distribute under the provided terms.

## Features
- **CameraX Integration**: Utilizes CameraX API for efficient image capture and analysis across Android devices.
- **Real-Time Image Processing**: Captures images in YUV format and processes them in C for display.
- **JNI and SDL3 Integration**: Combines Java and native C code seamlessly through SDL3 for optimized multimedia handling.
- **Open Source and Free to Use**: Distributed under permissive terms for both personal and commercial use.

## Why CameraX Instead of SDL3’s New Camera API?

SDL3 introduced a new camera API for cross-platform camera access. However, on Android, this API does not work consistently across all device models. The primary reason is likely due to SDL3’s reliance on the Android Camera2 API, which varies significantly across different devices and vendors, leading to compatibility issues.

To ensure compatibility and reliability across the Android ecosystem, this project uses **CameraX** instead of SDL3's camera API. CameraX is built on top of Camera2 and provides a higher-level API designed specifically for Android, handling device-specific variations to offer:
- **Device Compatibility**: CameraX offers robust support across a wide range of Android devices.
- **Consistent Image Processing**: Ensures that image analysis and real-time processing are reliable across devices.
- **Lifecycle Awareness**: Simplifies managing the camera lifecycle, integrating smoothly with Android’s architecture.

Using CameraX allows this project to maintain device compatibility and leverage SDL3’s rendering capabilities without the limitations posed by SDL3's native camera API on Android.

## Project Structure
- **Java Code**: The main Android activity `CameraXsdl3Activity.java` handles CameraX lifecycle and image processing, passing YUV data to native C functions.
- **C Code**: The file `camera.c`  contains SDL3 functions to manage rendering, image updates, and orientation adjustments.
- **JNI Bridge**: Connects Java and C for YUV data processing and rendering.

## Contact
- **Email**: epinot@yahoo.com

## Licence
MIT License

Copyright (c) 2024 Emmanuel Pinot

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
