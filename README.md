# dlb_lip

The Latency Indication Protocol (LIP)

## Description

The Latency Indication Protocol (LIP) was developed as a test kit by Dolby Laboratories Inc. in response
to the lack of a capable HDMI signaling solution for A/V latency. The core idea is that LIP provides
full transparency about the audio and video latency of all devices in an HDMI setup and thereby allows
to apply necessary alignment at the source, where video is still compressed and thereby less costly to buffer.

The latency indication protocol is an extension of the Consumer Electronics Control (CEC) protocol
designed for improved audio/video (A/V) synchronization when the audio and video content is
decoded and rendered on different devices interconnected via HDMI.

A source device using LIP that has received information about downstream device latencies can optimally
compensate the difference and ensure a proper A/V synchronization at the rendering points.

The functionality of the Latency Indication Protocol includes querying for the support of the protocol,
sending information about the audio and video latency values of sink devices, and providing dynamic
updates to the source device on changes in the cluster or processing mode of a sink device.
The Latency Indication Protocol is designed for specific device setups.

The following scenarios are supported:
[1] TV audio output connected to an audio system via ARC or eARC
[2] Source device connected directly to a TV via HDMI
[3] TV as the hub, connecting to a source device via HDMI, as well as to an audio system via ARC or eARC


## Usage

To use the dlb_lip, the dlb_liptool and libcec projects are required.

## License

The dlb_lip project is distributed under the 3-Clause BSD License.
