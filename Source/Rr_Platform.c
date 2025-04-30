#include <Rr/Rr_Platform.h>

#include "Rr_Log.h"

#include <Rr/Rr_Input.h>

#include <SDL3/SDL.h>

void Rr_LockSpinlock(Rr_Spinlock *SpinLock)
{
    int Loops = 0;
    const int MaxLoops = 1000000;
    while(!Rr_TryLockSpinlock(SpinLock))
    {
        if(Loops > MaxLoops)
        {
            RR_ABORT("Spin lock timeout!");
        }
    }
}

bool Rr_PollEvent(Rr_Event *Event)
{
    static SDL_Event SDLEvent;
    if(SDL_PollEvent(&SDLEvent) == false)
    {
        return false;
    }

    switch(SDLEvent.type)
    {
        case SDL_EVENT_MOUSE_MOTION:
        {
            Event->Type = RR_EVENT_TYPE_MOUSE_MOTION;
            Event->MouseMotion.Position =
                (Rr_Vec2){ SDLEvent.motion.x, SDLEvent.motion.y };
            Event->MouseMotion.Delta =
                (Rr_Vec2){ SDLEvent.motion.xrel, SDLEvent.motion.yrel };
            return true;
        }
        case SDL_EVENT_MOUSE_WHEEL:
        {
            Event->Type = RR_EVENT_TYPE_MOUSE_WHEEL;
            Event->MouseMotion.Position =
                (Rr_Vec2){ SDLEvent.wheel.mouse_x, SDLEvent.wheel.mouse_y };
            Event->MouseMotion.Delta =
                (Rr_Vec2){ SDLEvent.wheel.x, SDLEvent.wheel.y };
            return true;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            Event->Type = SDLEvent.type == SDL_EVENT_MOUSE_BUTTON_UP
                              ? RR_EVENT_TYPE_MOUSE_BUTTON_UP
                              : RR_EVENT_TYPE_MOUSE_BUTTON_DOWN;
            Event->MouseButton.Position =
                (Rr_Vec2){ SDLEvent.button.x, SDLEvent.button.y };
            if(SDLEvent.button.button == SDL_BUTTON_LEFT)
            {
                Event->MouseButton.Button = RR_MOUSE_BUTTON_LEFT;
            }
            else if(SDLEvent.button.button == SDL_BUTTON_RIGHT)
            {
                Event->MouseButton.Button = RR_MOUSE_BUTTON_RIGHT;
            }
            else if(SDLEvent.button.button == SDL_BUTTON_MIDDLE)
            {
                Event->MouseButton.Button = RR_MOUSE_BUTTON_MIDDLE;
            }
            else if(SDLEvent.button.button == SDL_BUTTON_X1)
            {
                Event->MouseButton.Button = RR_MOUSE_BUTTON_X1;
            }
            else if(SDLEvent.button.button == SDL_BUTTON_X2)
            {
                Event->MouseButton.Button = RR_MOUSE_BUTTON_X2;
            }
            Event->MouseButton.Clicks = SDLEvent.button.clicks;
            return true;
        }
        case SDL_EVENT_DROP_FILE:
        {
            Event->Type = RR_EVENT_TYPE_DROP_FILE;
            Event->DropFile.Path = SDLEvent.drop.data;
            return true;
        }
        case SDL_EVENT_QUIT:
        {
            Event->Type = RR_EVENT_TYPE_QUIT;
            return true;
        }
        default:
            return false;
    }
}
