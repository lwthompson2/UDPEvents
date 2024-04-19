# UDP Events

This repo is for an [Open Ephys GUI](https://github.com/open-ephys/plugin-GUI) processor plugin.
It's based on Open Ephys [processor plugin template](https://github.com/open-ephys-plugins/processor-plugin-template).
For more info on Open Ephys and plugins in general, please see the [Open Ephys docs](https://open-ephys.github.io/gui-docs/Tutorials/How-To-Make-Your-Own-Plugin.html).

This UDP Events plugin can be used to inject TTL and Text events into an existing Open Ephys data stream via a UDP network socket.
It can align "soft" timestamps from an external system to "real" sample numbers in the existing data stram with high precision.


## Downloading and installing

Here are some notes on how to get and install the plugin -- so far.  This is still a work in progress.

We're building the plugin for different platforms using GitHub Actions.
Here's a Linux example, with Windows coming soon.

To download a built plugin go to the latest build for your platform at the repo [actions](https://github.com/benjamin-heasly/UDPEvents/actions) page.
On the build results page, look for the "Artifacts" section. Download an artifact with a name like `UDPEvents-linux_test-8-API8.zip` and unzip it.

On Linux the `.zip` file contains a dynamic library file called `UDPEvents.so` file.  This is the plugin.

Copy the `UDPEvents.so` into the `plugins/` subdir of your Open Ephys GUI installation.
You might need to figure out where this is.
For example, using the [official Ubuntu installer](https://open-ephys.github.io/gui-docs/User-Manual/Installing-the-GUI.html) this `plugins/` subdir ended up at `/usr/local/bin/open-ephys-gui/plugins/`.

So a copy command like this shoudl work:

```
sudo cp UDPEvents.so /usr/local/bin/open-ephys-gui/plugins/
```

### subdir ownership

For some reason, on my/Ben's laptop `/usr/local/bin/open-ephys-gui` was owned by the `docker` user.
Was this a weird quirk of the Open Ephys installer?
Either way, I also had to fix the ownership of the subdir like the rest of `/usr/local/bin/`.

```
sudo chown -R root:root /usr/local/bin/open-ephys-gui/
```

After fixing that, the copy command above worked fine, and I could launch `open-ephys` and see "UDP Events" along with other plugins, and add it to a signal chain.

## Integrating with clients

The UDP Events plugin will act like a server, starting and stopping whenever Open Ephys starts and stops acquisition.
During acquisition UDP Events will bind a host address and UDP port and wait for messages to arrive from a client.
As each message arrives, UDP Events will:

 - take a local timestamp
 - reply to the client with this timestamp, as an acknowledgement
 - parse the message as a TTL or Text event
 - save the message in a queue, to be added to an existing data stream along with other data being processed

The ack timestamps are informational only.
Clients can use them to check that they are connecting to UDP Events as expected, and can expect that the timesamps will increase over time.

Each event should arrive in a single UDP message, with binary data in one of two formats, with details below.
For a working example client in Python, see [test-client.py](./test-client.py)

### TTL Events

TTL event messages should have exactly 11 bytes:

| byte index | number of bytes | data type | description |
| --- | --- | --- | --- |
| 0 | 1 | uint8 | **message type** for TTL messages this is the literal value `0x01` |
| 1 | 8 | double | **timestamp** event time in seconds from the client's point of view |
| 9 | 1 | uint8 | **line number** 0-based line number for an Open Ephys TTL line (0-255) |
| 10 | 1 | uint8 | **line state** on/off state for the Open Ephys TTL line (nonzero is "on") |

### Text Events

Text event messages should start with exactly 11 header bytes, followed by a variable number of text bytes:

| byte index | number of bytes | data type | description |
| --- | --- | --- | --- |
| 0 | 1 | uint8 | **message type** for Text messages this is the literal value `0x02` |
| 1 | 8 | double | **timestamp** event time in seconds from the client's point of view |
| 9 | 2 | uint16 | **text length** byte length of text that follows (network byte order -- use [htons()](https://beej.us/guide/bgnet/html/#htonsman)) |
| 11 | **text length** | char | **text** message text encoded as ASCII or UTF-8 |

### Ack timestamps

UDP Events will reply to the sender of each message with an 8-byte acknowledgement:

| byte index | number of bytes | data type | description |
| --- | --- | --- | --- |
| 0 | 8 | double | **timestamp** ack time in seconds from the UDP Events point of view |

## Aligning with an existing data stream

UDP Events will align "soft" timestamps received in UDP messages to "real" sample numbers in a selected Open Ephys data stream.
This alignment can preserve high precision in the sample numbers -- more precise than each Open Ephys data block.
For this to work the client must send UDP TTL messages with the same **line number** as some existing TTL events on the selected stream.

TODO: more on this...