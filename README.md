# UDP Events

This repo is for an [Open Ephys GUI](https://github.com/open-ephys/plugin-GUI) processor plugin.
It's based on the Open Ephys [processor plugin template](https://github.com/open-ephys-plugins/processor-plugin-template).
For more info on Open Ephys and plugins in general, please see the [Open Ephys docs](https://open-ephys.github.io/gui-docs/Tutorials/How-To-Make-Your-Own-Plugin.html).

The UDP Events plugin can inject TTL and text events into an existing Open Ephys data stream via a UDP network socket.
It can align the "soft" timestamps from an external system to "real" sample numbers in the existing data stream.
For TTL messages, the timestamp alignment can have high precision (more precise than Open Ephys data blocks).

![UDP Events Editor](./udp-events-editor.png)

UDP Events will bind a **HOST** address and **PORT** number where it can receive UDP messages.

It will look for real, upstream TTL events on a selected **LINE**, with the selected **STATE** and use these to align soft TTL and text messages received via UDP.

It will add aligned TTL and text messages to a selected Open Ephys data stream, shown here as "**example_...**".

## Downloading and Installing

We're building the plugin for different platforms using GitHub Actions.
To download a built plugin go to the latest build for your platform at this repo's [actions](https://github.com/benjamin-heasly/UDPEvents/actions) page.

On the latest build page, look for the "Artifacts" section.
Download an artifact with a name like `UDPEvents-system_version-API8.zip` and unzip it.

### Linux

On Linux the `.zip` file contains a dynamic library file called `UDPEvents.so`.
This is the plugin.
Copy `UDPEvents.so` into a folder where Open Ephys can find it.

The standard place would be your own user's `.config` directory, for example:

```
~/.config/open-ephys/plugins-api8
```

If for some reason that doesn't work, you might also try the `plugins/` subdir of your Open Ephys GUI installation.
You might need root / `sudo` permission to copy into this directory.
For example:

```
/usr/local/bin/open-ephys-gui/plugins/
```

Once the plugin is copied over you should be able to launch `open-ephys` and see "UDP Events" listed along with other plugins.

### Windows

On Windows the `.zip` file contains a dynamic library file called `UDPEvents.dll`.
This is the plugin.
Copy `UDPEvents.dll` into a folder where Open Ephys can find it.

The standard place would be the system's common app data directory, for example:

```
C:\Documents and Settings\All Users\Application Data\Open Ephys\plugins-api8\
```

If for some reason that doesn't work, you might also try the `plugins/` subdir of your Open Ephys GUI installation, for example:

```
C:\Program Files\Open Ephys\plugins
```

Once the plugin is copied over, you should be able to launch the Open Ephys GUI and see "UDP Events" listed along with other plugins.

### macOS

On macOS the `.zip` file contains a dynamic library file called `UDPEvents.bundle`.
This is the plugin.
Copy `UDPEvents.bundle` into a folder where Open Ephys can find it.

The standard place would be your own users's application data directory, for example:

```
~/Library/Application Support/open-ephys/plugins-api8
```

If for some reason that doesn't work, you might also try the `PlugIns/` subdir of the open-ephys app bundle you installed.
To do this you might need to use the terminal or right-click the open-ephys app and choose "Show Package Contents".
For example:

```
/Applications/open-ephys.app/Contents/PlugIns/
```

Once the plugin is copied over, you should be able to launch the Open Ephys GUI and see "UDP Events" listed along with other plugins.

## Integrating with Clients

The UDP Events plugin will act like a server that starts and stops whenever Open Ephys starts and stops acquisition.
During acquisition UDP Events will bind its **HOST** address and UDP **PORT** and wait for messages to arrive from a client.
As each message arrives, UDP Events will:

 - take a local timestamp
 - reply to the client with this timestamp, as an acknowledgement
 - parse the message as either TTL or text
 - save the message in a queue, to be added to the selected data stream along with other signals and events

The ack timestamps are informational only.
Clients can use them to check that they are connecting to UDP Events as expected, and can expect that the timesamps will increase over time.

Clients should send events as a single UDP message each, with binary data in one of the two formats described below.
For a working example client in Python, see [test-client.py](./test-client.py) in this repo.

### TTL Events

TTL event messages should have exactly 11 bytes:

| byte index | number of bytes | data type | description |
| --- | --- | --- | --- |
| 0 | 1 | uint8 | **message type** for TTL messages this is the literal value `0x01` |
| 1 | 8 | double | **timestamp** event time in seconds (including fractions) from the client's point of view |
| 9 | 1 | uint8 | **line number** 0-based line number for an Open Ephys TTL line (0-255) |
| 10 | 1 | uint8 | **line state** on/off state for the Open Ephys TTL line (nonzero is "on") |

### Text Events

Text event messages should start with exactly 11 header bytes, followed by a variable number of text bytes:

| byte index | number of bytes | data type | description |
| --- | --- | --- | --- |
| 0 | 1 | uint8 | **message type** for text messages this is the literal value `0x02` |
| 1 | 8 | double | **timestamp** event time in seconds (including fractions) from the client's point of view |
| 9 | 2 | uint16 | **text length** byte length of text that follows (network byte order -- use [htons()](https://beej.us/guide/bgnet/html/#htonsman)) |
| 11 | **text length** | char | **text** message text encoded as ASCII or UTF-8 |

### Ack Timestamps

UDP Events will reply to the sender of each message with an 8-byte acknowledgement:

| byte index | number of bytes | data type | description |
| --- | --- | --- | --- |
| 0 | 8 | double | **timestamp** ack time in seconds from the UDP Events point of view |

## Data Stream Alignment

UDP Events will align soft timestamps received in UDP messages to real sample numbers in a selected Open Ephys data stream.
For TTL messages, the alignment preserves high timing precision -- more precise than the start of each Open Ephys data block.

### TTL Event Pairs

For this alignment to work the client must send TTL Event messages via UDP with the same **LINE** number as real, upstream TTL events on the same Open Ephys data stream.
The soft timestamp for these TTL event messages should be the client's best estimate of when the real TTL event actually occurred, from the client's point of view.

UDP Events will look for pairs of TTL events on the same **LINE** -- one from a UDP message and one from upstream in Open Ephys.
Optionally it can filter these events by a the line **STATE**: low, high, or both.
For each pair it will estimate and record a conversion from client soft timestamp to data stream sample number.

As other TTL and text messages arrive via UDP, UDP Events will convert their soft timestamps to the closest sample number on the selected data stream, and add them as events to the stream.

### Accuracy

Alignment accuracy will be limited by how well the client can measure when real TTL events actually occur, and report these measurements via UDP.
So, UDP Events will make the most sense when the client has solid timing and has control over both the UDP messages and the corresponding upstream events.
Such a client could enrich a single DIO line with various other "soft TTL" and text events.

![UDP Events DIO Example](./udp-events-open-ephys.png)

### Reading Data Downstream / Offline

#### TTL

For soft TTL messages received via UDP, no special handling should be required.  UDP Events will save these as events to the selected data stream with high-precision sample numbers.  These sample numbers should flow all the way to the Open Ephys data file (Binary, NWB, etc.) just like other TTL events.

#### Text

Text messages are a bit different.
As of writing (April 2024) Open Ephys only saves text events with relatively coarse, per-block timestamps.
For some situations this might be sufficient.

In case it's helpful to recover high-precision timing for text events, UDP Events appends timing info to the body of each text message it receives.

The format looks like this:

```
original message text@<client_soft_timestamp>=<stream_sample_number>
```

Downstream tools can looking for the delimiters `@` and `=` at the end of each message and parse out the details.  The `<client_soft_timestamp>` would be the raw value in seconds sent by the client.  The `<stream_sample_number>` would be an aligned, integer sample number on the selected data stream.

#### TTL Pair Text Events

In addition, UDP Events saves a separate text event for each TTL event pair it receives on **LINE**, as described above.
These record and expose the raw timing data that UDP Events uses when aligning client seconds to stream sample numbers.

These messages have a similar format:

```
UDP Events sync on line <LINE>@<client_soft_timestamp>=<stream_sample_number>
```

These always start with the same literal text: `UDP Events sync on line `.  The following `<LINE>` is the selected **LINE** number.  As above, `<client_soft_timestamp>` is the raw value in seconds sent by the client.  Here, the `<stream_sample_number>` is the *actual* sample number of an upstream TTL event on the same **LINE**.

## Testing

You can test UDP Events using a Python script like [test-client.py](./test-client.py) in this repo.
This script depends on [pyzmq](https://pypi.org/project/pyzmq/).
You might be able to install this with `pip install pyzmq`.

This script expects an Open Ephys signal chain with the following:

 - [File Reader](https://open-ephys.github.io/gui-docs/User-Manual/Plugins/File-Reader.html) -- Provide sample data and a data stream to work in.
 - [Network Events](https://open-ephys.github.io/gui-docs/User-Manual/Plugins/Network-Events.html) -- Create upstream TTL events for UDP Events to look for.
 - UDP Events -- this plugin!
 - [TTL Display Panel](https://open-ephys.github.io/gui-docs/User-Manual/Plugins/TTL-Panels.html) -- Blink as TTL events arrive.
 - [LFP Viewer](https://open-ephys.github.io/gui-docs/User-Manual/Plugins/LFP-Viewer.html) -- Show TTL events aligned with upstream sampled signals.
 - [Record Node](https://open-ephys.github.io/gui-docs/User-Manual/Plugins/Record-Node.html) -- Save data for detailed inspection.

![UDP Events Test Signal Chain](./udp-events-test-signal-chain.png)

### Phony Client
In this setup, [test-client.py](./test-client.py) will play the role of "client", with control over both Network Events and UDP Events.

The signal chain's File Reader and Network Events will work together to stand in for a genuine data source like an [Acquisition Board](https://open-ephys.github.io/gui-docs/User-Manual/Plugins/Acquisition-Board.html), which might record both sampled signals and TTL events with respect to a single clock.
Timing accuracy of this test setup will be limited by the relatively coarse, best-effort timing of Network Events with respect to upstream sample numbers.

Choose **LINE** 4 in the UDP Events settings editor, to match the line number used in `test-client.py`.

### Running a Test

To start a test press the Open Ephys Record button.
This will start data streaming from the File Reader's example file.
Waveforms should apear in the LFP Viewer window.

While acquisition is still running, execute the client script.

```
python test-client.py
```

This should run for about 10 seconds, then stop.  While running it will send a series of events to Open Ephys.

The script will send 10 pairs of TTL events on **LINE** 4.  For each pair:
 - a real/upstream event sent to Network Events
 - a soft UDP counterpart sent to UDP Events

The upstream events themselves should be visible as blinking lights in the TTL Display Panel, and transparent overlays in the LFP Viewer window.  UDP Events will use these event pairs to align other events, below, and instert them into the selected data stream.

The script will also send 10 pairs of soft TTL events on **LINE** 1:
 - one to turn line 1 on
 - antoher to turn line 1 off

These cycle line 1 as fast as the script can manage.
The cycle time will usually be too quick to see in the TTL Display Panel or LFP viewer.
But these events should be saved in the recorded data file with high timing precision -- perhaps receiving sample numbers that are 1 or 2 samples apart.

Along with TTL event mesages above, the script will send 10 text messages via UDP, which should also be saved in the data file.

If all this happens, then it seems UDP Events is working for you!
