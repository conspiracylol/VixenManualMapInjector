#include <windows.h>
#include <fstream>

#define OUTPUT_PLEASE_BABY_BOY 0
#define ALLOC_CONSOLE_PLEASE 0

#if !OUTPUT_PLEASE_BABY_BOY
#define VLog(data, ...)
#else
#define VLog(text, ...) printf(text, __VA_ARGS__);
#endif

#include <stdio.h>
#include <string>
#include <iostream>
#include "injector.h"


static int MMapWithPath(wchar_t* Path)
{
	//MessageBoxA(0, "Mapping..", 0, 0);

#if OUTPUT_PLEASE_BABY_BOY
#if ALLOC_CONSOLE_PLEASE
	AllocConsole();
	freopen("conin$", "r", stdin);
	freopen("conout$", "w", stdout);
	freopen("conout$", "w", stderr);
#endif
#endif

	wchar_t* dllPath = Path;

	if (GetFileAttributes(dllPath) == INVALID_FILE_ATTRIBUTES)
	{
		VLog("[Vixen] Error: dll does NOT exist!\r\n");

		return -1;

	}

	std::ifstream File(dllPath, std::ios::binary | std::ios::ate);

	if (File.fail())
	{

		VLog("[Vixen] Error: Failed to open DLL!\r\n");

		File.close();
		return -1;
	}

	auto FileSize = File.tellg();
	if (FileSize < 0x1000)
	{
		VLog("[Vixen] Error: Invalid File Size!\r\n");

		File.close();
		return -1;
	}

	BYTE* pSrcData = new BYTE[(UINT_PTR)FileSize];
	if (!pSrcData)
	{
		VLog("[Vixen] Error: Failed to allocate file!\r\n");
		File.close();
		return -1;
	}

	File.seekg(0, std::ios::beg);
	File.read((char*)(pSrcData), FileSize);
	File.close();

	if (!ManualMapDll(pSrcData, FileSize))
	{
		VLog("[Vixen] Error: Failed to map dll!\r\n");

		delete[] pSrcData;		
		return -1;
	}


	delete[] pSrcData;

	VLog("[Vixen] Success!\r\n");

	//MessageBoxA(0, "Success", 0, 0);

	return 0x69;
}
