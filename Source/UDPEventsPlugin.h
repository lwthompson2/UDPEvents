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

	/** If the processor has a custom editor, this method must be defined to instantiate it. */
	AudioProcessorEditor *createEditor() override;

	void registerParameters() override;

	/** Called every time the settings of an upstream plugin are changed.
		Allows the processor to handle variations in the channel configuration or any other parameter
		passed through signal chain. The processor can use this function to modify channel objects that
		will be passed to downstream plugins. */

	/** Update internal variables in respons selections made in the editor UI. */
	void parameterValueChanged(Parameter *param) override;

	/** Start the background UDP thread. */
	bool startAcquisition() override;

	/** Stop the background UDP thread. */
	bool stopAcquisition() override;

	/** Check for UDP messages on a separate thread. */
	void run() override;

	/** Defines the functionality of the processor.
		The process method is called every time a new data buffer is available.
		Visualizer plugins typically use this method to send data to the canvas for display purposes */
	void process(AudioBuffer<float> &buffer) override;

	/** Handles events received by the processor
		Called automatically for each received event whenever checkForEvents() is called from
		the plugin's process() method */
	void handleTTLEvent(TTLEventPtr event) override;

private:
	/** Editable settings.*/
	String hostToBind = "127.0.0.1";
	uint16 portToBind = 12345;
	uint16 streamId = 0;
	uint8 syncLine = 0;
	uint8 syncStateIndex = 0;

	/** Hold events received via UDP, until processing them into the selected data stream. */
	struct SoftEvent
	{
		/** 0x01 = "TTL", 0x02 = "Text". */
		uint8 type = 0;

		/** High-precision timestamp from the client's point of view. */
		double clientSeconds = 0.0;

		/** Acquisition message recv timestamp. */
		int64 systemTimeMilliseconds = 0;

		/** 0-based line number for TTL events. */
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

	/** Keep track of real and soft sync events and convert client soft secs to local sample numbers. */
	struct SyncEstimate
	{
		/** Sample number of a real, local, sampled, sync event. */
		int64 syncLocalSampleNumber = 0;

		/** Timestamp of a real, local, sampled, sync event, I believe in ms. */
		int64 syncLocalTimestamp = 0;

		/** Timestamp of a corresponding soft, external sync event. */
		double syncSoftSecs = 0.0;

		/** Estimate of the local sample number that corresponds to soft timestamp 0.0.*/
		int64 softSampleZero = 0;

		/** Reset and begin a new estimate. */
		void clear()
		{
			syncLocalSampleNumber = 0;
			syncLocalTimestamp = 0;
			syncSoftSecs = 0.0;
			softSampleZero = 0;
		}

		/** Convert a soft, external timestamp to the nearest local sample number. */
		int64 softSampleNumber(double softSecs, float localSampleRate)
		{
			int64 sampleNumber = softSecs * localSampleRate + softSampleZero;
			LOGD("SyncEstimate computed sampleNumber ", (long)sampleNumber, " for softSecs ", softSecs, " at localSampleRate ", localSampleRate);
			return sampleNumber;
		}

		/** Record the sample number of a real sync event, return whether the sync estimate is now complete. */
		bool recordLocalSampleNumber(int64 sampleNumber, float localSampleRate)
		{
			syncLocalSampleNumber = sampleNumber;
			LOGD("SyncEstimate got syncLocalSampleNumber ", (long)sampleNumber, " at localSampleRate ", localSampleRate);
			if (syncSoftSecs)
			{
				softSampleZero = syncLocalSampleNumber - syncSoftSecs * localSampleRate;
				LOGD("SyncEstimate computed softSampleZero ", (long)softSampleZero);
				return true;
			}
			return false;
		}

		/** Record the timestamp of a real sync event, return whether the sync estimate is now complete. */
		bool recordLocalTimestamp(int64 timeStamp, float localSampleRate)
		{
			syncLocalTimestamp = timeStamp;
			LOGD("SyncEstimate got syncLocalTimestamp ", (int64)timeStamp, " at localSampleRate ", localSampleRate);
			if (syncSoftSecs)
			{
				return true;
			}
			return false;
		}

		/** Record the timestamp of a soft sync event, return whether the sync estimate is now complete. */
		bool recordSoftTimestamp(double softSecs, float localSampleRate)
		{
			syncSoftSecs = softSecs;
			LOGD("SyncEstimate got syncSoftSecs ", syncSoftSecs, " at localSampleRate ", localSampleRate);
			if (syncLocalSampleNumber)
			{
				softSampleZero = syncLocalSampleNumber - syncSoftSecs * localSampleRate;
				LOGD("SyncEstimate computed softSampleZero ", (long)softSampleZero);
				return true;
			}
			return false;
		}
	};
	SyncEstimate workingSync;
	std::list<SyncEstimate> syncEstimates;

	//** Check whether incoming event data matches TTL event selection in the UI. */
	bool filterSyncEvent(uint8 line, bool state);

	/** Add a text event to represent a completed sync estimate. */
	void addEventForSyncEstimate(struct SyncEstimate syncEstimate);

	/** Convert a soft timestamp to the nearest local sample number using the most relevant sync estimate. */
	int64 softSampleNumber(double softSecs, float localSampleRate);
};

#endif
