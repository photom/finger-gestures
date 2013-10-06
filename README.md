fingure-gesture
===============

# Description
control something by finger gestures.
**experimental**

1. audio volume on ubuntu
1.1. 2fingures->turn down volume. 3fingures->turn up volume


# About license
This app uses NiTE2/OpenNI2 libraries and sample sources, Zhang-Suen Thinning, and OpenCV.
You shall comply with these licenses to use this app.

* NITE license:
 http://www.openni.org/nite-licensing-and-distribution-terms/
* Zhang-Suen Thinning:
 https://github.com/bsdnoobz/zhang-suen-thinning
* OpenCV
 http://opencv.org/

# Build

* preparation
    $ path-to/OpenNI-Linux-x64-2.2/install.sh
    $ . path-to/OpenNI-Linux-x64-2.2/OpenNIDevEnvironment
    $ path-to/NiTE-Linux-x64-2.2/install.sh
    $ . path-to/NiTE-Linux-x64-2.2/NiTEDevEnvironment

* Resease build
    $ make
* Debug build
    $ make CFG=Debug
