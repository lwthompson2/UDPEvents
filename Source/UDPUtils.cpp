/** Define several UDP socket operations with POSIX and Winsock implementations. */

// some winsock porting advice
// https://learn.microsoft.com/en-us/windows/win32/winsock/porting-socket-applications-to-winsock

#include "UDPUtils.h"

#ifdef WIN32

/** Implement our utils using Windock calls. */
#include <winsock2.h>

#else

/** Implement our utils using Posix socket calls. */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>

#endif
