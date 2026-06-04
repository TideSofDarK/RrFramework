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
#include "Rr_LogMacro.h"
#include "Rr_Vulkan.h"

#include <Rr/Rr_Utility.h>

#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xcb_util.h>
#include <xcb/xkb.h>
#include <xkbcommon/xkbcommon-x11.h>

#include <vulkan/vulkan_xcb.h>

#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>

typedef enum
{
    RR_XCB_CLIPBOARD_TARGET_UTF8_STRING,
    RR_XCB_CLIPBOARD_TARGET_COMPOUND_TEXT,
    RR_XCB_CLIPBOARD_TARGET_TEXT,
    RR_XCB_CLIPBOARD_TARGET_STRING,
    RR_XCB_CLIPBOARD_TARGET_TEXT_PLAIN_UTF8,
    RR_XCB_CLIPBOARD_TARGET_TEXT_PLAIN,
    RR_XCB_CLIPBOARD_TARGET_COUNT,
} Rr_XCBClipboardTarget;

static const char *RR_XCB_CLIPBOARD_TARGETS[RR_XCB_CLIPBOARD_TARGET_COUNT] = {
    "UTF8_STRING",
    "COMPOUND_TEXT",
    "TEXT",
    "STRING",
    "text/plain;charset=utf-8",
    "text/plain",
};

static struct Rr_Platform_XCB
{
    bool Initialized;

    xcb_connection_t *Connection;
    const xcb_setup_t *Setup;
    xcb_screen_t *Screen;

    xcb_window_t Window;
    bool FirstTimeShowFullscreen;
    bool Resizable;
    xcb_cursor_context_t *CursorContext;
    xcb_cursor_t Cursors[RR_CURSOR_TYPE_COUNT];
    xcb_cursor_t EmptyCursor;
    struct
    {
        xcb_atom_t Targets;
        xcb_atom_t Clipboard;
        xcb_atom_t UTF8String;
        xcb_atom_t TextURIList;
        xcb_atom_t WMProtocols;
        xcb_atom_t WMDeleteWindow;
        xcb_atom_t XdndTypeList;
        xcb_atom_t XdndSelection;
        xcb_atom_t XdndEnter;
        xcb_atom_t XdndPosition;
        xcb_atom_t XdndStatus;
        xcb_atom_t XdndLeave;
        xcb_atom_t XdndDrop;
        xcb_atom_t XdndFinished;
        xcb_atom_t XdndActionCopy;
        xcb_atom_t XdndAware;
    } Atoms;

    struct xkb_context *XKBContext;
    struct xkb_keymap *XKBKeymap;
    struct xkb_state *XKBState;
    struct xkb_state *XKBStateNone;
    int32_t XKBDeviceID;
    Rr_Scancode XKBKeycodeToScancode[256];

    bool UseRandr;

    uint32_t XdndVersion;
    xcb_window_t XdndSource;
    bool XdndKnownTarget;

    xcb_atom_t ClipboardTargets[RR_XCB_CLIPBOARD_TARGET_COUNT];
    char *Clipboard;
    size_t ClipboardLength;

    void *VulkanModule;
    PFN_vkGetInstanceProcAddr VkGetInstanceProcAddr;
    PFN_vkCreateXcbSurfaceKHR VkCreateXcbSurfaceKHR;
} gXCB;

typedef struct xkb_generic_event_t xkb_generic_event_t;
struct xkb_generic_event_t
{
    uint8_t response_type;
    uint8_t xkb_type;
    uint16_t sequence;
    xcb_timestamp_t time;
    uint8_t deviceId;
};

static inline xcb_atom_t Rr_GetXCBAtom(const char *CString)
{
    xcb_intern_atom_reply_t *Reply = xcb_intern_atom_reply(
        gXCB.Connection,
        xcb_intern_atom(
            gXCB.Connection,
            false,
            (uint16_t)strlen(CString),
            CString),
        NULL);
    if (!Reply)
    {
        return XCB_NONE;
    }
    xcb_atom_t Atom = Reply->atom;
    free(Reply);

    return Atom;
}

static inline Rr_Scancode Rr_XCBKeyNameToScancode(char const XCBKeyName[4])
{
    static struct
    {
        Rr_Scancode Scancode;
        char const *Name;
    } const Mappings[] = {
        { RR_SCANCODE_A, "AC01" },
        { RR_SCANCODE_B, "AB05" },
        { RR_SCANCODE_C, "AB03" },
        { RR_SCANCODE_D, "AC03" },
        { RR_SCANCODE_E, "AD03" },
        { RR_SCANCODE_F, "AC04" },
        { RR_SCANCODE_G, "AC05" },
        { RR_SCANCODE_H, "AC06" },
        { RR_SCANCODE_I, "AD08" },
        { RR_SCANCODE_J, "AC07" },
        { RR_SCANCODE_K, "AC08" },
        { RR_SCANCODE_L, "AC09" },
        { RR_SCANCODE_M, "AB07" },
        { RR_SCANCODE_N, "AB06" },
        { RR_SCANCODE_O, "AD09" },
        { RR_SCANCODE_P, "AD10" },
        { RR_SCANCODE_Q, "AD01" },
        { RR_SCANCODE_R, "AD04" },
        { RR_SCANCODE_S, "AC02" },
        { RR_SCANCODE_T, "AD05" },
        { RR_SCANCODE_U, "AD07" },
        { RR_SCANCODE_V, "AB04" },
        { RR_SCANCODE_W, "AD02" },
        { RR_SCANCODE_X, "AB02" },
        { RR_SCANCODE_Y, "AD06" },
        { RR_SCANCODE_Z, "AB01" },
        { RR_SCANCODE_1, "AE01" },
        { RR_SCANCODE_2, "AE02" },
        { RR_SCANCODE_3, "AE03" },
        { RR_SCANCODE_4, "AE04" },
        { RR_SCANCODE_5, "AE05" },
        { RR_SCANCODE_6, "AE06" },
        { RR_SCANCODE_7, "AE07" },
        { RR_SCANCODE_8, "AE08" },
        { RR_SCANCODE_9, "AE09" },
        { RR_SCANCODE_0, "AE10" },
        { RR_SCANCODE_RETURN, "RTRN" },
        { RR_SCANCODE_ESCAPE, "ESC" },
        { RR_SCANCODE_BACKSPACE, "BKSP" },
        { RR_SCANCODE_TAB, "TAB" },
        { RR_SCANCODE_SPACE, "SPCE" },
        { RR_SCANCODE_MINUS, "AE11" },
        { RR_SCANCODE_EQUALS, "AE12" },
        { RR_SCANCODE_LEFT_BRACKET, "AD11" },
        { RR_SCANCODE_RIGHT_BRACKET, "AD12" },
        { RR_SCANCODE_BACKSLASH, "BKSL" },
        { RR_SCANCODE_SEMICOLON, "AC10" },
        { RR_SCANCODE_APOSTROPHE, "AC11" },
        { RR_SCANCODE_GRAVE_TILDE, "TLDE" },
        { RR_SCANCODE_COMMA, "AB08" },
        { RR_SCANCODE_PERIOD, "AB09" },
        { RR_SCANCODE_SLASH, "AB10" },
        { RR_SCANCODE_CAPS_LOCK, "CAPS" },
        { RR_SCANCODE_F1, "FK01" },
        { RR_SCANCODE_F2, "FK02" },
        { RR_SCANCODE_F3, "FK03" },
        { RR_SCANCODE_F4, "FK04" },
        { RR_SCANCODE_F5, "FK05" },
        { RR_SCANCODE_F6, "FK06" },
        { RR_SCANCODE_F7, "FK07" },
        { RR_SCANCODE_F8, "FK08" },
        { RR_SCANCODE_F9, "FK09" },
        { RR_SCANCODE_F10, "FK10" },
        { RR_SCANCODE_F11, "FK11" },
        { RR_SCANCODE_F12, "FK12" },
        { RR_SCANCODE_PRINT_SCREEN, "PRSC" },
        { RR_SCANCODE_SCROLL_LOCK, "SCLK" },
        { RR_SCANCODE_PAUSE, "PAUS" },
        { RR_SCANCODE_INSERT, "INS" },
        { RR_SCANCODE_HOME, "HOME" },
        { RR_SCANCODE_PAGE_UP, "PGUP" },
        { RR_SCANCODE_DELETE, "DELE" },
        { RR_SCANCODE_END, "END" },
        { RR_SCANCODE_PAGE_DOWN, "PGDN" },
        { RR_SCANCODE_RIGHT, "RGHT" },
        { RR_SCANCODE_LEFT, "LEFT" },
        { RR_SCANCODE_DOWN, "DOWN" },
        { RR_SCANCODE_UP, "UP" },
        { RR_SCANCODE_NUMLOCK_CLEAR, "NMLK" },
        { RR_SCANCODE_KP_DIVIDE, "KPDV" },
        { RR_SCANCODE_KP_MULTIPLY, "KPMU" },
        { RR_SCANCODE_KP_MINUS, "KPSU" },
        { RR_SCANCODE_KP_PLUS, "KPAD" },
        { RR_SCANCODE_KP_ENTER, "KPEN" },
        { RR_SCANCODE_KP_1, "KP1" },
        { RR_SCANCODE_KP_2, "KP2" },
        { RR_SCANCODE_KP_3, "KP3" },
        { RR_SCANCODE_KP_4, "KP4" },
        { RR_SCANCODE_KP_5, "KP5" },
        { RR_SCANCODE_KP_6, "KP6" },
        { RR_SCANCODE_KP_7, "KP7" },
        { RR_SCANCODE_KP_8, "KP8" },
        { RR_SCANCODE_KP_9, "KP9" },
        { RR_SCANCODE_KP_0, "KP0" },
        { RR_SCANCODE_KP_PERIOD_DELETE, "KPDL" },
        { RR_SCANCODE_ISO_BACKSLASH, "LSGT" },
        { RR_SCANCODE_KP_EQUALS, "KPEQ" },
        { RR_SCANCODE_F13, "FK13" },
        { RR_SCANCODE_F14, "FK14" },
        { RR_SCANCODE_F15, "FK15" },
        { RR_SCANCODE_F16, "FK16" },
        { RR_SCANCODE_F17, "FK17" },
        { RR_SCANCODE_F18, "FK18" },
        { RR_SCANCODE_F19, "FK19" },
        { RR_SCANCODE_F20, "FK20" },
        { RR_SCANCODE_F21, "FK21" },
        { RR_SCANCODE_F22, "FK22" },
        { RR_SCANCODE_F23, "FK23" },
        { RR_SCANCODE_F24, "FK24" },
        { RR_SCANCODE_MENU, "MENU" },
        { RR_SCANCODE_LCTRL, "LCTL" },
        { RR_SCANCODE_LSHIFT, "LFSH" },
        { RR_SCANCODE_LALT, "LALT" },
        { RR_SCANCODE_LSUPER, "LWIN" },
        { RR_SCANCODE_RCTRL, "RCTL" },
        { RR_SCANCODE_RSHIFT, "RTSH" },
        { RR_SCANCODE_RALT, "RALT" },
        { RR_SCANCODE_RALT, "LVL3" },
        { RR_SCANCODE_RALT, "MDSW" },
        { RR_SCANCODE_RSUPER, "RWIN" },
    };

    for (size_t Index = 0; Index < RR_ARRAY_COUNT(Mappings); ++Index)
    {
        if (strncmp(
                Mappings[Index].Name,
                XCBKeyName,
                strlen(Mappings[Index].Name)) == 0)
        {
            return Mappings[Index].Scancode;
        }
    }

    return RR_SCANCODE_UNKNOWN;
}

static inline bool Rr_UpdateXKBKeymap(void)
{
    if (gXCB.XKBKeymap)
    {
        xkb_keymap_unref(gXCB.XKBKeymap);
    }
    if (gXCB.XKBState)
    {
        xkb_state_unref(gXCB.XKBState);
    }
    if (gXCB.XKBStateNone)
    {
        xkb_state_unref(gXCB.XKBStateNone);
    }

    gXCB.XKBKeymap = xkb_x11_keymap_new_from_device(
        gXCB.XKBContext,
        gXCB.Connection,
        gXCB.XKBDeviceID,
        XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!gXCB.XKBKeymap)
    {
        return false;
    }

    gXCB.XKBState = xkb_x11_state_new_from_device(
        gXCB.XKBKeymap,
        gXCB.Connection,
        gXCB.XKBDeviceID);
    if (!gXCB.XKBState)
    {
        xkb_keymap_unref(gXCB.XKBKeymap);
        gXCB.XKBKeymap = NULL;

        return false;
    }

    gXCB.XKBStateNone = xkb_x11_state_new_from_device(
        gXCB.XKBKeymap,
        gXCB.Connection,
        gXCB.XKBDeviceID);
    if (!gXCB.XKBStateNone)
    {
        xkb_keymap_unref(gXCB.XKBKeymap);
        gXCB.XKBKeymap = NULL;
        xkb_state_unref(gXCB.XKBState);
        gXCB.XKBState = NULL;

        return false;
    }
    xkb_layout_index_t Layout = xkb_state_key_get_layout(gXCB.XKBStateNone, 0);
    xkb_state_update_mask(gXCB.XKBStateNone, 2, 2, 2, Layout, Layout, Layout);

    xcb_xkb_get_names_reply_t *Reply = xcb_xkb_get_names_reply(
        gXCB.Connection,
        xcb_xkb_get_names(
            gXCB.Connection,
            (xcb_xkb_device_spec_t)gXCB.XKBDeviceID,
            XCB_XKB_NAME_DETAIL_KEY_NAMES | XCB_XKB_NAME_DETAIL_KEY_ALIASES |
                XCB_XKB_NAME_DETAIL_SYMBOLS),
        NULL);
    if (!Reply)
    {
        return false;
    }
    xcb_xkb_get_names_value_list_t Values = { 0 };
    void *NamesValueList = xcb_xkb_get_names_value_list(Reply);
    xcb_xkb_get_names_value_list_unpack(
        NamesValueList,
        Reply->nTypes,
        Reply->indicators,
        Reply->virtualMods,
        Reply->groupNames,
        Reply->nKeys,
        Reply->nKeyAliases,
        Reply->nRadioGroups,
        Reply->which,
        &Values);
    int KeyNameCount =
        xcb_xkb_get_names_value_list_key_names_length(Reply, &Values);
    xcb_xkb_key_name_iterator_t It =
        xcb_xkb_get_names_value_list_key_names_iterator(Reply, &Values);
    for (int Index = 0; Index < KeyNameCount; Index++)
    {
        xcb_xkb_key_name_t *KeyName = It.data;
        if (KeyName)
        {
            xcb_keycode_t Keycode = (xcb_keycode_t)(Reply->firstKey + Index);

            Rr_Scancode Scancode = Rr_XCBKeyNameToScancode(KeyName->name);
            gXCB.XKBKeycodeToScancode[Keycode] = Scancode;
        }

        xcb_xkb_key_name_next(&It);
    }

    free(Reply);

    return true;
}

static inline bool Rr_InitXKB(void)
{
    xcb_xkb_use_extension(
        gXCB.Connection,
        XKB_X11_MIN_MAJOR_XKB_VERSION,
        XKB_X11_MIN_MINOR_XKB_VERSION);
    struct xkb_context *Context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!Context)
    {
        return false;
    }
    gXCB.XKBContext = Context;

    int32_t DeviceID = xkb_x11_get_core_keyboard_device_id(gXCB.Connection);
    if (DeviceID == -1)
    {
        return false;
    }
    gXCB.XKBDeviceID = DeviceID;

    xcb_xkb_per_client_flags(
        gXCB.Connection,
        (xcb_xkb_device_spec_t)DeviceID,
        XCB_XKB_PER_CLIENT_FLAG_DETECTABLE_AUTO_REPEAT,
        1,
        0,
        0,
        0);

    static const uint16_t StateDetails =
        XCB_XKB_STATE_PART_MODIFIER_BASE | XCB_XKB_STATE_PART_MODIFIER_LATCH |
        XCB_XKB_STATE_PART_MODIFIER_LOCK | XCB_XKB_STATE_PART_GROUP_BASE |
        XCB_XKB_STATE_PART_GROUP_LATCH | XCB_XKB_STATE_PART_GROUP_LOCK;
    static const xcb_xkb_select_events_details_t SelectEventsDetails = {
        .affectNewKeyboard = XCB_XKB_NKN_DETAIL_KEYCODES,
        .newKeyboardDetails = XCB_XKB_NKN_DETAIL_KEYCODES,
        .affectState = StateDetails,
        .stateDetails = StateDetails,
    };
    static const uint16_t RequiredMapParts =
        XCB_XKB_MAP_PART_KEY_TYPES | XCB_XKB_MAP_PART_KEY_SYMS |
        XCB_XKB_MAP_PART_MODIFIER_MAP | XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS |
        XCB_XKB_MAP_PART_KEY_ACTIONS | XCB_XKB_MAP_PART_VIRTUAL_MODS |
        XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP;
    static const xcb_xkb_event_type_t SelectedEvents =
        XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY |
        XCB_XKB_EVENT_TYPE_STATE_NOTIFY;
    xcb_generic_error_t *SelectEventsError = xcb_request_check(
        gXCB.Connection,
        xcb_xkb_select_events_aux_checked(
            gXCB.Connection,
            (xcb_xkb_device_spec_t)DeviceID,
            SelectedEvents,
            0,
            0,
            RequiredMapParts,
            RequiredMapParts,
            &SelectEventsDetails));
    if (SelectEventsError)
    {
        free(SelectEventsError);
        return false;
    }

    if (!Rr_UpdateXKBKeymap())
    {
        return false;
    }

    return true;
}

bool Rr_ProcessXKBEvent(xkb_generic_event_t *Event)
{
    if (Event->deviceId != gXCB.XKBDeviceID)
    {
        return false;
    }

    switch (Event->xkb_type)
    {
        case XCB_XKB_NEW_KEYBOARD_NOTIFY:
        {
            xcb_xkb_new_keyboard_notify_event_t *NewKeyboardNotifyEvent =
                (xcb_xkb_new_keyboard_notify_event_t *)Event;
            if (NewKeyboardNotifyEvent->changed)
            {
                Rr_UpdateXKBKeymap();

                return true;
            }
        }
        break;
        case XCB_XKB_MAP_NOTIFY:
        {
            Rr_UpdateXKBKeymap();

            return true;
        }
        break;
        case XCB_XKB_STATE_NOTIFY:
        {
            xcb_xkb_state_notify_event_t *StateNotifyEvent =
                (xcb_xkb_state_notify_event_t *)Event;
            xkb_state_update_mask(
                gXCB.XKBState,
                StateNotifyEvent->baseMods,
                StateNotifyEvent->latchedMods,
                StateNotifyEvent->lockedMods,
                (uint32_t)StateNotifyEvent->baseGroup,
                (uint32_t)StateNotifyEvent->latchedGroup,
                StateNotifyEvent->lockedGroup);

            return true;
        }
        default:
        {
        }
        break;
    }

    return false;
}

static inline void Rr_ProcessXdndEvent(xcb_client_message_event_t *Event)
{
    if (Event->type == gXCB.Atoms.XdndEnter)
    {
        uint32_t Version = Event->data.data32[1] >> 24;
        if (Version > 5)
        {
            return;
        }

        xcb_window_t Source = Event->data.data32[0];
        bool IsList = Event->data.data32[1] & 1;
        size_t Count = 0;
        xcb_atom_t *Formats = NULL;
        xcb_atom_t AltFormats[3] = { 0 };
        xcb_get_property_reply_t *Reply = NULL;
        if (IsList)
        {
            Reply = xcb_get_property_reply(
                gXCB.Connection,
                xcb_get_property(
                    gXCB.Connection,
                    0,
                    Source,
                    gXCB.Atoms.XdndTypeList,
                    XCB_ATOM_ATOM,
                    0,
                    UINT32_MAX),
                NULL);
            if (!Reply)
            {
                return;
            }
            Count = (size_t)xcb_get_property_value_length(Reply) /
                    sizeof(xcb_atom_t);
            Formats = (xcb_atom_t *)xcb_get_property_value(Reply);
        }
        else
        {
            Count = 0;
            if (Event->data.data32[2] != XCB_NONE)
            {
                AltFormats[Count++] = Event->data.data32[2];
            }
            if (Event->data.data32[3] != XCB_NONE)
            {
                AltFormats[Count++] = Event->data.data32[3];
            }
            if (Event->data.data32[4] != XCB_NONE)
            {
                AltFormats[Count++] = Event->data.data32[4];
            }

            Formats = AltFormats;
        }

        bool KnownTarget = XCB_NONE;
        for (size_t Index = 0; Index < Count; ++Index)
        {
            if (Formats[Index] == gXCB.Atoms.TextURIList)
            {
                KnownTarget = Formats[Index];

                break;
            }
        }

        if (KnownTarget)
        {
            gXCB.XdndKnownTarget = true;
        }
        gXCB.XdndVersion = Version;
        gXCB.XdndSource = Source;

        if (Reply)
        {
            free(Reply);
        }
    }

    if (Event->type == gXCB.Atoms.XdndPosition)
    {
        uint32_t Version = Event->data.data32[1] >> 24;
        if (Version > 5)
        {
            return;
        }

        xcb_window_t Source = Event->data.data32[0];
        xcb_client_message_event_t StatusEvent = {
            .response_type = XCB_CLIENT_MESSAGE,
            .format = 32,
            .window = Source,
            .type = gXCB.Atoms.XdndStatus,
            .data.data32[0] = gXCB.Window,
        };
        if (gXCB.XdndKnownTarget)
        {
            StatusEvent.data.data32[1] = 1;
            if (Version >= 2)
            {
                StatusEvent.data.data32[4] = gXCB.Atoms.XdndActionCopy;
            }
        }
        xcb_send_event(
            gXCB.Connection,
            false,
            Source,
            XCB_EVENT_MASK_NO_EVENT,
            (void *)&StatusEvent);
        xcb_flush(gXCB.Connection);
    }

    if (Event->type == gXCB.Atoms.XdndDrop)
    {
        uint32_t Version = Event->data.data32[1] >> 24;
        if (Version > 5)
        {
            return;
        }

        xcb_window_t Source = Event->data.data32[0];
        if (gXCB.XdndKnownTarget)
        {
            xcb_timestamp_t Time = XCB_CURRENT_TIME;
            if (Version >= 1)
            {
                Time = Event->data.data32[2];
            }
            xcb_convert_selection(
                gXCB.Connection,
                gXCB.Window,
                gXCB.Atoms.XdndSelection,
                gXCB.Atoms.TextURIList,
                gXCB.Atoms.XdndSelection,
                Time);
            xcb_flush(gXCB.Connection);
        }
        else if (Version >= 2)
        {
            xcb_client_message_event_t FinishedEvent = {
                .response_type = XCB_CLIENT_MESSAGE,
                .format = 32,
                .window = Source,
                .type = gXCB.Atoms.XdndFinished,
            };
            xcb_send_event(
                gXCB.Connection,
                false,
                Source,
                XCB_EVENT_MASK_NO_EVENT,
                (void *)&FinishedEvent);
            xcb_flush(gXCB.Connection);
        }
    }
}

static inline bool Rr_InitRandr(void)
{
    xcb_generic_error_t *Error;
    xcb_randr_query_version_reply(
        gXCB.Connection,
        xcb_randr_query_version(gXCB.Connection, 1, 5),
        &Error);
    if (Error)
    {
        free(Error);

        return false;
    }

    xcb_randr_select_input(gXCB.Connection, gXCB.Screen->root, true);

    return true;
}

static inline void Rr_WarpXCBPointer(Rr_IntVec2 Position)
{
    xcb_warp_pointer(
        gXCB.Connection,
        XCB_NONE,
        gXCB.Window,
        0,
        0,
        0,
        0,
        (int16_t)Position.X,
        (int16_t)Position.Y);
}

static inline void Rr_ForceXCBWindowSize(Rr_IntVec2 WindowSize)
{
    enum WMSizeHintsFlag
    {
        WM_SIZE_HINT_US_POSITION = 1U << 0,
        WM_SIZE_HINT_US_SIZE = 1U << 1,
        WM_SIZE_HINT_P_POSITION = 1U << 2,
        WM_SIZE_HINT_P_SIZE = 1U << 3,
        WM_SIZE_HINT_P_MIN_SIZE = 1U << 4,
        WM_SIZE_HINT_P_MAX_SIZE = 1U << 5,
        WM_SIZE_HINT_P_RESIZE_INC = 1U << 6,
        WM_SIZE_HINT_P_ASPECT = 1U << 7,
        WM_SIZE_HINT_BASE_SIZE = 1U << 8,
        WM_SIZE_HINT_P_WIN_GRAVITY = 1U << 9
    };
    struct WMSizeHints
    {
        uint32_t Flags;
        int32_t X, Y;
        int32_t Width, Height;
        int32_t MinWidth, MinHeight;
        int32_t MaxWidth, MaxHeight;
        int32_t WidthInc, HeightInc;
        int32_t MinAspectNum, MinAspectDen;
        int32_t MaxAspectNum, MaxAspectDen;
        int32_t BaseWidth, BaseHeight;
        uint32_t WinGravity;
    } Hints = {
        .Flags = WM_SIZE_HINT_P_MIN_SIZE | WM_SIZE_HINT_P_MAX_SIZE,
        .MinWidth = WindowSize.X,
        .MinHeight = WindowSize.Y,
        .MaxWidth = WindowSize.X,
        .MaxHeight = WindowSize.Y,
    };
    xcb_change_property(
        gXCB.Connection,
        XCB_PROP_MODE_REPLACE,
        gXCB.Window,
        XCB_ATOM_WM_NORMAL_HINTS,
        XCB_ATOM_WM_SIZE_HINTS,
        32,
        sizeof(struct WMSizeHints) / sizeof(uint32_t),
        &Hints);
}

static inline void *Rr_OpenVulkanModuleLinux(void)
{
    int Flags = RTLD_NOW | RTLD_LOCAL;
    void *Module = dlopen("libvulkan.so.1", Flags);
    if (!Module)
    {
        Module = dlopen("libvulkan.so", Flags);
    }
    return Module;
}

bool Rr_InitPlatform(Rr_AppConfig *Config)
{
    assert(!gXCB.Initialized);

    RR_LOG_INFO("Using XCB");

    xcb_generic_error_t *XCBError = NULL;

    int ScreenIndex;
    xcb_connection_t *Connection = xcb_connect(NULL, &ScreenIndex);
    if (!Connection)
    {
        return false;
    }
    gXCB.Connection = Connection;

    xcb_setup_t const *Setup = xcb_get_setup(Connection);
    if (!Setup)
    {
        return false;
    }
    gXCB.Setup = Setup;

    xcb_screen_t *Screen = NULL;
    xcb_screen_iterator_t ScreenIt = xcb_setup_roots_iterator(Setup);
    for (; ScreenIt.rem; --ScreenIndex, xcb_screen_next(&ScreenIt))
    {
        if (ScreenIndex == 0)
        {
            Screen = ScreenIt.data;
            break;
        }
    }
    if (!Screen)
    {
        return false;
    }
    gXCB.Screen = Screen;

    Rr_IntVec2 WindowSize = {
        (int32_t)((float)Screen->width_in_pixels * RR_WINDOWED_RATIO),
        (int32_t)((float)Screen->height_in_pixels * RR_WINDOWED_RATIO)
    };
    Rr_IntVec2 WindowPosition = {
        Screen->width_in_pixels / 2 - WindowSize.X / 2,
        Screen->height_in_pixels / 2 - WindowSize.Y / 2
    };

    uint32_t ValueMask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t ValueList[] = {
        Screen->black_pixel,
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
            XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_EXPOSURE |
            XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_FOCUS_CHANGE |
            XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_VISIBILITY_CHANGE |
            XCB_EVENT_MASK_KEYMAP_STATE | XCB_EVENT_MASK_ENTER_WINDOW
    };

    uint32_t Window = xcb_generate_id(Connection);
    XCBError = xcb_request_check(
        Connection,
        xcb_create_window_checked(
            Connection,
            XCB_COPY_FROM_PARENT,
            Window,
            Screen->root,
            (int16_t)WindowPosition.X,
            (int16_t)WindowPosition.Y,
            (uint16_t)WindowSize.X,
            (uint16_t)WindowSize.Y,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            Screen->root_visual,
            ValueMask,
            ValueList));
    if (XCBError)
    {
        RR_LOG_ERROR("Failed to create XCB window!");

        free(XCBError);

        return false;
    }
    gXCB.Window = Window;

    gXCB.FirstTimeShowFullscreen =
        Config->WindowFlags & RR_WINDOW_FLAGS_FULLSCREEN_BIT;
    gXCB.Resizable = Config->WindowFlags & RR_WINDOW_FLAGS_RESIZE_BIT;
    if (!gXCB.Resizable)
    {
        Rr_ForceXCBWindowSize(WindowSize);
    }

    if (Rr_InitRandr())
    {
        gXCB.UseRandr = true;
    }

    gXCB.Atoms.Targets = Rr_GetXCBAtom("TARGETS");
    gXCB.Atoms.Clipboard = Rr_GetXCBAtom("CLIPBOARD");
    gXCB.Atoms.UTF8String = Rr_GetXCBAtom("UTF8_STRING");
    gXCB.Atoms.TextURIList = Rr_GetXCBAtom("text/uri-list");
    gXCB.Atoms.WMProtocols = Rr_GetXCBAtom("WM_PROTOCOLS");
    gXCB.Atoms.WMDeleteWindow = Rr_GetXCBAtom("WM_DELETE_WINDOW");
    xcb_change_property(
        gXCB.Connection,
        XCB_PROP_MODE_REPLACE,
        gXCB.Window,
        gXCB.Atoms.WMProtocols,
        4,
        32,
        1,
        &gXCB.Atoms.WMDeleteWindow);
    gXCB.Atoms.XdndTypeList = Rr_GetXCBAtom("XdndTypeList");
    gXCB.Atoms.XdndSelection = Rr_GetXCBAtom("XdndSelection");
    gXCB.Atoms.XdndEnter = Rr_GetXCBAtom("XdndEnter");
    gXCB.Atoms.XdndPosition = Rr_GetXCBAtom("XdndPosition");
    gXCB.Atoms.XdndStatus = Rr_GetXCBAtom("XdndStatus");
    gXCB.Atoms.XdndLeave = Rr_GetXCBAtom("XdndLeave");
    gXCB.Atoms.XdndDrop = Rr_GetXCBAtom("XdndDrop");
    gXCB.Atoms.XdndFinished = Rr_GetXCBAtom("XdndFinished");
    gXCB.Atoms.XdndActionCopy = Rr_GetXCBAtom("XdndActionCopy");
    gXCB.Atoms.XdndAware = Rr_GetXCBAtom("XdndAware");
    uint32_t XdndVersion = 5;
    xcb_change_property(
        Connection,
        XCB_PROP_MODE_REPLACE,
        Window,
        gXCB.Atoms.XdndAware,
        XCB_ATOM_ATOM,
        32,
        1,
        &XdndVersion);

    for (size_t Index = 0; Index < RR_XCB_CLIPBOARD_TARGET_COUNT; ++Index)
    {
        gXCB.ClipboardTargets[Index] =
            Rr_GetXCBAtom(RR_XCB_CLIPBOARD_TARGETS[Index]);
    }

    xcb_cursor_context_t *CursorContext = NULL;
    xcb_cursor_context_new(
        Connection,
        xcb_setup_roots_iterator(Setup).data,
        &CursorContext);
    if (!CursorContext)
    {
        return false;
    }
    gXCB.CursorContext = CursorContext;
    gXCB.Cursors[RR_CURSOR_TYPE_NORMAL] =
        xcb_cursor_load_cursor(CursorContext, "default");
    gXCB.Cursors[RR_CURSOR_TYPE_RESIZE_EW] =
        xcb_cursor_load_cursor(CursorContext, "ew-resize");
    gXCB.Cursors[RR_CURSOR_TYPE_RESIZE_NS] =
        xcb_cursor_load_cursor(CursorContext, "ns-resize");
    gXCB.Cursors[RR_CURSOR_TYPE_RESIZE_NWSE] =
        xcb_cursor_load_cursor(CursorContext, "nwse-resize");
    gXCB.Cursors[RR_CURSOR_TYPE_RESIZE_NESW] =
        xcb_cursor_load_cursor(CursorContext, "nesw-resize");
    gXCB.Cursors[RR_CURSOR_TYPE_RESIZE_ALL] =
        xcb_cursor_load_cursor(CursorContext, "all-scroll");
    gXCB.Cursors[RR_CURSOR_TYPE_TEXT] =
        xcb_cursor_load_cursor(CursorContext, "text");
    xcb_pixmap_t Pixmap = xcb_generate_id(Connection);
    gXCB.EmptyCursor = xcb_generate_id(Connection);
    xcb_create_pixmap(Connection, 1, Pixmap, Screen->root, 1, 1);
    xcb_create_cursor(
        Connection,
        gXCB.EmptyCursor,
        Pixmap,
        Pixmap,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0);
    xcb_free_pixmap(Connection, Pixmap);

    Rr_SetWindowTitle(Config->Title);

    xcb_flush(Connection);

    if (!Rr_InitXKB())
    {
        return false;
    }

    void *Module = Rr_OpenVulkanModuleLinux();
    if (!Module)
    {
        return false;
    }
    gXCB.VulkanModule = Module;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    gXCB.VkGetInstanceProcAddr =
        (PFN_vkGetInstanceProcAddr)dlsym(Module, "vkGetInstanceProcAddr");
    if (!gXCB.VkGetInstanceProcAddr)
    {
        return false;
    }
    gXCB.VkCreateXcbSurfaceKHR = (PFN_vkCreateXcbSurfaceKHR)dlsym(
        gXCB.VulkanModule,
        "vkCreateXcbSurfaceKHR");
    if (!gXCB.VkCreateXcbSurfaceKHR)
    {
        return false;
    }
#pragma GCC diagnostic pop // Restore diagnostics state

    gXCB.Initialized = true;

    return true;
}

void Rr_CleanupPlatform(void)
{
    assert(gXCB.Initialized);

    for (int Index = 0; Index < RR_CURSOR_TYPE_COUNT; ++Index)
    {
        xcb_free_cursor(gXCB.Connection, gXCB.Cursors[Index]);
    }
    xcb_cursor_context_free(gXCB.CursorContext);

    xkb_state_unref(gXCB.XKBState);
    xkb_keymap_unref(gXCB.XKBKeymap);
    xkb_context_unref(gXCB.XKBContext);

    xcb_disconnect(gXCB.Connection);

    dlclose(gXCB.VulkanModule);

    free(gXCB.Clipboard);

    RR_ZERO(gXCB);
}

void (*Rr_GetVkGetInstanceProcAddr(void))(void)
{
    return (void (*)(void))gXCB.VkGetInstanceProcAddr;
}

const char *const *Rr_GetVulkanExtensions(uint32_t *Count)
{
    static char const *Extensions[] = { VK_KHR_SURFACE_EXTENSION_NAME,
                                        VK_KHR_XCB_SURFACE_EXTENSION_NAME };

    if (Count)
    {
        *Count = RR_ARRAY_COUNT(Extensions);
    }

    return Extensions;
}

bool Rr_CreateVulkanSurface(uint64_t Instance, uint64_t *Surface)
{
    VkXcbSurfaceCreateInfoKHR Info = {
        .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .connection = gXCB.Connection,
        .window = gXCB.Window,
    };

    return gXCB.VkCreateXcbSurfaceKHR(
               (VkInstance)Instance,
               &Info,
               NULL,
               (VkSurfaceKHR *)Surface) == VK_SUCCESS;
}

static inline void Rr_ProcessXCBKeyEvent(
    xcb_key_press_event_t *XCBEvent,
    bool Down,
    Rr_Arena *Arena)
{
    Rr_Scancode Scancode = gXCB.XKBKeycodeToScancode[XCBEvent->detail];

    Rr_AddKeyEvent(Scancode, Down);

    if (Down)
    {
        uint32_t Codepoint =
            xkb_state_key_get_utf32(gXCB.XKBState, XCBEvent->detail);
        if (!Codepoint)
        {
            return;
        }

        Rr_AddTextInputEvent(Codepoint, Arena);
    }
}

static inline void Rr_ProcessXCBSelectionNotifyEvent(
    xcb_selection_notify_event_t *Event,
    Rr_Arena *Arena)
{
    if (Event->property != gXCB.Atoms.XdndSelection)
    {
        return;
    }

    xcb_get_property_reply_t *Reply = xcb_get_property_reply(
        gXCB.Connection,
        xcb_get_property(
            gXCB.Connection,
            0,
            Event->requestor,
            Event->property,
            gXCB.Atoms.TextURIList,
            0,
            UINT32_MAX),
        NULL);

    if (gXCB.XdndVersion >= 2)
    {
        xcb_client_message_event_t FinishedEvent = {
            .response_type = XCB_CLIENT_MESSAGE,
            .format = 32,
            .window = gXCB.XdndSource,
            .type = gXCB.Atoms.XdndFinished,
            .data.data32[0] = gXCB.Window,
            .data.data32[1] = Reply != NULL,
            .data.data32[2] = gXCB.Atoms.XdndActionCopy,
        };
        xcb_send_event(
            gXCB.Connection,
            false,
            gXCB.XdndSource,
            XCB_EVENT_MASK_NO_EVENT,
            (void *)&FinishedEvent);
        xcb_flush(gXCB.Connection);
    }

    if (Reply)
    {
        Rr_Scratch Scratch = Rr_GetScratch(Arena);

        size_t URIListLength = (size_t)xcb_get_property_value_length(Reply);
        char *URIList = RR_ALLOC_NO_ZERO(URIListLength + 1, Scratch.Arena);
        memcpy(URIList, xcb_get_property_value(Reply), URIListLength);
        URIList[URIListLength] = '\0';
        char const *Line = strtok(URIList, "\r\n");
        while (Line != NULL)
        {
            char const *Prefix = "file:///";
            if (strncmp(Line, Prefix, sizeof("file:///") - 1) == 0)
            {
                char const *Path = Line + sizeof("file:///") - 2;
                if (strlen(Path))
                {
                    Rr_AddDropFileEvent(Path);
                }
            }
            Line = strtok(NULL, "\r\n");
        }

        free(Reply);

        Rr_DestroyScratch(Scratch);
    }
}

static inline void Rr_ProcessXCBSelectionRequestEvent(
    xcb_selection_request_event_t *SelectionRequestEvent)
{
    xcb_selection_notify_event_t SelectionNotifyEvent = {
        .response_type = XCB_SELECTION_NOTIFY,
        .time = XCB_CURRENT_TIME,
        .requestor = SelectionRequestEvent->requestor,
        .selection = SelectionRequestEvent->selection,
        .target = SelectionRequestEvent->target,
        .property = SelectionRequestEvent->property
    };

    if (SelectionRequestEvent->property == XCB_NONE)
    {
        SelectionRequestEvent->property = SelectionRequestEvent->target;
    }

    if (SelectionRequestEvent->target == gXCB.Atoms.Targets)
    {
        xcb_change_property(
            gXCB.Connection,
            XCB_PROP_MODE_REPLACE,
            SelectionRequestEvent->requestor,
            SelectionRequestEvent->property,
            XCB_ATOM_ATOM,
            sizeof(xcb_atom_t) * 8,
            sizeof(xcb_atom_t) * RR_XCB_CLIPBOARD_TARGET_COUNT,
            gXCB.ClipboardTargets);
    }
    else
    {
        bool KnownTarget = false;
        for (size_t Index = 0; Index < RR_XCB_CLIPBOARD_TARGET_COUNT; ++Index)
        {
            if (gXCB.ClipboardTargets[Index] == SelectionRequestEvent->target)
            {
                KnownTarget = true;

                break;
            }
        }

        if (KnownTarget)
        {
            xcb_change_property(
                gXCB.Connection,
                XCB_PROP_MODE_REPLACE,
                SelectionRequestEvent->requestor,
                SelectionRequestEvent->property,
                SelectionRequestEvent->target,
                8,
                (uint32_t)gXCB.ClipboardLength,
                gXCB.Clipboard);
        }
        else
        {
            SelectionNotifyEvent.property = XCB_NONE;
        }
    }

    xcb_send_event(
        gXCB.Connection,
        0,
        SelectionRequestEvent->requestor,
        XCB_EVENT_MASK_PROPERTY_CHANGE,
        (void *)&SelectionNotifyEvent);
    xcb_flush(gXCB.Connection);
}

void Rr_ProcessPlatformEvents(Rr_Arena *Arena)
{
    xcb_connection_t *Connection = gXCB.Connection;

    xcb_generic_event_t *XCBEvent = NULL;
    while ((XCBEvent = xcb_poll_for_event(Connection)))
    {
        if (Rr_ProcessXKBEvent((xkb_generic_event_t *)XCBEvent))
        {
            free(XCBEvent);

            continue;
        }

        uint32_t XCBEventType = (uint32_t)(XCBEvent->response_type & ~0x80);
        switch (XCBEventType)
        {
            case XCB_CLIENT_MESSAGE:
            {
                xcb_client_message_event_t *MessageEvent =
                    (xcb_client_message_event_t *)XCBEvent;

                Rr_ProcessXdndEvent(MessageEvent);

                if (MessageEvent->data.data32[0] == gXCB.Atoms.WMDeleteWindow)
                {
                    Rr_AddQuitRequestedEvent();
                }
            }
            break;
            case XCB_FOCUS_IN:
            {
                Rr_AddFocusEvent(true);
            }
            break;
            case XCB_FOCUS_OUT:
            {
                Rr_AddFocusEvent(false);
            }
            break;
            case XCB_BUTTON_PRESS:
            {
                xcb_button_press_event_t *ButtonPressEvent =
                    (xcb_button_press_event_t *)XCBEvent;

                if (ButtonPressEvent->detail == 4)
                {
                    Rr_AddMouseWheelEvent(
                        gPlatform.MousePosition,
                        Rr_V2(0.0f, 1.0f));
                }
                else if (ButtonPressEvent->detail == 5)
                {
                    Rr_AddMouseWheelEvent(
                        gPlatform.MousePosition,
                        Rr_V2(0.0f, -1.0f));
                }
                else if (ButtonPressEvent->detail == 6)
                {
                    Rr_AddMouseWheelEvent(
                        gPlatform.MousePosition,
                        Rr_V2(-1.0f, 0.0f));
                }
                else if (ButtonPressEvent->detail == 7)
                {
                    Rr_AddMouseWheelEvent(
                        gPlatform.MousePosition,
                        Rr_V2(1.0f, 0.0f));
                }
                else
                {
                    Rr_MouseButton Button =
                        (Rr_MouseButton)(ButtonPressEvent->detail - 1);
                    if (ButtonPressEvent->detail > 7)
                    {
                        Button -= 4;
                    }

                    Rr_AddMouseButtonEvent(
                        true,
                        gPlatform.MousePosition,
                        Button);

                    gPlatform.MouseState |= (Rr_MouseButtonFlags)(1 << Button);
                }
            }
            break;
            case XCB_BUTTON_RELEASE:
            {
                xcb_button_release_event_t *ButtonReleaseEvent =
                    (xcb_button_release_event_t *)XCBEvent;

                Rr_MouseButton Button =
                    (Rr_MouseButton)(ButtonReleaseEvent->detail - 1);
                if (ButtonReleaseEvent->detail > 7)
                {
                    Button -= 4;
                }

                Rr_AddMouseButtonEvent(false, gPlatform.MousePosition, Button);

                gPlatform.MouseState &= (Rr_MouseButtonFlags) ~(1 << Button);
            }
            break;
            case XCB_MOTION_NOTIFY:
            {
                xcb_motion_notify_event_t *MotionEvent =
                    (xcb_motion_notify_event_t *)XCBEvent;

                Rr_Vec2 Position = {
                    (float)MotionEvent->event_x,
                    (float)MotionEvent->event_y,
                };

                if (gPlatform.RelativeMouseMode)
                {
                    Rr_Vec2 Delta = Rr_SubV2(
                        Position,
                        Rr_CastV2(gPlatform.RelativeMouseRestorePosition));

                    if ((int)Delta.X == 0 && (int)Delta.Y == 0)
                    {
                        break;
                    }

                    gPlatform.MousePosition =
                        Rr_AddV2(gPlatform.MousePosition, Delta);
                    gPlatform.MousePositionDelta =
                        Rr_AddV2(gPlatform.MousePositionDelta, Delta);

                    Rr_WarpXCBPointer(gPlatform.RelativeMouseRestorePosition);
                }
                else
                {
                    gPlatform.MousePosition = Position;
                }

                Rr_AddMouseMotionEvent(gPlatform.MousePosition);
            }
            break;
            case XCB_KEY_PRESS:
            {
                Rr_ProcessXCBKeyEvent(
                    (xcb_key_press_event_t *)XCBEvent,
                    true,
                    Arena);
            }
            break;
            case XCB_KEY_RELEASE:
            {
                Rr_ProcessXCBKeyEvent(
                    (xcb_key_press_event_t *)XCBEvent,
                    false,
                    Arena);
            }
            break;
            case XCB_CONFIGURE_NOTIFY:
            {
            }
            break;
            case XCB_SELECTION_NOTIFY:
            {
                Rr_ProcessXCBSelectionNotifyEvent(
                    (xcb_selection_notify_event_t *)XCBEvent,
                    Arena);
            }
            break;
            case XCB_SELECTION_CLEAR:
            {
                free(gXCB.Clipboard);
                gXCB.Clipboard = NULL;
                gXCB.ClipboardLength = 0;
            }
            break;
            case XCB_SELECTION_REQUEST:
            {
                Rr_ProcessXCBSelectionRequestEvent(
                    (xcb_selection_request_event_t *)XCBEvent);
            }
            break;
            default:
            {
            }
            break;
        }

        free(XCBEvent);
    }
}

void Rr_ShowWindow(void)
{
    xcb_map_window(gXCB.Connection, gXCB.Window);
    if (gXCB.FirstTimeShowFullscreen)
    {
        Rr_SetWindowFullscreen(true);
        gXCB.FirstTimeShowFullscreen = false;
    }
    xcb_flush(gXCB.Connection);
}

static inline bool Rr_GetWMState(xcb_atom_t StateAtom)
{
    xcb_atom_t WMState = Rr_GetXCBAtom("_NET_WM_STATE");

    xcb_get_property_reply_t *Reply = xcb_get_property_reply(
        gXCB.Connection,
        xcb_get_property(
            gXCB.Connection,
            0,
            gXCB.Window,
            WMState,
            XCB_ATOM_ATOM,
            0,
            1024),
        NULL);
    if (!Reply)
    {
        return false;
    }

    int AtomCount =
        xcb_get_property_value_length(Reply) / (int)sizeof(xcb_atom_t);
    xcb_atom_t *Atoms = (xcb_atom_t *)xcb_get_property_value(Reply);

    bool State = false;
    for (int i = 0; i < AtomCount; ++i)
    {
        if (Atoms[i] == StateAtom)
        {
            State = true;
            break;
        }
    }

    free(Reply);

    return State;
}

void Rr_SetRelativeMouseMode(bool Relative)
{
    if (gPlatform.RelativeMouseMode == Relative)
    {
        return;
    }

    if (Relative)
    {
        xcb_grab_pointer_reply_t *Reply = xcb_grab_pointer_reply(
            gXCB.Connection,
            xcb_grab_pointer(
                gXCB.Connection,
                false,
                gXCB.Window,
                XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                    XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_ENTER_WINDOW |
                    XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_POINTER_MOTION,
                XCB_GRAB_MODE_ASYNC,
                XCB_GRAB_MODE_ASYNC,
                gXCB.Window,
                gXCB.EmptyCursor,
                XCB_TIME_CURRENT_TIME),
            NULL);
        if (!Reply)
        {
            return;
        }

        if (Reply->status == XCB_GRAB_STATUS_SUCCESS)
        {
            gPlatform.MousePositionDelta = Rr_V2F(0.0f);
            gPlatform.RelativeMouseRestorePosition =
                Rr_CastIntV2(gPlatform.MousePosition);
        }

        free(Reply);
    }
    else
    {
        xcb_ungrab_pointer(gXCB.Connection, XCB_TIME_CURRENT_TIME);

        Rr_WarpXCBPointer(gPlatform.RelativeMouseRestorePosition);
    }

    gPlatform.RelativeMouseMode = Relative;
}

void Rr_SetCursor(Rr_CursorType Type)
{
    if (gPlatform.CursorType == Type)
    {
        return;
    }

    xcb_change_window_attributes(
        gXCB.Connection,
        gXCB.Window,
        XCB_CW_CURSOR,
        &gXCB.Cursors[Type]);

    gPlatform.CursorType = Type;
}

bool Rr_IsWindowMinimized(void)
{
    return Rr_GetWMState(Rr_GetXCBAtom("_NET_WM_STATE_HIDDEN"));
}

bool Rr_IsWindowFullscreen(void)
{
    return Rr_GetWMState(Rr_GetXCBAtom("_NET_WM_STATE_FULLSCREEN"));
}

void Rr_SetWindowFullscreen(bool Fullscreen)
{
    xcb_atom_t WMState = Rr_GetXCBAtom("_NET_WM_STATE");
    xcb_atom_t WMStateFullscreen = Rr_GetXCBAtom("_NET_WM_STATE_FULLSCREEN");

    if (WMState == XCB_NONE || WMStateFullscreen == XCB_NONE)
    {
        return;
    }

    xcb_client_message_event_t Event = {
        .response_type = XCB_CLIENT_MESSAGE,
        .window = gXCB.Window,
        .type = WMState,
        .format = 32,
        .data.data32[0] = Fullscreen ? 1 : 0,
        .data.data32[1] = WMStateFullscreen,
        .data.data32[2] = 0,
        .data.data32[3] = 1,
        .data.data32[4] = 0,
    };

    xcb_send_event(
        gXCB.Connection,
        0,
        xcb_setup_roots_iterator(gXCB.Setup).data->root,
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
            XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
        (void *)&Event);
    xcb_flush(gXCB.Connection);
}

static inline double Rr_GetXCBScreenRefreshRate(void)
{
    xcb_randr_get_screen_info_reply_t *Reply = xcb_randr_get_screen_info_reply(
        gXCB.Connection,
        xcb_randr_get_screen_info_unchecked(gXCB.Connection, gXCB.Window),
        NULL);

    double Result = (double)Reply->rate;

    free(Reply);

    return Result;
}

double Rr_GetDisplayRefreshRate(void)
{
    if (!gXCB.UseRandr)
    {
        return Rr_GetXCBScreenRefreshRate();
    }

    xcb_randr_get_output_primary_reply_t *OutputPrimaryReply =
        xcb_randr_get_output_primary_reply(
            gXCB.Connection,
            xcb_randr_get_output_primary(gXCB.Connection, gXCB.Screen->root),
            NULL);
    if (!OutputPrimaryReply)
    {
        return Rr_GetXCBScreenRefreshRate();
    }
    xcb_randr_output_t PrimaryOutput = OutputPrimaryReply->output;
    free(OutputPrimaryReply);

    xcb_randr_get_output_info_reply_t *OutputInfoReply =
        xcb_randr_get_output_info_reply(
            gXCB.Connection,
            xcb_randr_get_output_info(
                gXCB.Connection,
                PrimaryOutput,
                XCB_CURRENT_TIME),
            NULL);
    xcb_randr_crtc_t crtc = OutputInfoReply->crtc;
    free(OutputInfoReply);

    xcb_randr_get_crtc_info_reply_t *CRTCInfoReply =
        xcb_randr_get_crtc_info_reply(
            gXCB.Connection,
            xcb_randr_get_crtc_info(gXCB.Connection, crtc, 0),
            NULL);
    xcb_randr_mode_t Mode = CRTCInfoReply->mode;
    free(CRTCInfoReply);

    xcb_randr_get_screen_resources_current_reply_t
        *ScreenResourcesCurrentReply =
            xcb_randr_get_screen_resources_current_reply(
                gXCB.Connection,
                xcb_randr_get_screen_resources_current_unchecked(
                    gXCB.Connection,
                    gXCB.Window),
                NULL);
    size_t ModeInfoCount =
        (size_t)xcb_randr_get_screen_resources_current_modes_length(
            ScreenResourcesCurrentReply);
    xcb_randr_mode_info_t *ModeInfos =
        xcb_randr_get_screen_resources_current_modes(
            ScreenResourcesCurrentReply);
    for (size_t Index = 0; Index < ModeInfoCount; ++Index)
    {
        xcb_randr_mode_info_t *ModeInfo = ModeInfos + Index;
        if (ModeInfo->id == Mode)
        {
            double Result = (double)ModeInfo->dot_clock /
                            (double)(ModeInfo->htotal * ModeInfo->vtotal);

            free(ScreenResourcesCurrentReply);

            return Result;
        }
    }

    free(ScreenResourcesCurrentReply);

    return Rr_GetXCBScreenRefreshRate();
}

Rr_Vec2 Rr_QueryPlatformMousePosition(void)
{
    xcb_query_pointer_reply_t *Reply = xcb_query_pointer_reply(
        gXCB.Connection,
        xcb_query_pointer(gXCB.Connection, gXCB.Window),
        NULL);
    Rr_Vec2 Position = Rr_V2((float)Reply->win_x, (float)Reply->win_y);
    free(Reply);

    return Position;
}

Rr_IntVec2 Rr_GetWindowSize(void)
{
    xcb_get_geometry_reply_t *Reply = xcb_get_geometry_reply(
        gXCB.Connection,
        xcb_get_geometry(gXCB.Connection, gXCB.Window),
        NULL);
    Rr_IntVec2 Result = { Reply->width, Reply->height };
    free(Reply);

    return Result;
}

void Rr_SetWindowSize(Rr_IntVec2 Size)
{
    uint32_t Values[] = { (uint32_t)Size.X, (uint32_t)Size.Y };
    xcb_configure_window(
        gXCB.Connection,
        gXCB.Window,
        XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
        Values);
    xcb_flush(gXCB.Connection);
    Rr_ForceXCBWindowSize(Size);
}

void Rr_SetWindowTitle(const char *Title)
{
    xcb_change_property(
        gXCB.Connection,
        XCB_PROP_MODE_REPLACE,
        gXCB.Window,
        XCB_ATOM_WM_NAME,
        XCB_ATOM_STRING,
        8,
        (uint32_t)strlen(Title),
        Title);
}

Rr_IntVec2 Rr_GetDisplaySize(void)
{
    return Rr_IntV2(
        gXCB.Screen->width_in_pixels,
        gXCB.Screen->height_in_pixels);
}

float Rr_GetDisplayScale(void)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    float Scale = 1.0f;

    xcb_atom_t Atom = Rr_GetXCBAtom("RESOURCE_MANAGER");
    xcb_get_property_reply_t *Reply = xcb_get_property_reply(
        gXCB.Connection,
        xcb_get_property(
            gXCB.Connection,
            0,
            gXCB.Screen->root,
            Atom,
            XCB_ATOM_ANY,
            0,
            1024 * 8),
        NULL);
    if (Reply)
    {
        size_t Length = (size_t)xcb_get_property_value_length(Reply);
        char *Buffer = RR_ALLOC_NO_ZERO(Length + 1, Scratch.Arena);
        memcpy(Buffer, xcb_get_property_value(Reply), Length);
        Buffer[Length] = '\0';

        char *ValueStart = strstr(Buffer, "Xft.dpi:");
        if (ValueStart)
        {
            ValueStart += sizeof("Xft.dpi:");
            char *ValueEnd = ValueStart;

            while (*ValueEnd != '\0' && *ValueEnd != '\n')
            {
                ValueEnd++;
            }

            size_t ValueLength = (size_t)(ValueEnd - ValueStart);
            char *ValueString =
                RR_ALLOC_NO_ZERO(ValueLength + 1, Scratch.Arena);
            memcpy(ValueString, ValueStart, ValueLength);
            ValueString[ValueLength] = '\0';
            int DensityPerInch = 96;
            if (sscanf(ValueString, "%d", &DensityPerInch))
            {
                Scale = (float)DensityPerInch / 96.0f;
            }
        }

        free(Reply);
    }

    Rr_DestroyScratch(Scratch);

    return Scale;
}

void Rr_SetClipboardText(const char *CString)
{
    size_t Length = (size_t)strlen(CString);
    if (!Length)
    {
        free(gXCB.Clipboard);
        gXCB.Clipboard = NULL;

        return;
    }
    gXCB.ClipboardLength = Length;
    gXCB.Clipboard = realloc(gXCB.Clipboard, Length);
    memcpy(gXCB.Clipboard, CString, Length);

    xcb_set_selection_owner(
        gXCB.Connection,
        gXCB.Window,
        gXCB.Atoms.Clipboard,
        XCB_CURRENT_TIME);
    xcb_flush(gXCB.Connection);
}

char const *Rr_GetClipboardText(Rr_Arena *Arena)
{
    if (gXCB.Clipboard)
    {
        char *Buffer =
            RR_ALLOC_COPY(gXCB.Clipboard, gXCB.ClipboardLength, Arena);
        Buffer[gXCB.ClipboardLength] = '\0';

        return Buffer;
    }

    xcb_convert_selection(
        gXCB.Connection,
        gXCB.Window,
        gXCB.Atoms.Clipboard,
        gXCB.Atoms.UTF8String,
        gXCB.Atoms.Clipboard,
        XCB_CURRENT_TIME);
    xcb_flush(gXCB.Connection);

    for (uint64_t ToSleepNS = 1 << 7; ToSleepNS < (1 << 15); ToSleepNS <<= 1)
    {
        xcb_generic_event_t *Event = xcb_poll_for_event(gXCB.Connection);
        if (!Event)
        {
            Rr_SleepNS(ToSleepNS);

            continue;
        }
        int Type = Event->response_type & ~0x80;
        free(Event);
        if (Type == XCB_SELECTION_NOTIFY)
        {
            break;
        }
    }

    xcb_get_property_reply_t *Reply = xcb_get_property_reply(
        gXCB.Connection,
        xcb_get_property(
            gXCB.Connection,
            1,
            gXCB.Window,
            gXCB.Atoms.Clipboard,
            gXCB.Atoms.UTF8String,
            0,
            0),
        NULL);
    uint32_t BytesAfter = Reply->bytes_after;
    free(Reply);
    char *Buffer = RR_ALLOC_NO_ZERO(BytesAfter + 1, Arena);

    Reply = xcb_get_property_reply(
        gXCB.Connection,
        xcb_get_property(
            gXCB.Connection,
            1,
            gXCB.Window,
            gXCB.Atoms.Clipboard,
            gXCB.Atoms.UTF8String,
            0,
            (BytesAfter + 4) / sizeof(uint32_t)),
        NULL);

    if (xcb_get_property_value_length(Reply) > (int)BytesAfter)
    {
        free(Reply);

        return NULL;
    }

    memcpy(Buffer, xcb_get_property_value(Reply), BytesAfter);
    Buffer[BytesAfter] = '\0';

    free(Reply);

    return Buffer;
}
