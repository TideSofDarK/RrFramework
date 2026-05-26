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

#include "Rr_Platform.h"

#include <Rr/Rr_App.h>

void Rr_SetMouseButtonEvent(
    bool Down,
    Rr_Vec2 Position,
    Rr_MouseButton Button,
    Rr_Event *Event)
{
    Event->Type =
        Down ? RR_EVENT_TYPE_MOUSE_BUTTON_DOWN : RR_EVENT_TYPE_MOUSE_BUTTON_UP;
    Event->MouseButton.Position = Position;
    Event->MouseButton.Button = (uint8_t)Button;

    static uint64_t LastClickTime[RR_MOUSE_BUTTON_COUNT] = { 0 };
    static uint8_t Clicks[RR_MOUSE_BUTTON_COUNT] = { 0 };
    if (Down)
    {
        uint64_t Now = Rr_GetTimeMS();
        uint64_t Diff = Now - LastClickTime[Event->MouseButton.Button];
        if (Diff < RR_DOUBLE_CLICK_TIME_MS)
        {
            Clicks[Event->MouseButton.Button]++;
        }
        else
        {
            Clicks[Event->MouseButton.Button] = 0;
        }
        Event->MouseButton.Clicks = Clicks[Event->MouseButton.Button] + 1;
        LastClickTime[Event->MouseButton.Button] = Now;
    }
    else
    {
        Event->MouseButton.Clicks = 1;
    }
}
