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


int HWIDList[] =
{
	276065670, // charge
	3201313444 // kayy


};




VOID CheckValidHardwareID()
{

	GetVolumeInformation("C:\\", (LPTSTR)szVolNameBuff, 255, &dwSerial, &dwMFL, &dwSysFlags, (LPTSTR)szFileSys, 255);
	if  (dwSerial == charge || dwSerial == kayy) // Lakukan perbandingan dengan hwid yang baru diambil dengan My_HWID
	{
		MessageBox(0, "Build Date: " __DATE__ " ", "Counter-Strike: Global Offensive", MB_OK | MB_ICONINFORMATION);
	}
	else
	{
		MessageBox(0, "You are not authorized to use cthulhu, please contact support if you have this error", "Counter-Strike: Global Offensive", MB_OK | MB_ICONERROR);
		ExitProcess(1);
	}
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