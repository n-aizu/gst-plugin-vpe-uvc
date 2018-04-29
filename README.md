gst-plugin-vpe-uvc
=============

About
-----

gst-plugin-vpe-uvc is a GStreamer plugin for using the hardware accelerated color-conversion using VPE on TI AM572x processors.
This plugin is designed to used with usb camera(UVC driver).
And this plugin is designed to used with sink element which does not support dma-buf.

There is a [problem about dma-buf sharing between UVC and VPE driver](https://e2e.ti.com/support/arm/sitara_arm/f/791/t/523614).
This plugin solved this problem with CMEM and V4L2 User Pointer.

This plugin has been tested with BeagleBoard-X15.

How to use it
-----

First of all, install [debian image](https://debian.beagleboard.org/images/bbx15-debian-9.3-lxqt-armhf-2018-01-28-4gb.img.xz) to BeagleBoard-X15.
Then, boot BeagleBoard-X15 and install dependencies.

    sudo apt-get install libgstreamer1.0-0 libgstreamer1.0-dev gstreamer1.0-tools gstreamer1.0-plugins-base \
    libgstreamer-plugins-base1.0-0 libgstreamer-plugins-base1.0-dev cmem-dev gtk-doc-tools libxext-dev

You should download [gst-plugins-good 1.10.4](https://github.com/GStreamer/gst-plugins-good/commit/b32b503e56ed213b1e85b30f38fb584c3f0876f3) and [build it without libv4l2](https://developer.ridgerun.com/wiki/index.php?title=Gstreamer1.0_and_V4L2_UserPtr#A_Workaround).

Next, build gst-plugin-vpe-uvc:

    ./autogen.sh
	make

Example call:

    gst-launch-1.0 --gst-plugin-path=./src/.libs v4l2src io-mode=userptr device=/dev/video1 ! video/x-raw,format=NV12,width=1920,height=1080,framerate=30/1 ! acceltransform ! xvimagesink

DISCLAIMER
-----

All tools/sources in this repo are provided "AS IS" without warranty of any kind.
