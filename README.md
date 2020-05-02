# Introduction

The [reMarkable tablet](https://remarkable.com) provides a very good
experience for digital writing/drawing. Unfortunately, sharing drawing
in real-time is difficult, especially on Linux, where the official
reMarkable cloud streaming does not work.

Since the reMarkable uses an e-paper display, it's possible to extract
high quality damage tracking information from the kernel, using
[mxc_epdc_fb_damage](https://github.com/peter-sa/mxc_epdc_fb_damage).
The availability of this information means that efficiently copying
updates off of the device should be quite easy. This repo uses
[libvncserver](https://libvnc.github.io) and
[mxc_epdc_fb_damage](https://github.com/peter-sa/mxc_epdc_fb_damage)
to implement a simple well-performing VNC server. This allows
streaming the display contents off the device to any VNC/RFB client.
Performance is quite good, at least when using the ZRLE compression
built into libvncserver; over USB connections, drawing is quite
smooth, with negligible latency. Large framebuffer updates, such as
opening or closing a notebook, are, in fact, frequently displayed on
the client before the reMarkable screen has finished refreshing. The
code has been less well tested over WiFi connections; drawing over an
SSH tunnel over a WiFi connection seems to occasionally stutter due to
unpredictable latency, although unencrypted connections still seem to
perform well.

# Building

The supported way to build this is via the
[Nix](https://nixos.org/nix) package manager, through the
[nix-remarkable](https://github.com/peter-sa/nix-remarkable)
expressions. To build just this project via `nix build` from this
repo, download it into the `pkgs/` directory of `nix-remarkable`.

For other systems, the commands needed to compile and link are
relatively simple, and given in [derivation.nix](./derivation.nix).
libvncserver (built for the reMarkable), reMarkable kernel headers,
and the `mxc_epdc_fb_damage.h` file from
[mxc_epdc_fb_damage](https://github.com/peter-sa/mxc_epdc_fb_damage)
are required to build it.

Prebuilt binaries are available in the [Releases
tab](https://github.com/peter-sa/rM-vnc-server/releases).

# Usage

Make sure that
[mxc_epdc_fb_damage](https://github.com/peter-sa/mxc_epdc_fb_damage)
is installed on the device. Copy `rM-vnc-server` to the device and run
it; this will start a vnc server listening on port 5900. Any VNC
client should be able to provide a (view-only) view of the tablet's
screen when pointed at the reMarkable's IP address and standard VNC
port (5900). [Vinagre](https://gitlab.gnome.org/GNOME/vinagre) and
[gst-libvncclient-rfbsrc](https://github.com/peter-sa/gst-libvncclient-rfbsrc)
have been tested.

Note that the server will bind to all available network interfaces by
default, so only run this if the tablet's WiFi is disconnected or
connected to a trusted network. iptables rules could be used to
restrict traffic.
