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
    // Id of data stream to filter.
    addIntParameter(Parameter::GLOBAL_SCOPE,
                    "data_stream",
                    "Which data stream to filter",
                    0,
                    0,
                    65535);

    // Real TTL line to use for sync events.
    StringArray syncLines;
    for (int i = 1; i <= 64; i++)
    {
        syncLines.add(String(i));
    }
    addCategoricalParameter(Parameter::GLOBAL_SCOPE,
                            "sync_line",
                            "Real TTL sync line",
                            syncLines,
                            0);

    // Start listening for UDP messages and enqueueing soft events.
    // TODO: should this be tied to start/stop acquire rather than construct / destruct?
    startThread();
}

UDPEventsPlugin::~UDPEventsPlugin()
{
    // TODO: should this be tied to start/stop acquire rather than construct / destruct?
    if (!stopThread(1000))
    {
        // The thread should not take long to exit, so somethign must be wrong!
        jassertfalse;
        LOGE("UDP Events Thread stop timeout, forcing termination -- things might be unstable going forward.");
    }
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

    LOGD("UDP Events Thread is ready at address: ", hostToBind, " port: ", portToBind);

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

AudioProcessorEditor *UDPEventsPlugin::createEditor()
{
    editor = std::make_unique<UDPEventsPluginEditor>(this);
    return editor.get();
}

void UDPEventsPlugin::parameterValueChanged(Parameter *param)
{
    if (param->getName().equalsIgnoreCase("data_stream"))
    {
        streamId = (uint16)(int)param->getValue();
    }
    else if (param->getName().equalsIgnoreCase("sync_line"))
    {
        syncLine = (int)param->getValue();
    }
}

void UDPEventsPlugin::updateSettings()
{
    // Add a TTL event channel to each data stream.
    // In process() handleTTLEvent() and below, we'll pick one stream using the users's selected streamId.
    // We don't have access to this selection at the time when updateSettings() is called.
    for (auto stream : dataStreams)
    {
        EventChannel::Settings ttlChannelSettings{
            EventChannel::Type::TTL,
            "UDP TTL Events",
            "Soft TTL events added via UDP",
            "udp.ttl.events",
            stream};
        EventChannel *ttlChannel = new EventChannel(ttlChannelSettings);
        eventChannels.add(ttlChannel);
        ttlChannel->addProcessor(processorInfo.get());
    }
}

EventChannel *UDPEventsPlugin::getTTLChannel()
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

    // TODO: I think this synchronously calls back to handleTTLEvent() below.
    // How does that fact inform how we form real-soft sync event twins?
    // When/where to wait for the other twin?
    // What if they arrive in different "process" blocks?
    checkForEvents();

    for (auto stream : dataStreams)
    {
        if (stream->getStreamId() == streamId)
        {
            // Process queued soft events into the selected stream.
            EventChannel *ttlChannel = getTTLChannel();
            {
                ScopedLock TTLlock(softEventQueueLock);
                while (!softEventQueue.empty())
                {
                    const SoftEvent &softEvent = softEventQueue.front();
                    softEventQueue.pop();
                    if (softEvent.type == 1)
                    {
                        // Got a soft "TTL" event.
                        if (softEvent.lineNumber == syncLine)
                        {
                            // Stop consuming soft events here,
                            // until pairing this with a real TTL event in handleTTLEvent().
                            break;
                        }
                        else
                        {
                            // Put this TTL event into the selected stream.
                            int64 softSampleNumber = softEvent.clientSeconds / stream->getSampleRate() + clientSampleZero;
                            TTLEventPtr ttlEvent = TTLEvent::createTTLEvent(ttlChannel,
                                                                            softSampleNumber,
                                                                            softEvent.lineNumber,
                                                                            softEvent.lineState);
                            addEvent(ttlEvent, 0);
                        }
                    }
                    else if (softEvent.type == 2)
                    {
                        // Got a soft Text event.
                        // Best effort converting sample number here, even though Ephys currently ignores it for text!
                        int64 softSampleNumber = softEvent.clientSeconds / stream->getSampleRate() + clientSampleZero;
                        TextEventPtr textEvent = TextEvent::createTextEvent(getMessageChannel(),
                                                                            softSampleNumber,
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
    // TODO: pair up a real TTL event on the syncLine with its soft event twin.
}
