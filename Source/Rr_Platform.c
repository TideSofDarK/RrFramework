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

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_PLATFORM
#include "Rr_App.h"
#include "Rr_LogMacro.h"

#include <Rr/Rr_Utility.h>

#include <ctype.h>

Rr_Platform gPlatform = { 0 };

void Rr_ReleaseAllInput(void)
{
    gPlatform.MouseState = 0;
    gPlatform.RelativeMouseMode = false;

    for (Rr_Scancode Scancode = RR_SCANCODE_UNKNOWN;
         Scancode < RR_SCANCODE_COUNT;
         ++Scancode)
    {
        if (gPlatform.PressedKeys[Scancode])
        {
            Rr_AddKeyEvent(Scancode, false);
        }
    }

    gPlatform.Keymod = 0;
}

bool Rr_IsScancodePressed(Rr_Scancode Scancode)
{
    return gPlatform.PressedKeys[Scancode];
}

Rr_Vec2 Rr_GetMousePosition(void)
{
    return gPlatform.MousePosition;
}

Rr_Vec2 Rr_GetMousePositionDelta(void)
{
    return gPlatform.MousePositionDelta;
}

Rr_MouseButtonFlags Rr_GetMouseState(void)
{
    return gPlatform.MouseState;
}

static inline Rr_Event *Rr_AddEvent(void)
{
    Rr_ThreadContext *ThreadContext = Rr_GetThreadContext();

    Rr_EventHiveIterator It =
        Rr_PushEventIntoHive(&gPlatform.EventHive, ThreadContext->Arena);
    return It.Element;
}

void Rr_AddQuitRequestedEvent(void)
{
    Rr_Event *Event = Rr_AddEvent();
    Event->Type = RR_EVENT_TYPE_QUIT_REQUESTED;
}

void Rr_AddSwapchainCreatedEvent(void)
{
    Rr_Event *Event = Rr_AddEvent();
    Event->Type = RR_EVENT_TYPE_SWAPCHAIN_CREATED;
}

static inline void Rr_UpdateKeymodState(Rr_KeymodFlagsBits Bit, bool On)
{
    if (On)
    {
        gPlatform.Keymod |= (Rr_KeymodFlags)Bit;
    }
    else
    {
        gPlatform.Keymod &= (Rr_KeymodFlags)~Bit;
    }
}

void Rr_AddKeyEvent(Rr_Scancode Scancode, bool Down)
{
    if (Scancode == RR_SCANCODE_UNKNOWN)
    {
        return;
    }

    if (!Down && !gPlatform.PressedKeys[Scancode])
    {
        return;
    }

    bool WasDown = gPlatform.PressedKeys[Scancode];
    gPlatform.PressedKeys[Scancode] = Down;

    switch (Scancode)
    {
        case RR_SCANCODE_LCTRL:
        {
            Rr_UpdateKeymodState(RR_KEYMOD_LCTRL, Down);
        }
        break;
        case RR_SCANCODE_LSHIFT:
        {
            Rr_UpdateKeymodState(RR_KEYMOD_LSHIFT, Down);
        }
        break;
        case RR_SCANCODE_LALT:
        {
            Rr_UpdateKeymodState(RR_KEYMOD_LALT, Down);
        }
        break;
        case RR_SCANCODE_LSUPER:
        {
            Rr_UpdateKeymodState(RR_KEYMOD_LSUPER, Down);
        }
        break;
        case RR_SCANCODE_RCTRL:
        {
            Rr_UpdateKeymodState(RR_KEYMOD_RCTRL, Down);
        }
        break;
        case RR_SCANCODE_RSHIFT:
        {
            Rr_UpdateKeymodState(RR_KEYMOD_RSHIFT, Down);
        }
        break;
        case RR_SCANCODE_RALT:
        {
            Rr_UpdateKeymodState(RR_KEYMOD_RALT, Down);
        }
        break;
        case RR_SCANCODE_RSUPER:
        {
            Rr_UpdateKeymodState(RR_KEYMOD_RSUPER, Down);
        }
        break;
        default:
        {
        }
        break;
    }

    Rr_Event *Event = Rr_AddEvent();
    if (WasDown && Down)
    {
        Event->Type = RR_EVENT_TYPE_KEY_REPEAT;
    }
    else
    {
        Event->Type = Down ? RR_EVENT_TYPE_KEY_DOWN : RR_EVENT_TYPE_KEY_UP;

        if (Down)
        {
            gPlatform.PressedKeyCount++;
        }
        else
        {
            gPlatform.PressedKeyCount--;
        }
    }
    Event->Key.Keymod = gPlatform.Keymod;
    Event->Key.Scancode = Scancode;
    Event->Key.Down = Down;

    if (Scancode == RR_SCANCODE_F4 && gPlatform.Keymod & RR_KEYMOD_ALT)
    {
        Event = Rr_AddEvent();
        Event->Type = RR_EVENT_TYPE_QUIT_REQUESTED;
    }
}

void Rr_AddMouseMotionEvent(Rr_Vec2 Position)
{
    Rr_Event *Event = Rr_AddEvent();
    Event->Type = RR_EVENT_TYPE_MOUSE_MOTION;
    Event->MouseMotion.Position = Position;
}

void Rr_AddMouseWheelEvent(Rr_Vec2 Position, Rr_Vec2 Amount)
{
    Rr_Event *Event = Rr_AddEvent();
    Event->Type = RR_EVENT_TYPE_MOUSE_WHEEL;
    Event->Wheel.Position = Position;
    Event->Wheel.Amount = Amount;
}

void Rr_AddMouseButtonEvent(bool Down, Rr_Vec2 Position, Rr_MouseButton Button)
{
    Rr_Event *Event = Rr_AddEvent();
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

void Rr_AddTextInputEventString(char const *CString, size_t Length)
{
    Rr_Event *Event = Rr_AddEvent();
    Event->Type = RR_EVENT_TYPE_TEXT_INPUT;
    Event->Text.CString = CString;
    Event->Text.Length = Length;
}

void Rr_AddTextInputEvent(uint32_t Codepoint, Rr_Arena *Arena)
{
    if ((Codepoint == 127 || (Codepoint >= 0 && Codepoint <= 31)))
    {
        return;
    }

    char *Buffer = RR_ALLOC_NO_ZERO(5, Arena);
    Rr_CodepointToUTF8(Codepoint, Buffer);

    Rr_AddTextInputEventString(Buffer, strlen(Buffer));
}

void Rr_AddDropFileEvent(char const *Path)
{
    if (!Path)
    {
        return;
    }

    Rr_Event *Event = Rr_AddEvent();
    Event->Type = RR_EVENT_TYPE_DROP_FILE;
    Event->DropFile.Path = Path;
}

void Rr_AddFocusEvent(bool HasFocus)
{
    Rr_Event *Event = Rr_AddEvent();
    Event->Type = RR_EVENT_TYPE_FOCUS;
    Event->Focus.Focused = HasFocus;

    if (!HasFocus)
    {
        Rr_ReleaseAllInput();
    }
}
