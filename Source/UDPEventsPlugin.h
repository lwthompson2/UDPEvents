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

	/** Called every time the settings of an upstream plugin are changed.
		Allows the processor to handle variations in the channel configuration or any other parameter
		passed through signal chain. The processor can use this function to modify channel objects that
		will be passed to downstream plugins. */
	void updateSettings() override;

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

	/** Get the event channel we created for the given selected streamId. */
	EventChannel *getTTLChannel();

	/** Estimate of which stream sample number corresponds to zero clientSeconds. */
	int64 clientSampleZero = 0;

	/** Editable settings.*/
	uint16 streamId = 0;
	int syncLine = 0;

	String hostToBind = "127.0.0.1";
    uint16_t portToBind = 12345;
};

#endif
