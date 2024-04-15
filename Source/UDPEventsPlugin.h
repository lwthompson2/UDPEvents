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

	/** Keep track of real and soft sync events and convert client soft secs to local sample numbers. */
	struct SyncEstimate
	{
		/** Sample number of a real, local, sampled, sync event. */
		uint64 syncLocalSampleNumber = 0;

		/** Timestamp of a corresponding soft, external sync event. */
		double syncSoftSecs = 0.0;

		/** Estimate of the local sample number that corresponds to soft timestamp 0.0.*/
		uint64 softSampleZero = 0;

		/** Reset and begin a new estimate. */
		void clear()
		{
			syncLocalSampleNumber = 0;
			syncSoftSecs = 0.0;
			softSampleZero = 0;
		}

		/** Convert a soft, external timestamp to the nearest local sample number. */
		int64 softSampleNumber(double softSecs, float localSampleRate)
		{
			return softSecs * localSampleRate + softSampleZero;
		}

		/** Record the latest sample number of a real sync event.
		 * Return whether or not the sync estimate is now complete.
		 */
		bool recordLocalSampleNumber(uint64 sampleNumber, float localSampleRate)
		{
			syncLocalSampleNumber = sampleNumber;
			if (syncSoftSecs)
			{
				// This local sample number completes the next sync estimate going forward.
				softSampleZero = syncLocalSampleNumber - syncSoftSecs * localSampleRate;
				return true;
			}
			return false;
		}

		/** Record the latest timestamp from a soft sync event.
		 * Return whether or not the sync estimate is now complete.
		 */
		bool recordSoftTimestamp(double softSecs, float localSampleRate)
		{
			syncSoftSecs = softSecs;
			if (syncLocalSampleNumber)
			{
				// This soft timestamp completes the next sync estimate going forward.
				softSampleZero = syncLocalSampleNumber - syncSoftSecs * localSampleRate;
				return true;
			}
			return false;
		}
	};
	SyncEstimate workingSync;
	std::list<SyncEstimate> syncEstimates;

	/** Convert a soft, external timestamp to the nearest local sample number,
	 * using the most relevant / contemporary sync estimate.
	 */
	int64 softSampleNumber(double softSecs, float localSampleRate);
};

#endif
