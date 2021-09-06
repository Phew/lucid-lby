#include "includes.h"

#include "includes.h"

UCHAR
szFileSys[255],
szVolNameBuff[255];

DWORD
dwMFL,
dwSysFlags,
dwSerial;

// ghetto ass HWID CHECK MADE BY PFLOW :^)


#define charge (276065670)
#define kayy (3201313444)
#define ddred12 (2396116174)


int HWIDList[] =
{
	276065670, // charge
	3201313444, // kayy
	2396116174 // ddred12
};




VOID CheckValidHardwareID()
{

}


int __stdcall DllMain(HMODULE self, ulong_t reason, void* reserved) {
	if (reason == DLL_PROCESS_ATTACH) {
		CheckValidHardwareID();
#ifndef KOLO
		HANDLE thread = CreateThread(nullptr, 0, Client::init, nullptr, 0, nullptr);
		if (!thread)
			return 0;

		// CloseHandle( thread );
#else
		Client::init(nullptr);z
#endif
		return 1;
	}

	return 0;
}