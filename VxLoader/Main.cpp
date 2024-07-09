
#include <windows.h>
#include "MMap.h"
#include <thread>

bool DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
    
        if (MMapWithPath(((wchar_t*)_(L"C:\\Users\\conspiracy\\Vixen\\VxnInjector\\UD\\Vxn.dll"))) == 0x69)
        {
            return false;
        }
        else 
        {
		// u can legitimately just unload here ig
        }
    }

    return true;
}