#include <Rr/Rr_Platform.h>

#if defined(_WIN32)
#include "Rr_Win32.h"
#elif defined(__linux__)
#include "Rr_Linux.h"
#endif
