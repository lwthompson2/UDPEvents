#ifndef UDPUTILS_H_DEFINED
#define UDPUTILS_H_DEFINED

/** Define several UDP socket operations to abstract us away from POSIX vs Winsock details. */

/** Network IP v4 address and port that callers can allocate on the stack. */
struct UdpAddress
{
    // String representation of the host name.
    char hostName[16];

    // Binary representation of the host name (required for send and receive).
    unsigned long host;

    // Port number using local host's byte ordering.
    unsigned short port;
};

/** Create a socket with an integer handle.  Start (increment) the socket system as needed. */
int udpOpenSocket();

/** Close the given socket.  Clean up (decrement) the socket system as needed. */
void udpCloseSocket(int s);

/** Get a short description for the most recent socket error. */
const char *udpErrorMessage();

/** Bind the given socket to the given address.  Return negative on error. */
int udpBind(int s, const struct UdpAddress *const address);

/** Get the address a socket was bound to (which could have been assigned by the system). */
void udpGetAddress(int s, struct UdpAddress *const address);

/** Convert an addres host's binary representation to a string name. */
void udpHostBinToName(struct UdpAddress *const address);

/** Convert an address host's string name to binary representation. */
void udpHostNameToBin(struct UdpAddress *const address);

/** Sleep until a message arrives, up to the given timeout ms.  Return true if a message did arrive. */
bool udpAwaitMessage(int s, int timeoutMs);

/** Read one message from an unconnected client.  Fill in the given address for the client and return the number of bytes read. */
int udpReceiveFrom(int s, struct UdpAddress *const address, char *message, int messageLength);

/** Send a message to the given unconnected client's address, return the number of bytes written. */
int udpSendTo(int s, const struct UdpAddress *const address, const char *message, int messageLength);

/** Convert a 16-bit unsigned integer from netowrk to host byte order. */
short unsigned int udpNToHS(short unsigned int netInt);

#endif
