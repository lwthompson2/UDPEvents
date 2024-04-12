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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#include "UDPEventsPlugin.h"

#include "UDPEventsPluginEditor.h"

UDPEventsPlugin::UDPEventsPlugin()
    : GenericProcessor("UDP Events"), Thread("UDP Events Thread")
{
    // Host address to bind for listening.
    addStringParameter(Parameter::GLOBAL_SCOPE,
                       "host",
                       "Host address to bind for receiving UDP messages.",
                       "127.0.0.1",
                       true);

    // Host port to bind for listening.
    addIntParameter(Parameter::GLOBAL_SCOPE,
                    "port",
                    "Host port to bind for receiving UDP messages.",
                    12345,
                    0,
                    65535,
                    true);

    // Id of data stream to filter.
    addIntParameter(Parameter::GLOBAL_SCOPE,
                    "stream",
                    "Which data stream to filter",
                    0,
                    0,
                    65535,
                    true);

    // Real TTL line to use for sync events.
    StringArray syncLines;
    for (int i = 1; i <= 64; i++)
    {
        syncLines.add(String(i));
    }
    addCategoricalParameter(Parameter::GLOBAL_SCOPE,
                            "line",
                            "TTL line number where real sync events will occur",
                            syncLines,
                            0,
                            false);
}

UDPEventsPlugin::~UDPEventsPlugin()
{
}

AudioProcessorEditor *UDPEventsPlugin::createEditor()
{
    editor = std::make_unique<UDPEventsPluginEditor>(this);
    return editor.get();
}

void UDPEventsPlugin::parameterValueChanged(Parameter *param)
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
        syncLine = (uint8)(int)param->getValue();
    }
}

bool UDPEventsPlugin::startAcquisition()
{
    // Start listening for UDP messages and enqueueing soft events.
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
    LOGD("UDP Events Thread is starting.");

    // Create a new UDP socket to receive on.
    int socket = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socket < 0)
    {
        LOGE("UDP Events Thread could not create a socket: ", socket, " errno: ", errno);
        return;
    }

    // Bind the local address and port so we can receive and act as a server.
    sockaddr_in addressToBind;
    addressToBind.sin_family = AF_INET;
    addressToBind.sin_port = htons(portToBind);
    addressToBind.sin_addr.s_addr = inet_addr(hostToBind.toUTF8());
    int bindResult = bind(socket, (struct sockaddr *)&addressToBind, sizeof(addressToBind));
    if (bindResult < 0)
    {
        close(socket);
        LOGE("UDP Events Thread could not bind socket to address: ", hostToBind, " port: ", portToBind, " errno: ", errno);
        return;
    }

    sockaddr_in boundAddress;
    socklen_t boundNameLength = sizeof(boundAddress);
    getsockname(socket, (struct sockaddr *)&boundAddress, &boundNameLength);
    uint16_t boundPort = ntohs(boundAddress.sin_port);
    char boundHost[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &boundAddress.sin_addr, boundHost, sizeof(boundHost));
    LOGD("UDP Events Thread is ready to receive at address: ", boundHost, " port: ", boundPort);

    // Sleep until a message arrives.
    // Wake up every 100ms to remain responsive to exit requests.
    struct pollfd toPoll[1];
    toPoll[0].fd = socket;
    toPoll[0].events = POLLIN;
    int pollTimeoutMs = 100;

    // Read the client's address and message text into local buffers.
    socklen_t clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);
    size_t messageBufferSize = 65536;
    char messageBuffer[messageBufferSize] = {0};
    while (!threadShouldExit())
    {
        // Sleep until a message arrives.
        int numReady = poll(toPoll, 1, pollTimeoutMs);
        if (numReady > 0 && toPoll[0].revents & POLLIN)
        {
            int bytesRead = recvfrom(socket, messageBuffer, messageBufferSize - 1, 0, (struct sockaddr *)&clientAddress, &clientAddressLength);
            if (bytesRead <= 0)
            {
                LOGE("UDP Events Thread had a read error.  Bytes read: ", bytesRead, " errno: ", errno);
                continue;
            }

            // Process the next message.
            uint8 messageType = (uint8)messageBuffer[0];
            if (messageType == 1)
            {
                // Got a TTL message.
                SoftEvent event;
                event.type = 1;
                event.clientSeconds = *((double *)(messageBuffer + 1));
                event.lineNumber = (uint8)messageBuffer[9];
                event.lineState = (uint8)messageBuffer[10];
                {
                    ScopedLock TTLlock(softEventQueueLock);
                    softEventQueue.push(event);
                }
            }
            else if (messageType == 2)
            {
                // Got a Text message.
                SoftEvent event;
                event.type = 2;
                event.clientSeconds = *((double *)(messageBuffer + 1));
                event.textLength = *((uint16 *)(messageBuffer + 9));
                event.text = String::fromUTF8(messageBuffer + 10, event.textLength);
                {
                    ScopedLock TTLlock(softEventQueueLock);
                    softEventQueue.push(event);
                }
            }
            else
            {
                // Got some other message, ignore it.
                LOGD("UDP Events Thread ignoring message of unknown type ", messageType, " and byte size ", bytesRead);
                continue;
            }

            // Acknowledge the message with a timestamp.
            double serverSecs = CoreServices::getSoftwareTimestamp() / CoreServices::getSoftwareSampleRate();
            int n_bytes = sendto(socket, &serverSecs, 8, 0, reinterpret_cast<sockaddr *>(&clientAddress), sizeof(clientAddressLength));
        }
    }

    close(socket);
    LOGD("UDP Events Thread is stopping.");
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
    LOGD("UDPEventsPlugin could not find ttl event channel for streamId ", streamId);
    return nullptr;
}

void UDPEventsPlugin::process(AudioBuffer<float> &buffer)
{

    // This synchronously calls back to handleTTLEvent() below,
    // Which can affect the syncState.
    checkForEvents();

    for (auto stream : dataStreams)
    {
        if (stream->getStreamId() == streamId)
        {
            // Process queued soft events into the selected stream.
            EventChannel *ttlChannel = pickTTLChannel();
            if (ttlChannel == nullptr)
            {
                // The selected stream has no events channel, nothing we can do.
                break;
            }

            {
                ScopedLock TTLlock(softEventQueueLock);
                // TODO: think about how to transition to ready in the first place!
                // TODO: think about what to do with UDP events in front of the next soft sync event.
                while (!softEventQueue.empty() && syncState.isReady())
                {
                    const SoftEvent &softEvent = softEventQueue.front();
                    softEventQueue.pop();
                    if (softEvent.type == 1)
                    {
                        // Got a soft "TTL" event.
                        if (softEvent.lineNumber == syncLine)
                        {
                            // Record a new soft event timestamp.
                            // This can complete a new sync estimate,
                            // or invalidate the previous estimate until a real ttl event arrives, maybe in the next process() block.
                            syncState.recordSoftTimestamp(softEvent.clientSeconds, stream->getSampleRate());
                        }
                        else
                        {
                            // Put this TTL event into the selected stream.
                            int64 sampleNumber = syncState.softSampleNumber(softEvent.clientSeconds, stream->getSampleRate());
                            TTLEventPtr ttlEvent = TTLEvent::createTTLEvent(ttlChannel,
                                                                            sampleNumber,
                                                                            softEvent.lineNumber,
                                                                            softEvent.lineState);
                            addEvent(ttlEvent, 0);
                        }
                    }
                    else if (softEvent.type == 2)
                    {
                        // Got a soft Text event.
                        // Best effort converting sample number here, even though Ephys currently ignores it for text!
                        int64 sampleNumber = syncState.softSampleNumber(softEvent.clientSeconds, stream->getSampleRate());
                        TextEventPtr textEvent = TextEvent::createTextEvent(getMessageChannel(),
                                                                            sampleNumber,
                                                                            softEvent.text);
                        addEvent(textEvent, 0);
                    }
                }
            }
        }
    }
}

void UDPEventsPlugin::handleTTLEvent(TTLEventPtr event)
{
    if (event->getLine() == syncLine)
    {
        for (auto stream : dataStreams)
        {
            if (stream->getStreamId() == streamId)
            {
                // Record a new sync event sample number.
                // This can complete a new sync estimate,
                // or invalidate the previous estimate until a soft timestmap arrives.
                syncState.recordLocalSampleNumber(event->getSampleNumber(), stream->getSampleRate());
            }
        }
    }
}
