#include <Rr/Rr_Platform.h>

#include "Rr_Log.h"

void Rr_LockSpinLock(Rr_SpinLock *SpinLock)
{
    int Loops = 0;
    const int MaxLoops = 1000000;
    while(!Rr_TryLockSpinLock(SpinLock))
    {
        if(Loops > MaxLoops)
        {
            Rr_LogAbort("Spin lock timeout!");
        }
    }
}
