/** Implement our UDPUtils for POSIX systems like Linux and macOS. */

#ifndef WIN32

// Assume we are on a POSIX-compliant system.

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <errno.h>
#include <string.h>

#include "UDPUtils.h"

int udpOpenSocket()
{
    return socket(AF_INET, SOCK_DGRAM, 0);
}

void udpCloseSocket(int s)
{
    close(s);
}

const char *udpErrorMessage()
{
    return strerror(errno);
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
    struct pollfd toPoll[1];
    toPoll[0].fd = s;
    toPoll[0].events = POLLIN;
    int numReady = poll(toPoll, 1, timeoutMs);
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
