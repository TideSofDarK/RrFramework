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

#include "Rr_App.h"
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
#include <xcb/xfixes.h>
#include <xcb/xkb.h>
#include <xkbcommon/xkbcommon-x11.h>

#include <vulkan/vulkan_xcb.h>

#include <ctype.h>
#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>

typedef enum
{
    RR_XCB_TARGET_UTF8_STRING,
    RR_XCB_TARGET_COMPOUND_TEXT,
    RR_XCB_TARGET_TEXT,
    RR_XCB_TARGET_STRING,
    RR_XCB_TARGET_TEXT_PLAIN_UTF8,
    RR_XCB_TARGET_TEXT_PLAIN,
    RR_XCB_TARGET_COUNT
} Rr_XCBTarget;

static const char *RR_XCB_TARGETS[RR_XCB_TARGET_COUNT] = {
    "UTF8_STRING",
    "COMPOUND_TEXT",
    "TEXT",
    "STRING",
    "text/plain;charset=utf-8",
    "text/plain"
};

static struct Rr_Platform_XCB
{
    bool Initialized;

    xcb_connection_t *Connection;
    const xcb_setup_t *Setup;
    xcb_screen_t *Screen;

    xcb_window_t Window;
    xcb_cursor_context_t *CursorContext;
    xcb_cursor_t Cursors[RR_CURSOR_TYPE_COUNT];
    xcb_cursor_t EmptyCursor;
    Rr_CursorType CursorType;
    bool CursorDisabled;
    struct
    {
        xcb_atom_t Targets;
        xcb_atom_t Clipboard;
        xcb_atom_t ClipboardRr;
        xcb_atom_t UTF8String;
        xcb_atom_t WMProtocols;
        xcb_atom_t WMDeleteWindow;
    } Atoms;
    xcb_atom_t Targets[RR_XCB_TARGET_COUNT];
    bool WindowScaled;
    Rr_Vec2 WindowScale;
    Rr_IntVec2 WindowedOffset;
    Rr_IntVec2 WindowedExtent;
    bool PointerGrabbed;
    Rr_Vec2 MousePosition;
    Rr_Vec2 MousePositionDelta;
    Rr_Vec2 RestoreMousePosition;
    Rr_MouseButtonFlags MouseState;

    struct xkb_context *XKBContext;
    struct xkb_keymap *XKBKeymap;
    struct xkb_state *XKBState;
    struct xkb_state *XKBStateNone;
    int32_t XKBDeviceID;
    Rr_Scancode XKBKeycodeToScancode[256];
    bool XKBPressedKeys[256];
    Rr_Scancode ScancodeToXKBKeycode[256];

    bool UseRandr;

    char *Clipboard;

    void *VulkanModule;
    PFN_vkGetInstanceProcAddr VkGetInstanceProcAddr;
    PFN_vkCreateXcbSurfaceKHR VkCreateXcbSurfaceKHR;
} gPlatform;

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
    xcb_intern_atom_cookie_t Cookie = xcb_intern_atom(
        gPlatform.Connection,
        false,
        (uint16_t)strlen(CString),
        CString);
    xcb_intern_atom_reply_t *Reply =
        xcb_intern_atom_reply(gPlatform.Connection, Cookie, NULL);
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
    if (gPlatform.XKBKeymap)
    {
        xkb_keymap_unref(gPlatform.XKBKeymap);
    }
    if (gPlatform.XKBState)
    {
        xkb_state_unref(gPlatform.XKBState);
    }
    if (gPlatform.XKBStateNone)
    {
        xkb_state_unref(gPlatform.XKBStateNone);
    }

    gPlatform.XKBKeymap = xkb_x11_keymap_new_from_device(
        gPlatform.XKBContext,
        gPlatform.Connection,
        gPlatform.XKBDeviceID,
        XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!gPlatform.XKBKeymap)
    {
        return false;
    }

    gPlatform.XKBState = xkb_x11_state_new_from_device(
        gPlatform.XKBKeymap,
        gPlatform.Connection,
        gPlatform.XKBDeviceID);
    if (!gPlatform.XKBState)
    {
        xkb_keymap_unref(gPlatform.XKBKeymap);
        gPlatform.XKBKeymap = NULL;

        return false;
    }

    gPlatform.XKBStateNone = xkb_x11_state_new_from_device(
        gPlatform.XKBKeymap,
        gPlatform.Connection,
        gPlatform.XKBDeviceID);
    if (!gPlatform.XKBStateNone)
    {
        xkb_keymap_unref(gPlatform.XKBKeymap);
        gPlatform.XKBKeymap = NULL;
        xkb_state_unref(gPlatform.XKBState);
        gPlatform.XKBState = NULL;

        return false;
    }
    xkb_layout_index_t Layout =
        xkb_state_key_get_layout(gPlatform.XKBStateNone, 0);
    xkb_state_update_mask(
        gPlatform.XKBStateNone,
        2,
        2,
        2,
        Layout,
        Layout,
        Layout);

    xcb_xkb_get_names_reply_t *GetNamesReply = xcb_xkb_get_names_reply(
        gPlatform.Connection,
        xcb_xkb_get_names(
            gPlatform.Connection,
            (xcb_xkb_device_spec_t)gPlatform.XKBDeviceID,
            XCB_XKB_NAME_DETAIL_KEY_NAMES | XCB_XKB_NAME_DETAIL_KEY_ALIASES |
                XCB_XKB_NAME_DETAIL_SYMBOLS),
        NULL);
    if (!GetNamesReply)
    {
        return false;
    }
    xcb_xkb_get_names_value_list_t Values = { 0 };
    void *NamesValueList = xcb_xkb_get_names_value_list(GetNamesReply);
    xcb_xkb_get_names_value_list_unpack(
        NamesValueList,
        GetNamesReply->nTypes,
        GetNamesReply->indicators,
        GetNamesReply->virtualMods,
        GetNamesReply->groupNames,
        GetNamesReply->nKeys,
        GetNamesReply->nKeyAliases,
        GetNamesReply->nRadioGroups,
        GetNamesReply->which,
        &Values);
    int KeyNameCount =
        xcb_xkb_get_names_value_list_key_names_length(GetNamesReply, &Values);
    xcb_xkb_key_name_iterator_t It =
        xcb_xkb_get_names_value_list_key_names_iterator(GetNamesReply, &Values);
    for (int Index = 0; Index < KeyNameCount; Index++)
    {
        xcb_xkb_key_name_t *KeyName = It.data;
        if (KeyName)
        {
            xcb_keycode_t Keycode =
                (xcb_keycode_t)(GetNamesReply->firstKey + Index);

            Rr_Scancode Scancode = Rr_XCBKeyNameToScancode(KeyName->name);
            gPlatform.XKBKeycodeToScancode[Keycode] = Scancode;
            gPlatform.ScancodeToXKBKeycode[Scancode] = Keycode;
        }

        xcb_xkb_key_name_next(&It);
    }

    free(GetNamesReply);

    return true;
}

static inline bool Rr_InitXKB(void)
{
    xcb_xkb_use_extension(
        gPlatform.Connection,
        XKB_X11_MIN_MAJOR_XKB_VERSION,
        XKB_X11_MIN_MINOR_XKB_VERSION);
    struct xkb_context *Context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!Context)
    {
        return false;
    }
    gPlatform.XKBContext = Context;

    int32_t DeviceID =
        xkb_x11_get_core_keyboard_device_id(gPlatform.Connection);
    if (DeviceID == -1)
    {
        return false;
    }
    gPlatform.XKBDeviceID = DeviceID;

    xcb_xkb_per_client_flags(
        gPlatform.Connection,
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
    xcb_void_cookie_t SelectEventsCookie = xcb_xkb_select_events_aux_checked(
        gPlatform.Connection,
        (xcb_xkb_device_spec_t)DeviceID,
        SelectedEvents,
        0,
        0,
        RequiredMapParts,
        RequiredMapParts,
        &SelectEventsDetails);
    xcb_generic_error_t *SelectEventsError =
        xcb_request_check(gPlatform.Connection, SelectEventsCookie);
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
    if (Event->deviceId != gPlatform.XKBDeviceID)
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
                if (Rr_UpdateXKBKeymap())
                {
                    return true;
                }
            }
        }
        break;
        case XCB_XKB_MAP_NOTIFY:
        {
            if (Rr_UpdateXKBKeymap())
            {
                return true;
            }
        }
        break;
        case XCB_XKB_STATE_NOTIFY:
        {
            xcb_xkb_state_notify_event_t *StateNotifyEvent =
                (xcb_xkb_state_notify_event_t *)Event;
            xkb_state_update_mask(
                gPlatform.XKBState,
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

static inline bool Rr_InitRandr(void)
{
    xcb_generic_error_t *Error;
    xcb_randr_query_version_cookie_t Cookie =
        xcb_randr_query_version(gPlatform.Connection, 1, 5);
    xcb_randr_query_version_reply(gPlatform.Connection, Cookie, &Error);
    if (Error)
    {
        free(Error);

        return false;
    }

    xcb_randr_select_input(gPlatform.Connection, gPlatform.Screen->root, true);

    return true;
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
    assert(!gPlatform.Initialized);

    xcb_generic_error_t *XCBError = NULL;

    int ScreenID;
    xcb_connection_t *Connection = xcb_connect(NULL, &ScreenID);
    if (!Connection)
    {
        return false;
    }
    gPlatform.Connection = Connection;

    xcb_setup_t const *Setup = xcb_get_setup(Connection);
    if (!Setup)
    {
        return false;
    }
    gPlatform.Setup = Setup;

    xcb_screen_t *Screen = NULL;
    xcb_screen_iterator_t ScreenIt = xcb_setup_roots_iterator(Setup);
    for (; ScreenIt.rem; --ScreenID, xcb_screen_next(&ScreenIt))
    {
        if (ScreenID == 0)
        {
            Screen = ScreenIt.data;
            break;
        }
    }
    if (!Screen)
    {
        return false;
    }
    gPlatform.Screen = Screen;

    Rr_IntVec2 WindowExtent = {
        (int32_t)((float)Screen->width_in_pixels * RR_WINDOWED_RATIO),
        (int32_t)((float)Screen->height_in_pixels * RR_WINDOWED_RATIO)
    };
    Rr_IntVec2 WindowOffset = {
        Screen->width_in_pixels / 2 - WindowExtent.X / 2,
        Screen->height_in_pixels / 2 - WindowExtent.Y / 2
    };

    uint32_t ValueMask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t ValueList[] = {
        Screen->black_pixel,
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
            XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_EXPOSURE |
            XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE
    };

    uint32_t Window = xcb_generate_id(Connection);
    xcb_void_cookie_t CreateWindowCookie = xcb_create_window_checked(
        Connection,
        XCB_COPY_FROM_PARENT,
        Window,
        Screen->root,
        (int16_t)WindowOffset.X,
        (int16_t)WindowOffset.Y,
        (uint16_t)WindowExtent.X,
        (uint16_t)WindowExtent.Y,
        0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        Screen->root_visual,
        ValueMask,
        ValueList);
    XCBError = xcb_request_check(Connection, CreateWindowCookie);
    if (XCBError)
    {
        RR_LOG_ERROR("Failed to create XCB window!");

        free(XCBError);

        return false;
    }
    gPlatform.Window = Window;

    if (Rr_InitRandr())
    {
        gPlatform.UseRandr = true;
    }

    gPlatform.Atoms.Targets = Rr_GetXCBAtom("TARGETS");
    gPlatform.Atoms.Clipboard = Rr_GetXCBAtom("CLIPBOARD");
    gPlatform.Atoms.ClipboardRr = Rr_GetXCBAtom("CLIPBOARD_RR");
    gPlatform.Atoms.UTF8String = Rr_GetXCBAtom("UTF8_STRING");
    gPlatform.Atoms.WMProtocols = Rr_GetXCBAtom("WM_PROTOCOLS");
    gPlatform.Atoms.WMDeleteWindow = Rr_GetXCBAtom("WM_DELETE_WINDOW");
    xcb_change_property(
        gPlatform.Connection,
        XCB_PROP_MODE_REPLACE,
        gPlatform.Window,
        gPlatform.Atoms.WMProtocols,
        4,
        32,
        1,
        &gPlatform.Atoms.WMDeleteWindow);

    for (size_t Index = 0; Index < RR_XCB_TARGET_COUNT; ++Index)
    {
        gPlatform.Targets[Index] = Rr_GetXCBAtom(RR_XCB_TARGETS[Index]);
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
    gPlatform.CursorContext = CursorContext;
    gPlatform.Cursors[RR_CURSOR_TYPE_NORMAL] =
        xcb_cursor_load_cursor(CursorContext, "default");
    gPlatform.Cursors[RR_CURSOR_TYPE_RESIZE_EW] =
        xcb_cursor_load_cursor(CursorContext, "ew-resize");
    gPlatform.Cursors[RR_CURSOR_TYPE_RESIZE_NS] =
        xcb_cursor_load_cursor(CursorContext, "ns-resize");
    gPlatform.Cursors[RR_CURSOR_TYPE_RESIZE_NWSE] =
        xcb_cursor_load_cursor(CursorContext, "nwse-resize");
    gPlatform.Cursors[RR_CURSOR_TYPE_RESIZE_NESW] =
        xcb_cursor_load_cursor(CursorContext, "nesw-resize");
    gPlatform.Cursors[RR_CURSOR_TYPE_RESIZE_ALL] =
        xcb_cursor_load_cursor(CursorContext, "all-scroll");
    gPlatform.Cursors[RR_CURSOR_TYPE_TEXT] =
        xcb_cursor_load_cursor(CursorContext, "text");
    xcb_pixmap_t Pixmap = xcb_generate_id(Connection);
    gPlatform.EmptyCursor = xcb_generate_id(Connection);
    xcb_create_pixmap(Connection, 1, Pixmap, Screen->root, 1, 1);
    xcb_create_cursor(
        Connection,
        gPlatform.EmptyCursor,
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

    xcb_map_window(Connection, Window);
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
    gPlatform.VulkanModule = Module;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    gPlatform.VkGetInstanceProcAddr =
        (PFN_vkGetInstanceProcAddr)dlsym(Module, "vkGetInstanceProcAddr");
    if (!gPlatform.VkGetInstanceProcAddr)
    {
        return false;
    }
    gPlatform.VkCreateXcbSurfaceKHR = (PFN_vkCreateXcbSurfaceKHR)dlsym(
        gPlatform.VulkanModule,
        "vkCreateXcbSurfaceKHR");
    if (!gPlatform.VkCreateXcbSurfaceKHR)
    {
        return false;
    }
#pragma GCC diagnostic pop // Restore diagnostics state

    gPlatform.Initialized = true;

    return true;
}

void Rr_CleanupPlatform(void)
{
    assert(gPlatform.Initialized);

    for (int Index = 0; Index < RR_CURSOR_TYPE_COUNT; ++Index)
    {
        xcb_free_cursor(gPlatform.Connection, gPlatform.Cursors[Index]);
    }
    xcb_cursor_context_free(gPlatform.CursorContext);

    xkb_state_unref(gPlatform.XKBState);
    xkb_keymap_unref(gPlatform.XKBKeymap);
    xkb_context_unref(gPlatform.XKBContext);

    xcb_disconnect(gPlatform.Connection);

    dlclose(gPlatform.VulkanModule);

    RR_ZERO(gPlatform);
}

void (*Rr_GetVkGetInstanceProcAddr(void))(void)
{
    return (void (*)(void))gPlatform.VkGetInstanceProcAddr;
}

const char *const *Rr_GetVulkanExtensions(uint32_t *Count)
{
    static char const *Extensions[] = { "VK_KHR_surface",
                                        "VK_KHR_xcb_surface" };
    *Count = RR_ARRAY_COUNT(Extensions);
    return Extensions;
}

bool Rr_CreateVulkanSurface(uint64_t Instance, uint64_t *Surface)
{
    VkXcbSurfaceCreateInfoKHR Info = {
        .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .connection = gPlatform.Connection,
        .window = gPlatform.Window,
    };
    VkResult Result = gPlatform.VkCreateXcbSurfaceKHR(
        (VkInstance)Instance,
        &Info,
        NULL,
        (VkSurfaceKHR *)Surface);

    return Result == VK_SUCCESS;
}

bool Rr_IsScancodePressed(Rr_Scancode Scancode)
{
    return gPlatform.XKBPressedKeys[gPlatform.ScancodeToXKBKeycode[Scancode]];
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

void Rr_NewPlatformFrame(void)
{
    gPlatform.MousePositionDelta = Rr_V2F(0.0f);
}

static inline void Rr_ProcessXCBKeyEvent(
    xcb_key_press_event_t *XCBEvent,
    bool Press,
    Rr_Event *Event,
    Rr_Arena *Arena)
{
    if (Press && gPlatform.XKBPressedKeys[XCBEvent->detail])
    {
        Event->Type = RR_EVENT_TYPE_KEY_REPEAT;
    }
    else
    {
        Event->Type = Press ? RR_EVENT_TYPE_KEY_DOWN : RR_EVENT_TYPE_KEY_UP;
        gPlatform.XKBPressedKeys[XCBEvent->detail] = Press;
    }
    Event->Key.Down = Event->Type != RR_EVENT_TYPE_KEY_UP;
    Event->Key.Scancode = gPlatform.XKBKeycodeToScancode[XCBEvent->detail];
    Event->Key.Keymod = 0;
    if (RR_HAS_BIT(XCBEvent->state, XCB_MOD_MASK_SHIFT))
    {
        Event->Key.Keymod |= RR_KEYMOD_SHIFT;
    }
    if (RR_HAS_BIT(XCBEvent->state, XCB_MOD_MASK_CONTROL))
    {
        Event->Key.Keymod |= RR_KEYMOD_CTRL;
    }
    if (RR_HAS_BIT(XCBEvent->state, XCB_MOD_MASK_1))
    {
        Event->Key.Keymod |= RR_KEYMOD_ALT;
    }

    if (Event->Key.Down)
    {
        uint32_t Codepoint =
            xkb_state_key_get_utf32(gPlatform.XKBState, XCBEvent->detail);
        if (!Codepoint)
        {
            return;
        }
        int SignedCodepoint;
        memcpy(
            &SignedCodepoint,
            &Codepoint,
            RR_MIN(sizeof(SignedCodepoint), sizeof(Codepoint)));
        if (iscntrl(SignedCodepoint))
        {
            return;
        }

        char *Buffer = RR_ALLOC_NO_ZERO(5, Arena);
        Rr_CodepointToUTF8(Codepoint, Buffer);

        Rr_Event *InputEvent = Rr_AddEvent();
        InputEvent->Type = RR_EVENT_TYPE_TEXT_INPUT;
        InputEvent->Text.CString = Buffer;
        InputEvent->Text.Length = strlen(Buffer);
    }
}

static inline void Rr_ProcessXCBSelectionRequestEvent(
    xcb_selection_request_event_t *XCBEvent)
{
    if (XCBEvent->target == gPlatform.Atoms.Targets)
    {
        xcb_change_property(
            gPlatform.Connection,
            XCB_PROP_MODE_REPLACE,
            XCBEvent->requestor,
            XCBEvent->property,
            XCB_ATOM_ATOM,
            sizeof(xcb_atom_t) * 8,
            sizeof(xcb_atom_t) * RR_XCB_TARGET_COUNT,
            gPlatform.Targets);
    }
    else
    {
        bool KnownTarget = false;
        for (int i = 0; i < RR_XCB_TARGET_COUNT; ++i)
        {
            if (gPlatform.Targets[i] == XCBEvent->target)
            {
                KnownTarget = true;

                break;
            }
        }
        if (KnownTarget)
        {
            xcb_change_property(
                gPlatform.Connection,
                XCB_PROP_MODE_REPLACE,
                XCBEvent->requestor,
                XCBEvent->property,
                XCBEvent->target,
                8,
                (uint32_t)strlen(gPlatform.Clipboard),
                gPlatform.Clipboard);
        }
    }

    xcb_selection_notify_event_t SelectionNotifyEvent = {
        .response_type = XCB_SELECTION_NOTIFY,
        .time = XCB_CURRENT_TIME,
        .requestor = XCBEvent->requestor,
        .selection = XCBEvent->selection,
        .target = XCBEvent->target,
        .property = XCBEvent->property,
    };
    xcb_generic_error_t *Error = xcb_request_check(
        gPlatform.Connection,
        xcb_send_event(
            gPlatform.Connection,
            false,
            XCBEvent->requestor,
            XCB_EVENT_MASK_PROPERTY_CHANGE,
            (const char *)&SelectionNotifyEvent));
    if (Error)
    {
        free(Error);
    }
}

bool Rr_PollPlatformEvent(Rr_Event *Event, Rr_Arena *Arena)
{
    xcb_connection_t *Connection = gPlatform.Connection;

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

                if (MessageEvent->data.data32[0] ==
                    gPlatform.Atoms.WMDeleteWindow)
                {
                    Event->Type = RR_EVENT_TYPE_QUIT;

                    goto Translated;
                }
            }
            break;
            case XCB_EXPOSE:
            {
                free(XCBEvent);
            }
            break;
            case XCB_BUTTON_PRESS:
            {
                xcb_button_press_event_t *ButtonPressEvent =
                    (xcb_button_press_event_t *)XCBEvent;

                Event->Type = RR_EVENT_TYPE_MOUSE_BUTTON_DOWN;
                Event->MouseButton.Clicks = 1;
                Event->MouseButton.Button =
                    (uint8_t)(ButtonPressEvent->detail - 1);
                Event->MouseButton.Position = (Rr_Vec2){
                    (float)ButtonPressEvent->event_x,
                    (float)ButtonPressEvent->event_y,
                };

                gPlatform.MouseState |=
                    (Rr_MouseButtonFlags)(1 << Event->MouseButton.Button);

                goto Translated;
            }
            case XCB_BUTTON_RELEASE:
            {
                xcb_button_release_event_t *ButtonReleaseEvent =
                    (xcb_button_release_event_t *)XCBEvent;

                Event->Type = RR_EVENT_TYPE_MOUSE_BUTTON_UP;
                Event->MouseButton.Clicks = 1;
                Event->MouseButton.Button =
                    (uint8_t)(ButtonReleaseEvent->detail - 1);
                Event->MouseButton.Position = (Rr_Vec2){
                    (float)ButtonReleaseEvent->event_x,
                    (float)ButtonReleaseEvent->event_y,
                };

                gPlatform.MouseState &=
                    (Rr_MouseButtonFlags) ~(1 << Event->MouseButton.Button);

                goto Translated;
            }
            case XCB_MOTION_NOTIFY:
            {
                xcb_motion_notify_event_t *MotionEvent =
                    (xcb_motion_notify_event_t *)XCBEvent;

                Event->Type = RR_EVENT_TYPE_MOUSE_MOTION;
                Event->MouseMotion.Position = (Rr_Vec2){
                    (float)MotionEvent->event_x,
                    (float)MotionEvent->event_y,
                };

                gPlatform.MousePositionDelta = Rr_AddV2(
                    gPlatform.MousePositionDelta,
                    Rr_SubV2(
                        Event->MouseMotion.Position,
                        gPlatform.MousePosition));

                gPlatform.MousePosition = Event->MouseMotion.Position;

                goto Translated;
            }
            case XCB_ENTER_NOTIFY:
            {
                xcb_enter_notify_event_t *EnterEvent =
                    (xcb_enter_notify_event_t *)XCBEvent;

                free(XCBEvent);
            }
            break;
            case XCB_LEAVE_NOTIFY:
            {
                xcb_leave_notify_event_t *LeaveEvent =
                    (xcb_leave_notify_event_t *)XCBEvent;

                free(XCBEvent);
            }
            break;
            case XCB_KEY_PRESS:
            {
                Rr_ProcessXCBKeyEvent(
                    (xcb_key_press_event_t *)XCBEvent,
                    true,
                    Event,
                    Arena);

                goto Translated;
            }
            case XCB_KEY_RELEASE:
            {
                Rr_ProcessXCBKeyEvent(
                    (xcb_key_press_event_t *)XCBEvent,
                    false,
                    Event,
                    Arena);

                goto Translated;
            }
            case XCB_CONFIGURE_NOTIFY:
            {
                free(XCBEvent);
            }
            break;
            case XCB_SELECTION_NOTIFY:
            {
                RR_LOG_INFO("SEL NOTIFY");

                free(XCBEvent);
            }
            break;
            // case XCB_SELECTION_CLEAR:
            // {
            //     if (gPlatform.Clipboard)
            //     {
            //         free(gPlatform.Clipboard);
            //         gPlatform.Clipboard = NULL;
            //     }

            //     free(XCBEvent);
            // }
            // break;
            case XCB_SELECTION_REQUEST:
            {
                RR_LOG_INFO("SEL REQ");

                Rr_ProcessXCBSelectionRequestEvent(
                    (xcb_selection_request_event_t *)XCBEvent);

                free(XCBEvent);
            }
            break;
            default:
            {
                free(XCBEvent);
            }
            break;
        }
    }

    // xcb_aux_sync(gPlatform.Connection);
    // xcb_flush(gPlatform.Connection);

    return false;

Translated:

    free(XCBEvent);

    return true;
}

void Rr_ShowWindow(void)
{
}

bool Rr_IsWindowMinimized(void)
{
    return false;
}

bool Rr_IsWindowFullscreen(void)
{
    xcb_atom_t WMState = Rr_GetXCBAtom("_NET_WM_STATE");
    xcb_atom_t WMStateFullscreen = Rr_GetXCBAtom("_NET_WM_STATE_FULLSCREEN");

    xcb_get_property_cookie_t GetPropertyCookie = xcb_get_property(
        gPlatform.Connection,
        0,
        gPlatform.Window,
        WMState,
        XCB_ATOM_ATOM,
        0,
        1024);

    xcb_get_property_reply_t *GetPropertyReply =
        xcb_get_property_reply(gPlatform.Connection, GetPropertyCookie, NULL);
    if (!GetPropertyReply)
    {
        return false;
    }

    int AtomCount = xcb_get_property_value_length(GetPropertyReply) /
                    (int)sizeof(xcb_atom_t);
    xcb_atom_t *Atoms = (xcb_atom_t *)xcb_get_property_value(GetPropertyReply);

    bool IsFullscreen = false;
    for (int i = 0; i < AtomCount; ++i)
    {
        if (Atoms[i] == WMStateFullscreen)
        {
            IsFullscreen = true;
            break;
        }
    }

    free(GetPropertyReply);

    return IsFullscreen;
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
        .window = gPlatform.Window,
        .type = WMState,
        .format = 32,
        .data.data32[0] = Fullscreen ? 1 : 0,
        .data.data32[1] = WMStateFullscreen,
        .data.data32[2] = 0,
        .data.data32[3] = 1,
        .data.data32[4] = 0,
    };

    xcb_send_event(
        gPlatform.Connection,
        0,
        xcb_setup_roots_iterator(gPlatform.Setup).data->root,
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
            XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
        (void *)&Event);
    xcb_flush(gPlatform.Connection);
}

static inline void Rr_GrabPointer(void)
{
    if (gPlatform.PointerGrabbed)
    {
        return;
    }

    xcb_grab_pointer_cookie_t GrabPointerCookie = xcb_grab_pointer(
        gPlatform.Connection,
        false,
        gPlatform.Window,
        XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_ENTER_WINDOW |
            XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_POINTER_MOTION,
        XCB_GRAB_MODE_ASYNC,
        XCB_GRAB_MODE_ASYNC,
        gPlatform.Window,
        gPlatform.EmptyCursor,
        XCB_TIME_CURRENT_TIME);
    xcb_grab_pointer_reply_t *GrabPointerReply =
        xcb_grab_pointer_reply(gPlatform.Connection, GrabPointerCookie, NULL);
    if (!GrabPointerReply)
    {
        return;
    }

    if (GrabPointerReply->status == XCB_GRAB_STATUS_SUCCESS)
    {
        gPlatform.RestoreMousePosition = gPlatform.MousePosition;
        gPlatform.PointerGrabbed = true;
    }

    free(GrabPointerReply);
}

static inline void Rr_UngrabPointer(void)
{
    if (!gPlatform.PointerGrabbed)
    {
        return;
    }

    xcb_ungrab_pointer(gPlatform.Connection, XCB_TIME_CURRENT_TIME);
    xcb_warp_pointer(
        gPlatform.Connection,
        XCB_NONE,
        gPlatform.Window,
        0,
        0,
        0,
        0,
        (int16_t)gPlatform.RestoreMousePosition.X,
        (int16_t)gPlatform.RestoreMousePosition.Y);

    gPlatform.PointerGrabbed = false;
}

void Rr_SetRelativeMouseMode(bool Relative)
{
    if (Relative)
    {
        Rr_GrabPointer();
    }
    else
    {
        Rr_UngrabPointer();
    }
}

static inline float Rr_GetXCBScreenRefreshRate(void)
{
    xcb_randr_get_screen_info_cookie_t GetScreenInfoCookie =
        xcb_randr_get_screen_info_unchecked(
            gPlatform.Connection,
            gPlatform.Window);
    xcb_randr_get_screen_info_reply_t *GetScreenInfoReply =
        xcb_randr_get_screen_info_reply(
            gPlatform.Connection,
            GetScreenInfoCookie,
            NULL);

    float Result = (float)GetScreenInfoReply->rate;

    free(GetScreenInfoReply);

    return Result;
}

float Rr_GetDisplayRefreshRate(void)
{
    if (!gPlatform.UseRandr)
    {
        return Rr_GetXCBScreenRefreshRate();
    }

    xcb_randr_get_output_primary_cookie_t GetOutputPrimaryCookie =
        xcb_randr_get_output_primary(
            gPlatform.Connection,
            gPlatform.Screen->root);
    xcb_randr_get_output_primary_reply_t *GetOutputPrimaryReply =
        xcb_randr_get_output_primary_reply(
            gPlatform.Connection,
            GetOutputPrimaryCookie,
            NULL);
    if (!GetOutputPrimaryReply)
    {
        return Rr_GetXCBScreenRefreshRate();
    }
    xcb_randr_output_t PrimaryOutput = GetOutputPrimaryReply->output;
    free(GetOutputPrimaryReply);

    xcb_randr_get_output_info_cookie_t GetPutputInfoCookie =
        xcb_randr_get_output_info(
            gPlatform.Connection,
            PrimaryOutput,
            XCB_CURRENT_TIME);
    xcb_randr_get_output_info_reply_t *GetOutputInfoReply =
        xcb_randr_get_output_info_reply(
            gPlatform.Connection,
            GetPutputInfoCookie,
            NULL);
    xcb_randr_crtc_t crtc = GetOutputInfoReply->crtc;
    free(GetOutputInfoReply);

    xcb_randr_get_crtc_info_cookie_t GetCRTCInfoCookie =
        xcb_randr_get_crtc_info(gPlatform.Connection, crtc, 0);
    xcb_randr_get_crtc_info_reply_t *GetCRTCInfoReply =
        xcb_randr_get_crtc_info_reply(
            gPlatform.Connection,
            GetCRTCInfoCookie,
            NULL);
    xcb_randr_mode_t Mode = GetCRTCInfoReply->mode;
    free(GetCRTCInfoReply);

    xcb_randr_get_screen_resources_current_cookie_t
        GetScreenResourceCurrentCookie =
            xcb_randr_get_screen_resources_current_unchecked(
                gPlatform.Connection,
                gPlatform.Window);
    xcb_randr_get_screen_resources_current_reply_t
        *GetScreenResourcesCurrentReply =
            xcb_randr_get_screen_resources_current_reply(
                gPlatform.Connection,
                GetScreenResourceCurrentCookie,
                NULL);
    size_t ModeInfoCount =
        (size_t)xcb_randr_get_screen_resources_current_modes_length(
            GetScreenResourcesCurrentReply);
    xcb_randr_mode_info_t *ModeInfos =
        xcb_randr_get_screen_resources_current_modes(
            GetScreenResourcesCurrentReply);
    for (size_t Index = 0; Index < ModeInfoCount; ++Index)
    {
        xcb_randr_mode_info_t *ModeInfo = ModeInfos + Index;
        if (ModeInfo->id == Mode)
        {
            float Result = (float)ModeInfo->dot_clock /
                           (float)(ModeInfo->htotal * ModeInfo->vtotal);

            free(GetScreenResourcesCurrentReply);

            return Result;
        }
    }

    free(GetScreenResourcesCurrentReply);

    return Rr_GetXCBScreenRefreshRate();
}

Rr_IntVec2 Rr_GetWindowSize(void)
{
    xcb_get_geometry_reply_t *Geometry = xcb_get_geometry_reply(
        gPlatform.Connection,
        xcb_get_geometry(gPlatform.Connection, gPlatform.Window),
        NULL);
    Rr_IntVec2 Result = { Geometry->width, Geometry->height };
    free(Geometry);

    return Result;
}

void Rr_SetWindowTitle(const char *Title)
{
    xcb_change_property(
        gPlatform.Connection,
        XCB_PROP_MODE_REPLACE,
        gPlatform.Window,
        XCB_ATOM_WM_NAME,
        XCB_ATOM_STRING,
        8,
        (uint32_t)strlen(Title),
        Title);
}

Rr_IntVec2 Rr_GetDisplaySize(void)
{
    return Rr_IntV2(
        gPlatform.Screen->width_in_pixels,
        gPlatform.Screen->height_in_pixels);
}

float Rr_GetWindowContentsScale(void)
{
    return 1.0f;
}

void Rr_SetWindowSize(Rr_IntVec2 Size)
{
    uint32_t Values[] = { (uint32_t)Size.X, (uint32_t)Size.Y };
    xcb_configure_window(
        gPlatform.Connection,
        gPlatform.Window,
        XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
        Values);
    xcb_flush(gPlatform.Connection);
}

void Rr_SetCursor(Rr_CursorType Type)
{
    if (gPlatform.CursorType != Type)
    {
        xcb_change_window_attributes(
            gPlatform.Connection,
            gPlatform.Window,
            XCB_CW_CURSOR,
            &gPlatform.Cursors[Type]);
        gPlatform.CursorType = Type;
    }
}

void Rr_SetClipboardText(const char *CString)
{
    size_t Length = (size_t)strlen(CString);
    gPlatform.Clipboard = realloc(gPlatform.Clipboard, Length + 1);
    strcpy(gPlatform.Clipboard, CString);

    xcb_generic_error_t *Error;

    xcb_set_selection_owner(
        gPlatform.Connection,
        gPlatform.Window,
        XCB_ATOM_PRIMARY,
        XCB_CURRENT_TIME);
    xcb_xfixes_select_selection_input(
        gPlatform.Connection,
        gPlatform.Window,
        XCB_ATOM_PRIMARY,
        XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER);
    xcb_discard_reply(
        gPlatform.Connection,
        xcb_get_selection_owner_reply(
            gPlatform.Connection,
            xcb_get_selection_owner(gPlatform.Connection, XCB_ATOM_PRIMARY),
            &Error)
            ->sequence);
    if (Error)
    {
        free(Error);

        return;
    }

    xcb_set_selection_owner(
        gPlatform.Connection,
        gPlatform.Window,
        gPlatform.Atoms.Clipboard,
        XCB_CURRENT_TIME);
    xcb_xfixes_select_selection_input(
        gPlatform.Connection,
        gPlatform.Window,
        gPlatform.Atoms.Clipboard,
        XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER);
    xcb_discard_reply(
        gPlatform.Connection,
        xcb_get_selection_owner_reply(
            gPlatform.Connection,
            xcb_get_selection_owner(
                gPlatform.Connection,
                gPlatform.Atoms.Clipboard),
            &Error)
            ->sequence);
    if (Error)
    {
        free(Error);

        return;
    }

    xcb_flush(gPlatform.Connection);
}

const char *Rr_GetClipboardText(void)
{
    xcb_convert_selection(
        gPlatform.Connection,
        gPlatform.Window,
        XCB_ATOM_PRIMARY,
        gPlatform.Atoms.UTF8String,
        gPlatform.Atoms.ClipboardRr,
        XCB_CURRENT_TIME);
    xcb_flush(gPlatform.Connection);
    free(xcb_wait_for_event(gPlatform.Connection));

    xcb_get_property_cookie_t GetPropertyCookie = xcb_get_property(
        gPlatform.Connection,
        0,
        gPlatform.Window,
        gPlatform.Atoms.ClipboardRr,
        gPlatform.Atoms.UTF8String,
        0,
        UINT_MAX / 4);
    xcb_get_property_reply_t *GetPropertyReply =
        xcb_get_property_reply(gPlatform.Connection, GetPropertyCookie, NULL);
    if (GetPropertyReply != NULL)
    {
        size_t Length = (size_t)xcb_get_property_value_length(GetPropertyReply);
        gPlatform.Clipboard = realloc(gPlatform.Clipboard, Length + 1);
        memcpy(
            gPlatform.Clipboard,
            xcb_get_property_value(GetPropertyReply),
            Length);
        gPlatform.Clipboard[Length] = '\0';
        free(GetPropertyReply);
    }
    xcb_delete_property(
        gPlatform.Connection,
        gPlatform.Window,
        gPlatform.Atoms.ClipboardRr);

    return gPlatform.Clipboard;
}
