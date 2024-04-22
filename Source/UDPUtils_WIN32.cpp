/** Implement our UDPUtils for Windows / Winsock systems.
 *
 * Developing this on a Linux machine, it was handy to get Windows headers for auto-completion etc.
 * I got the headers from the wine project with: sudo apt install libwine-dev
 * This added headers in: /usr/include/wine/wine/windows/winsock2.h
 * I added these to the plugin cmake build (temporarily!) with: include_directories("/usr/include/wine/wine/windows")
 * After reconfiguring the project, VS code started auto-completing things from winsock2.h, etc.
 *
 */

// https://web.archive.org/web/20140625123925/http://nadeausoftware.com/articles/2012/01/c_c_tip_how_use_compiler_predefined_macros_detect_operating_system#WindowsCygwinnonPOSIXandMinGW
#if defined(_WIN32)

#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "UDPUtils.h"

int udpOpenSocket()
{
    // This increments a count each time, balanced by udpCloseSocket().
    // The first one does the actual startup.
    WSADATA ws;
    int startupResult = WSAStartup(MAKEWORD(2, 2), &ws);
    if (startupResult != 0)
    {
        return -1;
    }

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET)
    {
        return -2;
    }

    return s;
}

void udpCloseSocket(int s)
{
    closesocket(s);

    // This decrements a count each time, balancing out udpOpenSocket().
    // The last one does actual cleanup.
    WSACleanup();
}

char lastErrorMessage[256];
const char *udpErrorMessage()
{
    int errorCode = WSAGetLastError();

    // Start with the null-terminated empty string.
    lastErrorMessage[0] = '\0';

    // Ask the system to describe this error code.
    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        lastErrorMessage,
        sizeof(lastErrorMessage),
        NULL);

    // In case the system didn't fill in the message, fill in a placeholder.
    if (!lastErrorMessage[0])
    {
        sprintf(lastErrorMessage, "WSAGetLastError: %d", errorCode);
    }

    return lastErrorMessage;
}

int udpBind(int s, const struct UdpAddress *const address)
{
    struct sockaddr_in addressToBind;
    addressToBind.sin_family = AF_INET;
    addressToBind.sin_port = htons(address->port);
    addressToBind.sin_addr.s_addr = address->host;
    return bind(s, (struct sockaddr *)&addressToBind, sizeof(addressToBind));
}

void udpGetAddress(int s, struct UdpAddress *const address)
{
    struct sockaddr_in boundAddress;
    boundAddress.sin_family = AF_INET;
    socklen_t boundNameLength = sizeof(boundAddress);
    getsockname(s, (struct sockaddr *)&boundAddress, &boundNameLength);
    address->host = boundAddress.sin_addr.s_addr;
    address->port = ntohs(boundAddress.sin_port);
}

void udpHostBinToName(struct UdpAddress *const address)
{
    inet_ntop(AF_INET, &address->host, address->hostName, sizeof(address->hostName));
}

void udpHostNameToBin(struct UdpAddress *const address)
{
    inet_pton(AF_INET, address->hostName, &address->host);
}

bool udpAwaitMessage(int s, int timeoutMs)
{
    WSAPOLLFD toPoll[1];
    toPoll[0].fd = s;
    toPoll[0].events = POLLIN;
    int numReady = WSAPoll(toPoll, 1, timeoutMs);
    return numReady > 0 && toPoll[0].revents & POLLIN;
}

int udpReceiveFrom(int s, struct UdpAddress *const address, char *message, int messageLength)
{
    struct sockaddr_in clientAddress;
    clientAddress.sin_family = AF_INET;
    socklen_t clientAddressLength = sizeof(clientAddress);
    int bytesRead = recvfrom(s, message, messageLength, 0, (struct sockaddr *)&clientAddress, &clientAddressLength);
    if (bytesRead >= 0)
    {
        address->host = clientAddress.sin_addr.s_addr;
        address->port = ntohs(clientAddress.sin_port);
    }
    return bytesRead;
}

int udpSendTo(int s, const struct UdpAddress *const address, const char *message, int messageLength)
{
    struct sockaddr_in clientAddress;
    clientAddress.sin_family = AF_INET;
    clientAddress.sin_addr.s_addr = address->host;
    clientAddress.sin_port = htons(address->port);
    socklen_t clientAddressLength = sizeof(clientAddress);
    return sendto(s, message, messageLength, 0, (const struct sockaddr *)&clientAddress, clientAddressLength);
}

short unsigned int udpNToHS(short unsigned int netInt)
{
    return ntohs(netInt);
}

#endif
