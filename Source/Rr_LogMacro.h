/*
 * Copyright (C) 2024-2026 Alexandr Semenov <tidesmain@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include <Rr/Rr_Log.h>

#ifdef RR_LOG_MACRO_CATEGORY
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdlib.h>
#define RR_LOG_ABORT(...)                                \
    {                                                    \
        Rr_LogError(RR_LOG_MACRO_CATEGORY, __VA_ARGS__); \
        abort();                                         \
    }
#define RR_LOG_ERROR(...)   Rr_LogError(RR_LOG_MACRO_CATEGORY, __VA_ARGS__)
#define RR_LOG_WARNING(...) Rr_LogWarning(RR_LOG_MACRO_CATEGORY, __VA_ARGS__)
#define RR_LOG_INFO(...)    Rr_LogInfo(RR_LOG_MACRO_CATEGORY, __VA_ARGS__)
#define RR_LOG_TRACE(...)   Rr_LogTrace(RR_LOG_MACRO_CATEGORY, __VA_ARGS__)
#define RR_LOG(...)         Rr_LogTrace(RR_LOG_MACRO_CATEGORY, __VA_ARGS__)
#endif
