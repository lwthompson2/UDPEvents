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
#include "UDPUtils.h"

UDPEventsPlugin::UDPEventsPlugin()
    : GenericProcessor("UDP Events"), Thread("UDP Events Thread")
{ 
}

UDPEventsPlugin::~UDPEventsPlugin()
{
}

void UDPEventsPlugin::registerParameters()
{
    // Register parameters here, if any

    // Host port to bind for receiving as a server.
    addIntParameter(Parameter::PROCESSOR_SCOPE, "port",
        "Port",
        "Host port to bind for receiving UDP messages.",
        12345,
        0,
        65535,
        true);

    // Host address to bind for receiving as a server.
    addStringParameter(Parameter::PROCESSOR_SCOPE, "host",
        "Host",
        "Host address to bind for receiving UDP messages.",
        "127.0.0.1",
        true);

    // Id of data stream to filter.
    addIntParameter(Parameter::PROCESSOR_SCOPE, "stream",
        "Stream",
        "Which data stream to filter",
        0,
        0,
        65535,
        true);

    // Real TTL line to use for sync events.
    Array<String> syncLines;
    for (int i = 1; i <= 256; i++)
    {
        syncLines.add(String(i));
    }
    addCategoricalParameter(Parameter::PROCESSOR_SCOPE,
        "line",
        "Line",
        "TTL line number where real sync events will occur",
        syncLines,
        0,
        false);

    // Real TTL line state to use for sync events.
    Array<String> syncStates;
    syncStates.add("both");
    syncStates.add("high");
    syncStates.add("low");
    addCategoricalParameter(Parameter::PROCESSOR_SCOPE,
        "state",
        "State",
        "TTL line state for real sync events",
        syncStates,
        0,
        false);
}

AudioProcessorEditor* UDPEventsPlugin::createEditor()
{
    editor = std::make_unique<UDPEventsPluginEditor>(this);
    return editor.get();
}

void UDPEventsPlugin::parameterValueChanged(Parameter* param)
{
    if (param->getName().equalsIgnoreCase("host"))
    {
        hostToBind = param->getValueAsString();
    }
    else if (param->getName().equalsIgnoreCase("port"))
    {
        portToBind = (uint16)(int)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("stream"))
    {
        streamId = (uint16)(int)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("line"))
    {
        // The UI presents 1-based line numbers 1-256 but internal code uses 0-based 0-255.
        // Looks like getValue() for a categorical parameter gives the selection index.
        // Because of how we set up the categories above, selection index works as the 0-based line number.
        syncLine = (uint8)(int)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("state"))
    {
        syncStateIndex = (uint8)(int)param->getValue();
    }
}

bool UDPEventsPlugin::startAcquisition()
{
    /** Start with fresh sync estimates each acquisition.*/
    workingSync.clear();
    syncEstimates.clear();

    /** UDP socket and buffer lifecycle will match GUI acquisition periods. */
    startThread();
    return isThreadRunning();
}

bool UDPEventsPlugin::stopAcquisition()
{
    if (!stopThread(1000))
    {
        LOGE("UDP Events Thread timed out when trying ot stop.  Forcing termination, so things might be unstable going forward.");
        return false;
    }
    return true;
}

void UDPEventsPlugin::run()
{
    LOGC("UDP Events Thread is starting.");

    // Create a new UDP socket to receive on.
    int serverSocket = udpOpenSocket();
    if (serverSocket < 0)
    {
        LOGE("UDP Events Thread error creating socket: ", udpErrorMessage());
        return;
    }

    // Bind the local address and port so we can receive, as a server.
    struct UdpAddress addressToBind;
    addressToBind.port = portToBind;
    hostToBind.copyToUTF8(addressToBind.hostName, sizeof(addressToBind.hostName));
    udpHostNameToBin(&addressToBind);
    int bindResult = udpBind(serverSocket, &addressToBind);
    if (bindResult < 0)
    {
        udpCloseSocket(serverSocket);
        LOGE("UDP Events Thread could not bind socket to address: ", hostToBind, " port: ", portToBind, " error: ", udpErrorMessage());
        return;
    }

    // Report the address and port we actually bound (they might have been assigned by system).
    struct UdpAddress boundAddress;
    udpGetAddress(serverSocket, &boundAddress);
    udpHostBinToName(&boundAddress);
    LOGC("UDP Events Thread is ready to receive at address: ", boundAddress.hostName, " port: ", boundAddress.port);

    // Read the client addresses and messages text into local buffers.
    struct UdpAddress clientAddress;
    char messageBuffer[65536] = {0};
    while (!threadShouldExit())
    {
        // Wait for a message to arrive, but wake every 100ms to remain responsive to exit requests.
        bool messageArrived = udpAwaitMessage(serverSocket, 100);
        if (messageArrived)
        {
            int bytesRead = udpReceiveFrom(serverSocket, &clientAddress, messageBuffer, sizeof(messageBuffer));
            if (bytesRead <= 0)
            {
                LOGE("UDP Events Thread had a read error.  Bytes read: ", bytesRead, " error: ", udpErrorMessage());
                continue;
            }

            // Record a timestamp close to when we got the UDP message.
            const int64 serverSecs = (double)CoreServices::getSystemTime(); 

            // Who sent us this message?
            udpHostBinToName(&clientAddress);
            LOGC("UDP Events Thread received ", bytesRead, " bytes from host: ", clientAddress.hostName, " port: ", clientAddress.port);

            // Acknowledge message receipt to the client.
            int bytesWritten = udpSendTo(serverSocket, &clientAddress, (const char *)&serverSecs, 8);
            if (bytesWritten < 0)
            {
                LOGE("UDP Events Thread had a write error.  Bytes written: ", bytesWritten, " error: ", udpErrorMessage());
                continue;
            }
            LOGC("UDP Events Thread sent ", bytesWritten, " bytes to host: ", clientAddress.hostName, " port: ", clientAddress.port);

            // Process the message itself.
            uint8 messageType = (uint8)messageBuffer[0];
            if (messageType == 1)
            {
                // This is a TTL message.
                SoftEvent ttlEvent;
                ttlEvent.type = 1;
                ttlEvent.clientSeconds = *((double *)(messageBuffer + 1));
                ttlEvent.systemTimeMilliseconds = serverSecs;
                ttlEvent.lineNumber = (uint8)messageBuffer[9];
                ttlEvent.lineState = (uint8)messageBuffer[10];

                LOGC("UDP Events Thread got a TTL message with client timestamp: ", ttlEvent.clientSeconds, " 0-based line number: ", (int)ttlEvent.lineNumber, " line state: ", (int)ttlEvent.lineState);

                // Enqueue this to be handled below, on the main thread, in process().
                {
                    ScopedLock TTLlock(softEventQueueLock);
                    softEventQueue.push(ttlEvent);
                }
            }
            else if (messageType == 2)
            {
                // This is a Text message.
                SoftEvent textEvent;
                textEvent.type = 2;
                textEvent.clientSeconds = *((double *)(messageBuffer + 1));
                textEvent.systemTimeMilliseconds = serverSecs;
                textEvent.textLength = udpNToHS(*((uint16 *)(messageBuffer + 9)));
                textEvent.text = String::fromUTF8(messageBuffer + 11, textEvent.textLength);

                LOGC("UDP Events Thread got a Text message with client timestamp: ", textEvent.clientSeconds, " message length: ", (int)textEvent.textLength, " message: ", textEvent.text);

                // Enqueue this to be handled below, on the main thread, in process().
                {
                    ScopedLock TTLlock(softEventQueueLock);
                    softEventQueue.push(textEvent);
                }
            }
            else
            {
                // This seems to be some unexpected message, and we'll ignore it.
                LOGE("UDP Events Thread ignoring message of unknown type ", (int)messageType, " and byte size ", bytesRead);
            }
        }
    }

    // The main loop has exited so we're done, so clean up and let the UDP thread terminate.
    udpCloseSocket(serverSocket);
    LOGC("UDP Events Thread is stopping.");
}

EventChannel *UDPEventsPlugin::pickTTLChannel()
{
    for (auto eventChannel : eventChannels)
    {
        if (eventChannel->getType() == EventChannel::Type::TTL && eventChannel->getStreamId() == streamId)
        {
            return eventChannel;
        }
    }
    return nullptr;
}

void UDPEventsPlugin::process(AudioBuffer<float> &buffer)
{
    // This synchronously calls back to handleTTLEvent(), below.
    checkForEvents();

    // Find the selected data stream.
    for (auto stream : dataStreams)
    {
        if (stream->getStreamId() == streamId)
        {
            // Find a TTL channel for the selected data stream.
            EventChannel *ttlChannel = pickTTLChannel();
            if (ttlChannel == nullptr)
            {
                return;
            }

            // Work through soft messages enqueued above, by run() on the UDP Thread.
            {
                ScopedLock TTLlock(softEventQueueLock);
                while (!softEventQueue.empty())
                {
                    const SoftEvent &softEvent = softEventQueue.front();
                    if (softEvent.type == 1)
                    {
                        // This is a TTL message.
                        if (filterSyncEvent(softEvent.lineNumber, (bool)softEvent.lineState))
                        {
                            LOGC("UDP Events recording soft TTL sync info on 0-based line: ", (int)softEvent.lineNumber, " state: ", (bool)softEvent.lineState, " client soft secs ", softEvent.clientSeconds);

                            // This is a soft sync event corresponding to a real TTL event.
                            bool syncComplete = workingSync.recordSoftTimestamp(softEvent.clientSeconds, stream->getSampleRate());
                            if (syncComplete)
                            {
                                // The working sync has seen both a real and a soft event.
                                // Record it as an event, add it to the sync history, and start a new sync going forward.
                                addEventForSyncEstimate(workingSync);
                                syncEstimates.push_back(workingSync);
                                workingSync.clear();
                            }
                        }
                        else
                        {
                            // This is a soft TTL event to add to the selected stream.
                            // We'll add it, if we can find a previous sync estimate.
                            int64 sampleNumber = softSampleNumber(softEvent.clientSeconds, stream->getSampleRate());
                            if (sampleNumber)
                            {
                                TTLEventPtr ttlEvent = TTLEvent::createTTLEvent(ttlChannel,
                                                                                softEvent.systemTimeMilliseconds,
                                                                                softEvent.lineNumber,
                                                                                softEvent.lineState);
                                addEvent(ttlEvent, 0);
                            }
                        }
                    }
                    else if (softEvent.type == 2)
                    {
                        // This is a Text message to add to the selected stream.
                        // We'll add it, if we can find a previous sync estimate.
                        int64 sampleNumber = softSampleNumber(softEvent.clientSeconds, stream->getSampleRate());
                        if (sampleNumber)
                        {
                            // Currently Open Ephys persists text events with low, per-block timing precision.
                            // Append high-precision timing info to the message for later reconstruction.
                            String messageText = softEvent.text + "@" + String(softEvent.clientSeconds, 8, false) + "=" + String(sampleNumber);
                            TextEventPtr textEvent = TextEvent::createTextEvent(getMessageChannel(),
                                                                                softEvent.systemTimeMilliseconds,
                                                                                messageText);
                            LOGC("Regular text event| type: ", typeid(softEvent.systemTimeMilliseconds).name(), " value: ", softEvent.systemTimeMilliseconds);
                            addEvent(textEvent, 0);
                        }
                    }

                    // Pop invokes destructor of message (and allocated text!) -- so wait until we're done.
                    softEventQueue.pop();
                }
            }
        }
    }
}

int64 UDPEventsPlugin::softSampleNumber(double softSecs, float localSampleRate)
{
    // Look for the last completed sync estimate preceeding the given softSecs.
    for (auto current = syncEstimates.rbegin(); current != syncEstimates.rend(); ++current)
    {
        LOGC("UDP Events has a sync estimate with client soft secs: ", current->syncSoftSecs);

        if (current->syncSoftSecs <= softSecs)
        {
            // This is the most relevant sync estimate.
            return current->softSampleNumber(softSecs, localSampleRate);
        }
    }

    // No relevant sync estimates.
    LOGE("UDP Events has no good sync estimate preceeding client soft secs: ", softSecs);
    return 0;
}

bool UDPEventsPlugin::filterSyncEvent(uint8 line, bool state)
{
    switch (syncStateIndex)
    {
    case 1:
        // syncStateIndex 1 means use only high state.
        return line == syncLine && state;
    case 2:
        // syncStateIndex 2 means use only low state.
        return line == syncLine && !state;
    default:
        // syncStateIndex 0 (or other) means use either state.
        return line == syncLine;
    }
}

void UDPEventsPlugin::addEventForSyncEstimate(struct SyncEstimate syncEstimate)
{
    LOGC("UDP Events adding sync estimate with client soft secs: ", syncEstimate.syncSoftSecs, " local timestamp: ", syncEstimate.syncLocalTimestamp);
    LOGC("Sync text event| type: ", typeid(syncEstimate.syncLocalTimestamp).name(), " value: ", syncEstimate.syncLocalTimestamp);
    String text = "UDP Events sync on line " + String(syncLine + 1) + "@" + String(syncEstimate.syncSoftSecs, 8, false) + "=" + String(syncEstimate.syncLocalSampleNumber);
    TextEventPtr textEvent = TextEvent::createTextEvent(getMessageChannel(),
                                                        (int64)syncEstimate.syncLocalTimestamp,
                                                        text);
    addEvent(textEvent, 0);
}

void UDPEventsPlugin::handleTTLEvent(TTLEventPtr event)
{
    if (filterSyncEvent(event->getLine(), event->getState()))
    {
        LOGC("UDP Events saw a real TTL event on 0-based line: ", (int)event->getLine(), " state: ", event->getState());

        // This real TTL event should corredspond to a soft TTL event.
        for (auto stream : dataStreams)
        {
            if (stream->getStreamId() == streamId)
            {
                // Using event->getTimestampInSeconds() is not working, try calculating manually?
                const int64 localMilliSecs = (double)((event->getSampleNumber() / stream->getSampleRate()) * 1000.0);
                LOGC("UDP Events recording real TTL sync info on 0-based line: ", (int)event->getLine(), " state: ", event->getState(), " local timestamp: ", (int64)localMilliSecs);
                workingSync.recordLocalSampleNumber(event->getSampleNumber(), stream->getSampleRate());
                bool completed = workingSync.recordLocalTimestamp(localMilliSecs, stream->getSampleRate());
                if (completed)
                {
                    // The working sync has seen both a real and a soft event.
                    // Record it as an event, add it to the sync history, and start a new sync going forward.
                    addEventForSyncEstimate(workingSync);
                    syncEstimates.push_back(workingSync);
                    workingSync.clear();
                }
            }
        }
    }
}
