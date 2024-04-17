#ifndef UDPUTILS_H_DEFINED
#define UDPUTILS_H_DEFINED

/** Define several UDP socket operations to abstract us away from POSIX vs Winsock details. */

// Represent a network address, somewhat like sockaddr_in, but with generic/convenient types.
struct udpAddress
{
    char* host;
    short hostLength;
    short port;
};

// Create a socket (and start socket system)
// int serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
int openUdpSocket();

// close an open socket (and clean up socket system)
// close(serverSocket);
void closeUdpSocket(int s);

// Get a socket error code and/or description.
// errno
// strerror(errno)
// https://stackoverflow.com/questions/3400922/how-do-i-retrieve-an-error-string-from-wsagetlasterror
const char *udpErrorMessage();

// Bind an address and port
// sockaddr_in addressToBind;
// addressToBind.sin_family = AF_INET;
// addressToBind.sin_port = htons(portToBind);
// addressToBind.sin_addr.s_addr = inet_addr(hostToBind.toUTF8());
// int bindResult = bind(serverSocket, (struct sockaddr *)&addressToBind, sizeof(addressToBind));
int bindUdpSocket(int s, udpAddress addressToBind);

// Inspect a socket's bound address and port
// sockaddr_in boundAddress;
// socklen_t boundNameLength = sizeof(boundAddress);
// getsockname(serverSocket, (struct sockaddr *)&boundAddress, &boundNameLength);
// uint16_t boundPort = ntohs(boundAddress.sin_port);
// char boundHost[INET_ADDRSTRLEN];
// inet_ntop(AF_INET, &boundAddress.sin_addr, boundHost, sizeof(boundHost));
void inspectUdpSocket(int s, udpAddress *address);

// Sleep for a timeout until data arrives.
// struct pollfd toPoll[1];
// toPoll[0].fd = serverSocket;
// toPoll[0].events = POLLIN;
// int pollTimeoutMs = 100;
// ...
// int numReady = poll(toPoll, 1, pollTimeoutMs);
// if (numReady > 0 && toPoll[0].revents & POLLIN)
bool awaitUdpMessage(int s, int timeoutMs);

// Receive from an unconnected client and inspect their address.
// sockaddr_in clientAddress;
// socklen_t clientAddressLength = sizeof(clientAddress);
// size_t messageBufferSize = 65536;
// char messageBuffer[messageBufferSize] = {0};
// ...
// int bytesRead = recvfrom(serverSocket, messageBuffer, messageBufferSize - 1, 0, (struct sockaddr *)&clientAddress, &clientAddressLength);
// ...
// Inspect an unconnected client's address.
// uint16_t clientPort = ntohs(clientAddress.sin_port);
// char clientHost[INET_ADDRSTRLEN];
// inet_ntop(AF_INET, &clientAddress.sin_addr, clientHost, sizeof(clientHost));
int receiveUdpFrom(int s, udpAddress *clientAddress, char * message, int messageLength);

// Send to an unconnected client
// int bytesWritten = sendto(serverSocket, &serverSecs, 8, 0, reinterpret_cast<const sockaddr *>(&clientAddress), clientAddressLength);
int sendUdpTo(int s, udpAddress clientAddress, const char * message, int messageLength);


#endif
