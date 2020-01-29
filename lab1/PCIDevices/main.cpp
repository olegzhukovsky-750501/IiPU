#pragma comment (lib, "Setupapi.lib")
#include <stdio.h>
#include <tchar.h>
#include <iomanip>

#include <SDKDDKVer.h>

#include <Windows.h>
#include <locale.h>

#include <iostream>

#include <setupapi.h>
#include <regstr.h>


using namespace std;

const int BUFFER_SIZE = 400;


int main() {
	HDEVINFO hDevInfo;

	hDevInfo = SetupDiGetClassDevs(NULL, REGSTR_KEY_PCIENUM, 0, DIGCF_PRESENT | DIGCF_ALLCLASSES);

	SP_DEVINFO_DATA infoData;
	ZeroMemory(&infoData, sizeof(SP_DEVINFO_DATA));
	infoData.cbSize = sizeof(SP_DEVINFO_DATA);

	DWORD i = 0;
	cout.setf(ios::left);
	cout << "-----------------------------------------------------------------------------------------------------------------------------------------------------" << endl;
	cout << setw(3) << "NUM" << "|" << setw(84) << "DEVICE NAME" << "|" << setw(37) << "COMPANY NAME" << "|" << setw(10) << "VENDOR ID" << "|" << setw(10) << "DEVICE ID" << "|" << endl;
	cout << "=====================================================================================================================================================" << endl;
	while (SetupDiEnumDeviceInfo(hDevInfo, i, &infoData))
	{
		TCHAR deviceID[BUFFER_SIZE], deviceName[BUFFER_SIZE], companyName[BUFFER_SIZE];
		ZeroMemory(&deviceID, sizeof(deviceID));
		ZeroMemory(&deviceName, sizeof(deviceName));
		ZeroMemory(&companyName, sizeof(companyName));

		//get device ID
		SetupDiGetDeviceInstanceId(hDevInfo, &infoData, deviceID, sizeof(deviceID), NULL);
		//get the description of a device
		SetupDiGetDeviceRegistryProperty(hDevInfo, &infoData, SPDRP_DEVICEDESC, NULL, (PBYTE)deviceName, sizeof(deviceName), NULL);
		SetupDiGetDeviceRegistryProperty(hDevInfo, &infoData, SPDRP_MFG, NULL, (PBYTE)companyName, sizeof(companyName), NULL);

		string venAndDevId(deviceID);

		cout << setw(3) << i << "|" << setw(84) << deviceName << "|" << setw(37) << companyName << "|" << setw(10) << venAndDevId.substr(8, 4).c_str() << "|" << setw(10) << venAndDevId.substr(17, 4).c_str() << "|" << endl;
		cout << "-----------------------------------------------------------------------------------------------------------------------------------------------------" << endl;
		i++;
	}

	return 0;
}
