/*
------------------------------------------------------------------

This file is part of the Open Ephys GUI
Copyright (C) 2022 Open Ephys

------------------------------------------------------------------

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef UDPEVENTSPLUGIN_H_DEFINED
#define UDPEVENTSPLUGIN_H_DEFINED

#include <ProcessorHeaders.h>

class UDPEventsPlugin : public GenericProcessor, public Thread
{
public:
	/** The class constructor, used to initialize any members. */
	UDPEventsPlugin();

	/** The class destructor, used to deallocate memory */
	~UDPEventsPlugin();

	/** Start the background UDP thread. */
	bool startAcquisition() override;

	/** Stop the background UDP thread. */
	bool stopAcquisition() override;

	/** If the processor has a custom editor, this method must be defined to instantiate it. */
	AudioProcessorEditor *createEditor() override;

	/** Defines the functionality of the processor.
		The process method is called every time a new data buffer is available.
		Visualizer plugins typically use this method to send data to the canvas for display purposes */
	void process(AudioBuffer<float> &buffer) override;

	/** Handles events received by the processor
		Called automatically for each received event whenever checkForEvents() is called from
		the plugin's process() method */
	void handleTTLEvent(TTLEventPtr event) override;

	/** Update internal variables in respons selections made in the editor UI. */
	void parameterValueChanged(Parameter *param) override;

	/** Check for UDP messages on a separate thread. */
	void run() override;

private:
	/** Hold events received via UDP, until processing them into the selected data stream. */
	struct SoftEvent
	{
		/** 0x01 = "TTL", 0x02 = "Text". */
		uint8 type = 0;

		/** High-precision timestamp from the client's point of view. */
		double clientSeconds = 0.0;

		/** Line number for TTL events. */
		uint8 lineNumber = 0;

		/** On/off state for TTL events (nonzero means "on"). */
		uint8 lineState = false;

		/** Length in bytes for message text. */
		uint16 textLength = 0;

		/** Message text, treated as single-byte encoding UTF-8 or ASCII. */
		String text = "";
	};
	std::queue<SoftEvent> softEventQueue;
	CriticalSection softEventQueueLock;

	/** Pick the first TTL event channel on the selected stream, if any. */
	EventChannel *pickTTLChannel();

	struct SyncState
	{
		/** Sample number of a real, local, sampled sync event. */
		uint64 syncLocalSampleNumber = 0;

		/** Timestamp of a corresponding soft, external sync event. */
		double syncSoftSecs = 0.0;

		/** Estimate of the local sample number that corresponds to soft timestamp 0.0.*/
		uint64 softSampleZero = 0;

		/** Sync state is ready to estimate when it has data from both local and soft sources. */
		bool isReady()
		{
			return syncLocalSampleNumber != 0 && syncSoftSecs != 0.0;
		}

		/** Convert a soft, external timestamp to the nearest local sample number. */
		int64 softSampleNumber(double softSecs, float localSampleRate)
		{
			return softSecs * localSampleRate + softSampleZero;
		}

		/** Record the latest sample number of a real sync event.
		 * This will either invalidate the current sync estimate or complete the next sync estimate.
		 */
		void recordLocalSampleNumber(uint64 sampleNumber, float localSampleRate)
		{
			bool alreadyReady = isReady();
			syncLocalSampleNumber = sampleNumber;

			if (alreadyReady)
			{
				// This local sample number is for the next sync estimate,
				// which won't be ready until the next soft timestamp arrives.
				syncSoftSecs = 0.0;
				LOGD("UDP Sync State transition to not ready, with new local sample number ", sampleNumber);
			}
			else if (syncSoftSecs != 0.0)
			{
				// This local sample number completes the next sync estimate going forward.
				softSampleZero = syncLocalSampleNumber - syncSoftSecs * localSampleRate;
				LOGD("UDP Sync State transition ready, with new local sample number ", sampleNumber);
			}
		}

		/** Record the latest timestamp from a soft sync event.
		 * This will either invalidate the current sync estimate or complete the next sync estimate.
		 */
		void recordSoftTimestamp(double softSecs, float localSampleRate)
		{
			bool alreadyReady = isReady();
			syncSoftSecs = softSecs;

			if (alreadyReady)
			{
				// This soft timestamp is for the next sync estimate,
				// which won't be ready until the next local timestamp arrives.
				syncLocalSampleNumber = 0;
				LOGD("UDP Sync State transition to not ready, with new soft timestamp ", softSecs);
			}
			else if (syncLocalSampleNumber != 0)
			{
				// This soft timestamp completes the next sync estimate going forward.
				softSampleZero = syncLocalSampleNumber - syncSoftSecs * localSampleRate;
				LOGD("UDP Sync State transition to ready, with new soft timestamp ", softSecs);
			}
		}
	};
	SyncState syncState;

	/** Editable settings.*/
	String hostToBind = "127.0.0.1";
	uint16 portToBind = 12345;
	uint16 streamId = 0;
	uint8 syncLine = 0;
};

#endif
