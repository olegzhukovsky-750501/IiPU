#include "ejectDevice.h"

//-------------------------------------------------
int EjectDevice(char DriveLetter)
{
	//MAKE LETTER IN UPPER CASE
	DriveLetter &= ~0x20;

	if (DriveLetter < 'A' || DriveLetter > 'Z') 
	{
		return 1;
	}


	char szRootPath[] = "X:\\";
	szRootPath[0] = DriveLetter;

	//volume
	char szVolumeAccessPath[] = "\\\\.\\X:";
	szVolumeAccessPath[4] = DriveLetter;

	long DeviceNumber = -1;

	//volume handler
	HANDLE hVolume = CreateFile(szVolumeAccessPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);
	if (hVolume == INVALID_HANDLE_VALUE) {
		return 1;
	}

	//get device number
	STORAGE_DEVICE_NUMBER sdn;
	DWORD dwBytesReturned = 0;
	long res = DeviceIoControl(hVolume, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn, sizeof(sdn), &dwBytesReturned, NULL);
	if (res) {
		DeviceNumber = sdn.DeviceNumber;
	}
	CloseHandle(hVolume);

	if (DeviceNumber == -1) {
		return 1;
	}

	UINT DriveType = GetDriveType(szRootPath);

	//DEVICE Instance for Ejecting
	DEVINST DevInst = GetDrivesDevInstByDeviceNumber(DeviceNumber, DriveType);
	if (DevInst == 0) {
		return 1;
	}

	//BAN REASON
	PNP_VETO_TYPE VetoType = PNP_VetoTypeUnknown;
	WCHAR VetoNameW[MAX_PATH];
	VetoNameW[0] = 0;
	int bSuccess = FALSE;

	//INTERFACES LIST. USE TOP ELEMENT FOR EJECTING
	DEVINST DevInstParent = 0;
	res = CM_Get_Parent(&DevInstParent, DevInst, 0);


	VetoNameW[0] = 0;




	//DEVICE EJECTING
	res = CM_Request_Device_EjectW(DevInstParent, &VetoType, VetoNameW, MAX_PATH, 0);

	bSuccess = (res == CR_SUCCESS && VetoType == PNP_VetoTypeUnknown);

	if (bSuccess) {
		printf("SUCCESS\n");
		return 0;
	}

	printf("FAILED !!!\n");

	printf("RESULT=0x%2X\n", res);

	if (VetoNameW[0]) {
		printf("VetoName=%ws", VetoNameW);
		printf("\n");
	}
	return 1;
}

DEVINST GetDrivesDevInstByDeviceNumber(long DeviceNumber, UINT DriveType)
{
	GUID* guid;
	//CHECK IF DEVICE REMOVABLE
	switch (DriveType) {
	case DRIVE_REMOVABLE:
		guid = (GUID*)&GUID_DEVINTERFACE_DISK;
		break;
	default:
		return 0;
	}

	//GET ALL DEVICES CONNECTED TO SYSTEM
	HDEVINFO hDevInfo = SetupDiGetClassDevs(guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if (hDevInfo == INVALID_HANDLE_VALUE) {
		return 0;
	}

	DWORD dwIndex = 0;
	long res;

	BYTE Buf[1024];
	PSP_DEVICE_INTERFACE_DETAIL_DATA pspdidd = (PSP_DEVICE_INTERFACE_DETAIL_DATA)Buf;
	SP_DEVICE_INTERFACE_DATA         spdid;
	SP_DEVINFO_DATA                  spdd;
	DWORD                            dwSize;

	spdid.cbSize = sizeof(spdid);

	while (TRUE) {
		res = SetupDiEnumDeviceInterfaces(hDevInfo, NULL, guid, dwIndex, &spdid);
		if (!res) {
			break;
		}

		dwSize = 0;
		SetupDiGetDeviceInterfaceDetail(hDevInfo, &spdid, NULL, 0, &dwSize, NULL); //CHECK BUFF SIZE
		//CHECK
		if (dwSize != 0 && dwSize <= sizeof(Buf)) {

			pspdidd->cbSize = sizeof(*pspdidd);

			ZeroMemory(&spdd, sizeof(spdd));
			spdd.cbSize = sizeof(spdd);
			//EJECT INTERFACE ATTRIBUTES
			long res = SetupDiGetDeviceInterfaceDetail(hDevInfo, &spdid, pspdidd, dwSize, &dwSize, &spdd);
			if (res) {
				//OPEN DISK HANDLER
				HANDLE hDrive = CreateFile(pspdidd->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
				if (hDrive != INVALID_HANDLE_VALUE) {
					//GET HIS NUMBER
					STORAGE_DEVICE_NUMBER sdn;
					DWORD dwBytesReturned = 0;
					res = DeviceIoControl(hDrive, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn, sizeof(sdn), &dwBytesReturned, NULL);
					if (res) {
						//COMPARE WITH THOSE NUMBER WHICH IS AN ARGUMENT
						if (DeviceNumber == (long)sdn.DeviceNumber) 
						{
							CloseHandle(hDrive);
							SetupDiDestroyDeviceInfoList(hDevInfo);
							return spdd.DevInst;
						}
					}
					CloseHandle(hDrive);
				}
			}
		}
		dwIndex++;
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);
	return 0;
}