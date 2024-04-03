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

#include "UDPEventsPlugin.h"

#include "UDPEventsPluginEditor.h"

UDPEventsPlugin::UDPEventsPlugin()
    : GenericProcessor("UDP Events")
{
    // Event frequency
    addFloatParameter(Parameter::GLOBAL_SCOPE,
                      "interval",
                      "Interval (in ms) for automated event generation (0 ms = off)",
                      1000.0f,
                      0.0f,
                      5000.0f,
                      50.0f);

    // Array of selectable TTL lines
    StringArray outputs;
    for (int i = 1; i <= 8; i++)
        outputs.add(String(i));

    // Event output line
    addCategoricalParameter(Parameter::GLOBAL_SCOPE,
                            "ttl_line",
                            "Event output line",
                            outputs,
                            0);

    addStringParameter(Parameter::GLOBAL_SCOPE,
                       "manual_trigger",
                       "Used to notify processor of manually triggered TTL events",
                       String());
}

void UDPEventsPlugin::parameterValueChanged(Parameter *param)
{
    if (param->getName().equalsIgnoreCase("manual_trigger"))
    {
        shouldTriggerEvent = true;
        LOGD("Event was manually triggered");
    }
    else if (param->getName().equalsIgnoreCase("interval"))
    {
        eventIntervalMs = (float)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("ttl_line"))
    {
        outputLine = (int)param->getValue();
    }
}

UDPEventsPlugin::~UDPEventsPlugin()
{
}

AudioProcessorEditor *UDPEventsPlugin::createEditor()
{
    editor = std::make_unique<UDPEventsPluginEditor>(this);
    return editor.get();
}

void UDPEventsPlugin::updateSettings()
{
    // Create and add a TTL channel to the first data stream.
    EventChannel::Settings settings{
        EventChannel::Type::TTL,
        "UDP Events",
        "UDP events added to the stream",
        "udp.events",
        dataStreams[0]};

    udpEventChannel = new EventChannel(settings);
    eventChannels.add(udpEventChannel);
    udpEventChannel->addProcessor(processorInfo.get());
}

void UDPEventsPlugin::process(AudioBuffer<float> &buffer)
{

    // loop through the streams
    for (auto stream : getDataStreams())
    {
        // Only generate on/off event for the first data stream
        if (stream == getDataStreams()[0])
        {
            int totalSamples = getNumSamplesInBlock(stream->getStreamId());
            uint64 startSampleForBlock = getFirstSampleNumberForBlock(stream->getStreamId());

            int eventIntervalInSamples;
            if (eventIntervalMs > 0)
            {
                eventIntervalInSamples = (int)stream->getSampleRate() * eventIntervalMs / 2 / 1000;
            }
            else
            {
                eventIntervalInSamples = (int)stream->getSampleRate() * 100 / 2 / 1000;
            }

            if (shouldTriggerEvent)
            {
                // add an ON event at the first sample.
                TTLEventPtr eventPtr = TTLEvent::createTTLEvent(udpEventChannel,
                                                                startSampleForBlock,
                                                                outputLine,
                                                                true);
                addEvent(eventPtr, 0);

                shouldTriggerEvent = false;
                eventWasTriggered = true;
                triggeredEventCounter = 0;
            }

            for (int i = 0; i < totalSamples; i++)
            {
                counter++;

                if (eventWasTriggered)
                {
                    triggeredEventCounter++;
                }

                if (triggeredEventCounter == eventIntervalInSamples)
                {
                    // add off event at the correct offset
                    TTLEventPtr eventPtr = TTLEvent::createTTLEvent(udpEventChannel,
                                                                    startSampleForBlock + i,
                                                                    outputLine,
                                                                    false);
                    addEvent(eventPtr, i);

                    eventWasTriggered = false;
                    triggeredEventCounter = 0;
                }

                if (counter == eventIntervalInSamples && eventIntervalMs > 0)
                {
                    // add on or off event at the correct offset
                    state = !state;
                    TTLEventPtr eventPtr = TTLEvent::createTTLEvent(udpEventChannel,
                                                                    startSampleForBlock + i,
                                                                    outputLine, state);
                    addEvent(eventPtr, i);

                    counter = 0;
                }

                if (counter > eventIntervalInSamples)
                {
                    counter = 0;
                }
            }
        }
    }
}

void UDPEventsPlugin::handleTTLEvent(TTLEventPtr event)
{
}

void UDPEventsPlugin::handleSpike(SpikePtr spike)
{
}

void UDPEventsPlugin::handleBroadcastMessage(String message)
{
}

void UDPEventsPlugin::saveCustomParametersToXml(XmlElement *parentElement)
{
}

void UDPEventsPlugin::loadCustomParametersFromXml(XmlElement *parentElement)
{
}

bool UDPEventsPlugin::startAcquisition()
{
    counter = 0;
    state = false;

    return true;
}
