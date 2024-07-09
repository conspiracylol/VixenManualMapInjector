#include "injector.h"
#include <windows.h>
#include <iostream>

#define OUTPUT_PLEASE_NIGGA 0
#if !(OUTPUT_PLEASE_NIGGA)
#define ILog(data, ...)
#else
#define ILog(text, ...) printf(text, __VA_ARGS__);
#endif

#ifdef _WIN64
#define CURRENT_ARCH IMAGE_FILE_MACHINE_AMD64
#else
#define CURRENT_ARCH IMAGE_FILE_MACHINE_I386
#endif

bool ManualMapDll(BYTE* pSrcData, SIZE_T FileSize, bool ClearHeader, bool ClearNonNeededSections, bool AdjustProtections, bool SEHExceptionSupport, DWORD fdwReason, LPVOID lpReserved) {
    IMAGE_NT_HEADERS* pOldNtHeader = nullptr;
    IMAGE_OPTIONAL_HEADER* pOldOptHeader = nullptr;
    IMAGE_FILE_HEADER* pOldFileHeader = nullptr;
    BYTE* pTargetBase = nullptr;

    if (reinterpret_cast<IMAGE_DOS_HEADER*>(pSrcData)->e_magic != 0x5A4D) { //"MZ"
        ILog("[Vixen] Invalid file\n");
        return false;
    }

    pOldNtHeader = reinterpret_cast<IMAGE_NT_HEADERS*>(pSrcData + reinterpret_cast<IMAGE_DOS_HEADER*>(pSrcData)->e_lfanew);
    pOldOptHeader = &pOldNtHeader->OptionalHeader;
    pOldFileHeader = &pOldNtHeader->FileHeader;

    if (pOldFileHeader->Machine != CURRENT_ARCH) {
        ILog("[Vixen] Invalid platform\n");
        return false;
    }

    ILog("[Vixen] File ok\n");

    pTargetBase = reinterpret_cast<BYTE*>(VirtualAlloc(nullptr, pOldOptHeader->SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!pTargetBase)
    {
        ILog("[Vixen] Memory allocation failed 0x%X\n", GetLastError());
        return false;
    }

    DWORD oldProtect = 0;
    VirtualProtect(pTargetBase, pOldOptHeader->SizeOfImage, PAGE_EXECUTE_READWRITE, &oldProtect);

    //Sleep(2500);

    MANUAL_MAPPING_DATA data{ 0 };
    data.pLoadLibraryA = IFH(LoadLibraryA).get();
    data.pGetProcAddress = IFH(GetProcAddress).get();
#ifdef _WIN64
    data.pRtlAddFunctionTable = (f_RtlAddFunctionTable)IFH(RtlAddFunctionTable).get();
#else 
    SEHExceptionSupport = false;
#endif
    data.pbase = pTargetBase;
    data.fdwReasonParam = fdwReason;
    data.reservedParam = lpReserved;
    data.SEHSupport = SEHExceptionSupport;

    // File header
    memcpy(pTargetBase, pSrcData, 0x1000); // only first 0x1000 bytes for the header

    IMAGE_SECTION_HEADER* pSectionHeader = IMAGE_FIRST_SECTION(pOldNtHeader);
    for (UINT i = 0; i != pOldFileHeader->NumberOfSections; ++i, ++pSectionHeader)
    {
        if (pSectionHeader->SizeOfRawData) {
            memcpy(pTargetBase + pSectionHeader->VirtualAddress, pSrcData + pSectionHeader->PointerToRawData, pSectionHeader->SizeOfRawData);
        }
    }

    // Shell code
    BYTE* pShellcode = reinterpret_cast<BYTE*>(VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!pShellcode) {
        ILog("[Vixen] Memory shellcode allocation failed 0x%X\n", GetLastError());
        VirtualFree(pTargetBase, 0, MEM_RELEASE);
        return false;
    }

    //Sleep(2500);
    memcpy(pShellcode, Shellcode, 0x1000);
   //Sleep(2500);

    ILog("[Vixen] Mapped DLL at %p\n", pTargetBase);
    ILog("[Vixen] Shell code at %p\n", pShellcode);

    ILog("[Vixen] Data allocated\n");

    // Declare a function pointer with the appropriate signature
    using ShellcodeFunc = void(*)(MANUAL_MAPPING_DATA*);

    // Cast the shellcode pointer to the function pointer
    ShellcodeFunc shellcodeFunc = reinterpret_cast<ShellcodeFunc>(pShellcode);

    // Call the shellcode function
    shellcodeFunc(&data);


    if (data.hMod == (HINSTANCE)0x404040) {
        ILog("[Vixen] Wrong mapping ptr\n");
        VirtualFree(pTargetBase, 0, MEM_RELEASE);
        VirtualFree(pShellcode, 0, MEM_RELEASE);
        return false;
    }
    else if (data.hMod == (HINSTANCE)0x505050) {
        ILog("[Vixen] WARNING: Exception support failed!\n");
    }

    BYTE* emptyBuffer = (BYTE*)malloc(1024 * 1024 * 20);
    if (emptyBuffer == nullptr) {
        ILog("[Vixen] Unable to allocate memory\n");
        return false;
    }

    printf("[Vixen] Mapped Modules Count: %d!\r\n", data.test);


    memset(emptyBuffer, 0, 1024 * 1024 * 20);

    // CLEAR PE HEAD
    if (ClearHeader) {
        memset(pTargetBase, 0, 0x1000);
    }

    // CLEAR NON-NEEDED SECTIONS
    if (ClearNonNeededSections) {
        pSectionHeader = IMAGE_FIRST_SECTION(pOldNtHeader);
        for (UINT i = 0; i != pOldFileHeader->NumberOfSections; ++i, ++pSectionHeader) {
            if (pSectionHeader->Misc.VirtualSize) {
                if ((SEHExceptionSupport ? 0 : strcmp((char*)pSectionHeader->Name, ".pdata") == 0) ||
                    strcmp((char*)pSectionHeader->Name, ".rsrc") == 0 ||
                    strcmp((char*)pSectionHeader->Name, ".reloc") == 0) {
                    ILog("[Vixen] Processing %s removal\n", pSectionHeader->Name);
                    memset(pTargetBase + pSectionHeader->VirtualAddress, 0, pSectionHeader->Misc.VirtualSize);
                }
            }
        }
    }

    // ADJUST PROTECTIONS
    if (AdjustProtections)
    {
        pSectionHeader = IMAGE_FIRST_SECTION(pOldNtHeader);
        for (UINT i = 0; i != pOldFileHeader->NumberOfSections; ++i, ++pSectionHeader) {
            if (pSectionHeader->Misc.VirtualSize) {
                DWORD old = 0;
                DWORD newP = PAGE_READONLY;

                if ((pSectionHeader->Characteristics & IMAGE_SCN_MEM_WRITE) > 0) {
                    newP = PAGE_READWRITE;
                }
                else if ((pSectionHeader->Characteristics & IMAGE_SCN_MEM_EXECUTE) > 0) {
                    newP = PAGE_EXECUTE_READ;
                }
                if (VirtualProtect(pTargetBase + pSectionHeader->VirtualAddress, pSectionHeader->Misc.VirtualSize, newP, &old)) {
                    ILog("[Vixen] section %s set as %lX\n", (char*)pSectionHeader->Name, newP);
                }
                else {
                    ILog("[Vixen] FAIL: section %s not set as %lX\n", (char*)pSectionHeader->Name, newP);
                }
            }
        }
        DWORD old = 0;
        VirtualProtect(pTargetBase, IMAGE_FIRST_SECTION(pOldNtHeader)->VirtualAddress, PAGE_READONLY, &old);
    }

    memset(pShellcode, 0, 0x1000);
    VirtualFree(pShellcode, 0, MEM_RELEASE);
    free(emptyBuffer);

    return true;
}

#define RELOC_FLAG32(RelInfo) ((RelInfo >> 0x0C) == IMAGE_REL_BASED_HIGHLOW)
#define RELOC_FLAG64(RelInfo) ((RelInfo >> 0x0C) == IMAGE_REL_BASED_DIR64)

#ifdef _WIN64
#define RELOC_FLAG RELOC_FLAG64
#else
#define RELOC_FLAG RELOC_FLAG32
#endif

#pragma runtime_checks("", off)
#pragma optimize("", off)
void __stdcall Shellcode(MANUAL_MAPPING_DATA* pData) 

    int loadedImportCount = 0;

    if (!pData) {
        pData->hMod = (HINSTANCE)0x404040;
        return;
    }

    BYTE* pBase = pData->pbase;
    auto* pOpt = &reinterpret_cast<IMAGE_NT_HEADERS*>(pBase + reinterpret_cast<IMAGE_DOS_HEADER*>((uintptr_t)pBase)->e_lfanew)->OptionalHeader;

    auto _LoadLibraryA = pData->pLoadLibraryA;
    auto _GetProcAddress = pData->pGetProcAddress;
#ifdef _WIN64
    auto _RtlAddFunctionTable = pData->pRtlAddFunctionTable;
#endif
    auto _DllMain = reinterpret_cast<f_DLL_ENTRY_POINT>(pBase + pOpt->AddressOfEntryPoint);

    BYTE* LocationDelta = pBase - pOpt->ImageBase;
    if (LocationDelta) {
        if (pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size) {
            auto* pRelocData = reinterpret_cast<IMAGE_BASE_RELOCATION*>(pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
            const auto* pRelocEnd = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<BYTE*>(pRelocData) + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size);
            while (pRelocData < pRelocEnd && pRelocData->VirtualAddress) {
                UINT AmountOfEntries = (pRelocData->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                WORD* pRelativeInfo = reinterpret_cast<WORD*>(pRelocData + 1);
                for (UINT i = 0; i != AmountOfEntries; ++i, ++pRelativeInfo) {
                    if (RELOC_FLAG(*pRelativeInfo)) {
                        UINT_PTR* pPatch = reinterpret_cast<UINT_PTR*>(pBase + pRelocData->VirtualAddress + ((*pRelativeInfo) & 0xFFF));
                        *pPatch += reinterpret_cast<UINT_PTR>(LocationDelta);
                    }
                }
                pRelocData = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<BYTE*>(pRelocData) + pRelocData->SizeOfBlock);
            }
        }
    }

    if (pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size) {
        auto* pImportDescr = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        while (pImportDescr->Name)
        {
            char* szMod = reinterpret_cast<char*>(pBase + pImportDescr->Name);
            loadedImportCount++;

            //printf("Summoning nigger import with name: %s!\r\n", szMod);
            //system("pause");

            HINSTANCE hDll = _LoadLibraryA(szMod);
            ULONG_PTR* pThunkRef = reinterpret_cast<ULONG_PTR*>(pBase + pImportDescr->OriginalFirstThunk);
            ULONG_PTR* pFuncRef = reinterpret_cast<ULONG_PTR*>(pBase + pImportDescr->FirstThunk);

            if (!pThunkRef) {
                pThunkRef = pFuncRef;
            }

            for (; *pThunkRef; ++pThunkRef, ++pFuncRef) {
                if (IMAGE_SNAP_BY_ORDINAL(*pThunkRef)) {
                    *pFuncRef = reinterpret_cast<ULONG_PTR>(_GetProcAddress(hDll, reinterpret_cast<char*>(*pThunkRef & 0xFFFF)));
                }
                else {
                    auto* pImport = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(pBase + (*pThunkRef));
                    *pFuncRef = reinterpret_cast<ULONG_PTR>(_GetProcAddress(hDll, pImport->Name));
                }
            }
            ++pImportDescr;
        }
    }

    if (pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size) {
        auto* pTLS = reinterpret_cast<IMAGE_TLS_DIRECTORY*>(pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);
        auto** pCallback = reinterpret_cast<PIMAGE_TLS_CALLBACK*>(pTLS->AddressOfCallBacks);
        for (; pCallback && *pCallback; ++pCallback) {
            (*pCallback)(pBase, DLL_PROCESS_ATTACH, nullptr);
        }
    }

#ifdef _WIN64
    if (pData->SEHSupport)
    {
        auto* pRuntimeFunction = reinterpret_cast<IMAGE_RUNTIME_FUNCTION_ENTRY*>(pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].VirtualAddress);
        ULONG EntryAmount = pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].Size / sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY);
        if (!_RtlAddFunctionTable(pRuntimeFunction, EntryAmount, reinterpret_cast<DWORD64>(pBase))) {
            pData->hMod = (HINSTANCE)0x505050;
        }
    }
#endif

    //printf("New import count: %d!\r\n", loadedImportCount);

    pData->test = loadedImportCount;

    _DllMain(pBase, pData->fdwReasonParam, pData->reservedParam);
    pData->hMod = reinterpret_cast<HINSTANCE>(pBase);
}
#pragma runtime_checks("", restore)
#pragma optimize("", on)
