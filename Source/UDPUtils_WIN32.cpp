/** Implement our UDPUtils for Windows / Winsock systems. */

// https://web.archive.org/web/20140625123925/http://nadeausoftware.com/articles/2012/01/c_c_tip_how_use_compiler_predefined_macros_detect_operating_system#WindowsCygwinnonPOSIXandMinGW
#if defined(_WIN32)

#include <winsock2.h>

#include "UDPUtils.h"

// TODO

// some winsock porting advice
// https://stackoverflow.com/questions/3400922/how-do-i-retrieve-an-error-string-from-wsagetlasterror
// https://learn.microsoft.com/en-us/windows/win32/winsock/porting-socket-applications-to-winsock

#endif
