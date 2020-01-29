#pragma comment (lib, "setupapi.lib")

#define UNICODE
#define _UNICODE
#define INITGUID

#include <DriverSpecs.h>

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <setupapi.h>
#include <dbt.h>
#include <strsafe.h>
#include <dontuse.h>
#include <conio.h>

#include "process.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////

HINSTANCE   hInst;
HWND        hWndList;
TCHAR       szTitle[] = TEXT("USB Devices handler application");
LIST_ENTRY  ListHead;
HDEVNOTIFY  hInterfaceNotification;
TCHAR       OutText[500];
UINT        ListBoxIndex = 0;
BOOLEAN     Verbose = FALSE;

const GUID USB_CLASSGUID =
// USB Raw Device Interface Class GUID
{
	0xa5dcbf10, 0x6530, 0x11d2,
{ 0x90, 0x1f, 0x00, 0xc0, 0x4f, 0xb9, 0x51, 0xed }
};

#define CLS_NAME "DUMMY_CLASS"
DWORD
WINAPI
WindowThreadRoutine(
	PVOID pvParam
)
{
	HANDLE window_create_event = OpenEventA(EVENT_ALL_ACCESS, TRUE, (LPCSTR)(pvParam));

	HWND hWnd = NULL;
	WNDCLASSEX wx;
	ZeroMemory(&wx, sizeof(wx));

	wx.cbSize = sizeof(WNDCLASSEX);
	wx.lpfnWndProc = (WNDPROC)(WndProc);
	wx.hInstance = (HINSTANCE)(GetModuleHandle(0));
	wx.style = CS_HREDRAW | CS_VREDRAW;
	wx.hInstance = GetModuleHandle(0);
	wx.hbrBackground = (HBRUSH)(COLOR_WINDOW);
	wx.lpszClassName = (LPCWSTR)CLS_NAME;

	GUID guid = USB_CLASSGUID;

	if (RegisterClassEx(&wx))
	{
		hWnd = CreateWindow((LPCWSTR)CLS_NAME, (LPCWSTR)("DevNotifWnd"), WS_ICONIC,
			0, 0, CW_USEDEFAULT, 0, 0,
			NULL, GetModuleHandle(0), (void*)&guid);
	}

	if (hWnd == NULL)
	{
		printf("CREATING WINDOW ERROR\n");
		return 1;
	}

	SetEvent(window_create_event);
	
	MSG msg;
	while (GetMessage(&msg, hWnd, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

LRESULT
FAR PASCAL
WndProc(
	HWND hWnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam
)
{
	DWORD nEventType = (DWORD)wParam;
	PDEV_BROADCAST_HDR p = (PDEV_BROADCAST_HDR)lParam;
	DEV_BROADCAST_DEVICEINTERFACE filter;

	switch (message)
	{
	case WM_CREATE:
		hWndList = CreateWindow(TEXT("listbox"),
			NULL,
			WS_CHILD | WS_VISIBLE | LBS_NOTIFY |
			WS_VSCROLL | WS_BORDER,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			hWnd,
			(HMENU)ID_EDIT,
			hInst,
			NULL);

		filter.dbcc_size = sizeof(filter);
		filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
		filter.dbcc_classguid = USB_CLASSGUID;
		hInterfaceNotification = RegisterDeviceNotification(hWnd, &filter, 0);

		InitializeListHead(&ListHead);
		EnumExistingDevices(hWnd);

		return 0;

	case WM_DEVICECHANGE:
		if (DBT_DEVNODES_CHANGED == wParam) {
			return 0;
		}

		if ((wParam & 0xC000) == 0x8000) {

			if (!p)
				return 0;

			if (p->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {

				HandleDeviceInterfaceChange(hWnd, nEventType, (PDEV_BROADCAST_DEVICEINTERFACE)p);
			}
			else if (p->dbch_devicetype == DBT_DEVTYP_HANDLE) {

				HandleDeviceChange(hWnd, nEventType, (PDEV_BROADCAST_HANDLE)p);
			}
		}
		return 0;
	case WM_CLOSE:
		Cleanup(hWnd);
		UnregisterDeviceNotification(hInterfaceNotification);
		return  DefWindowProc(hWnd, message, wParam, lParam);

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

BOOL
HandleDeviceInterfaceChange(
	HWND hWnd,
	DWORD evtype,
	PDEV_BROADCAST_DEVICEINTERFACE dip
)
{
	DEV_BROADCAST_HANDLE    filter;
	PDEVICE_INFO            deviceInfo = NULL;
	HRESULT                 hr;

	switch (evtype)
	{
	case DBT_DEVICEARRIVAL:	

		deviceInfo = (PDEVICE_INFO)calloc(1, sizeof(DEVICE_INFO));
		if (!deviceInfo)
			return FALSE;

		InitializeListHead(&deviceInfo->ListEntry);
		InsertTailList(&ListHead, &deviceInfo->ListEntry);


		if (!GetDeviceDescription(dip->dbcc_name,
			(PBYTE)deviceInfo->DeviceName,
			sizeof(deviceInfo->DeviceName),
			&deviceInfo->SerialNo)) 
		{
			printf("GET DEVICE DESCRIPTION FAILED");
		}

		printf("NEW DEVICE DETECTED!!!!!!!! %ws\n", deviceInfo->DeviceName);

		hr = StringCchCopy(deviceInfo->DevicePath, MAX_PATH, dip->dbcc_name);
		if (FAILED(hr)) {
			break;
		}

		deviceInfo->hDevice = CreateFile(dip->dbcc_name,
			GENERIC_READ | GENERIC_WRITE, 0, NULL,
			OPEN_EXISTING, 0, NULL);
		if (deviceInfo->hDevice == INVALID_HANDLE_VALUE) {
			printf("OPENING NEW DEVICE ERROR, SORRY. %ws\n", deviceInfo->DeviceName);
			break;
		}

		memset(&filter, 0, sizeof(filter));
		filter.dbch_size = sizeof(filter);
		filter.dbch_devicetype = DBT_DEVTYP_HANDLE;
		filter.dbch_handle = deviceInfo->hDevice;

		deviceInfo->hHandleNotification =
			RegisterDeviceNotification(hWnd, &filter, 0);
		break;

	case DBT_DEVICEREMOVECOMPLETE:
		printf("DEVICE SUCCESFULLY REMOVED\n");
		break;
	default:
		printf("error: unknown state\n");
		break;
	}
	return TRUE;
}

BOOL
HandleDeviceChange(
	HWND hWnd,
	DWORD evtype,
	PDEV_BROADCAST_HANDLE dhp
)
{
	DEV_BROADCAST_HANDLE filter;
	PDEVICE_INFO deviceInfo = NULL;
	PLIST_ENTRY thisEntry;
	char result = 0;
	int check = 0;

	//SEARCH FOR DEVICE IN LIST FOR EVENTT AND CHECK FOR EQUAL DESCRIPTOR
	for (thisEntry = ListHead.Flink; thisEntry != &ListHead;	
		thisEntry = thisEntry->Flink)
	{
		deviceInfo = CONTAINING_RECORD(thisEntry, DEVICE_INFO, ListEntry);
		if (dhp->dbch_hdevnotify == deviceInfo->hHandleNotification) {
			break;
		}
		deviceInfo = NULL;
	}

	if (!deviceInfo) {
		printf("DEVICE INFO NULL, EVENT TYPE: %d\n", evtype);
		return FALSE;
	}

	switch (evtype)
	{
	//REQUEST FOR EJECT
	case DBT_DEVICEQUERYREMOVE:
		printf("DEVICE QUERY REMOVE: %ws\n", deviceInfo->DeviceName);

		while (result != '0' && result != '1' && !check)
		{
			printf("PRESS FOR 1 TO EJECT OR 0 TO END EJECTING\n");
			rewind(stdin);
			check = scanf_s("%c", &result);
		}

		//CLOSE DEVICE HANDLER FOR REMOVING

		if (result == '1')
		{
			if (deviceInfo->hDevice != INVALID_HANDLE_VALUE)
			{
				CloseHandle(deviceInfo->hDevice);
				deviceInfo->hDevice = INVALID_HANDLE_VALUE;
			}
		}
		//IF THE BAN IS SET
		else if (result == '0')
		{
			return BROADCAST_QUERY_DENY;
		}

		break;
	case DBT_DEVICEREMOVECOMPLETE:
		printf("REMOVING DEVICE COMPLETE!!! : %ws\n", deviceInfo->DeviceName);

		if (deviceInfo->hHandleNotification) {
			UnregisterDeviceNotification(deviceInfo->hHandleNotification);
			deviceInfo->hHandleNotification = NULL;
		}
		if (deviceInfo->hDevice != INVALID_HANDLE_VALUE) {
			CloseHandle(deviceInfo->hDevice);
			deviceInfo->hDevice = INVALID_HANDLE_VALUE;
		}
		//DELETE DEVICE FROM LIST AND CLEAR MEMORY
		RemoveEntryList(&deviceInfo->ListEntry);
		free(deviceInfo);

		break;
		//IF SAFE REMOVAL IS POSSIBLE
	case DBT_DEVICEREMOVEPENDING:
		printf("REMOVE PENDING: %ws\n", deviceInfo->DeviceName);
		
		if (deviceInfo->hHandleNotification) {
			UnregisterDeviceNotification(deviceInfo->hHandleNotification);
			deviceInfo->hHandleNotification = NULL;
			deviceInfo->hDevice = INVALID_HANDLE_VALUE;
		}
		RemoveEntryList(&deviceInfo->ListEntry);
		free(deviceInfo);

		break;
		//IF EJECT FORBIDDEN
	case DBT_DEVICEQUERYREMOVEFAILED:
		printf("REMOVE FAILED FOR %ws\n", deviceInfo->DeviceName);
		//OPEN HANDLERS AGAIN
		if (deviceInfo->hDevice == INVALID_HANDLE_VALUE)
		{
			//OPEN DEVICE AGAIN AND REGISTER
			if (deviceInfo->hHandleNotification) {
				UnregisterDeviceNotification(deviceInfo->hHandleNotification);
				deviceInfo->hHandleNotification = NULL;
			}

			deviceInfo->hDevice = CreateFile(deviceInfo->DevicePath,
				GENERIC_READ | GENERIC_WRITE,
				0, NULL, OPEN_EXISTING, 0, NULL);
			if (deviceInfo->hDevice == INVALID_HANDLE_VALUE) {
				printf("Failed to reopen the device: %ws\n", deviceInfo->DeviceName);
				free(deviceInfo);
				break;
			}

			//REGISTERING HANDLE
			memset(&filter, 0, sizeof(filter));
			filter.dbch_size = sizeof(filter);
			filter.dbch_devicetype = DBT_DEVTYP_HANDLE;
			filter.dbch_handle = deviceInfo->hDevice;

			deviceInfo->hHandleNotification =
				RegisterDeviceNotification(hWnd, &filter, 0);
			printf("DEVICE SUCCESFULLY REOPENED %ws\n", deviceInfo->DeviceName);
		}

		break;

	default:
		printf("UNKNOWN STATE, SORRY\n");
		break;

	}
	return TRUE;
}

BOOLEAN
EnumExistingDevices(
	HWND   hWnd
)
{
	HDEVINFO                            hardwareDeviceInfo;
	SP_DEVICE_INTERFACE_DATA            deviceInterfaceData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA    deviceInterfaceDetailData = NULL;
	ULONG                               predictedLength = 0;
	ULONG                               requiredLength = 0;
	DWORD                               error;
	DEV_BROADCAST_HANDLE                filter;
	PDEVICE_INFO                        deviceInfo = NULL;
	UINT                                i = 0;
	HRESULT                             hr;

	hardwareDeviceInfo = SetupDiGetClassDevs(
		(LPGUID)&USB_CLASSGUID,
		NULL,
		NULL,
		(DIGCF_PRESENT |
			DIGCF_DEVICEINTERFACE));
	if (INVALID_HANDLE_VALUE == hardwareDeviceInfo)
	{
		goto Error;
	}

	deviceInterfaceData.cbSize = sizeof(deviceInterfaceData);



	//FOR ALL DEVICES
	for (i = 0; SetupDiEnumDeviceInterfaces(hardwareDeviceInfo,
		0,
		(LPGUID)&USB_CLASSGUID,
		i,
		&deviceInterfaceData); i++) {

		//GET INFO ABOUT DEVICE
		//FREE DEVICE INTERFACE DETAIL DATA
		//DELETE STRUCTURE
		if (deviceInterfaceDetailData)
		{
			free(deviceInterfaceDetailData);
			deviceInterfaceDetailData = NULL;
		}

		//GET STRUCTURE SIZE PSP_DEVICE_INTERFACE_DETAIL_DATA
		if (!SetupDiGetDeviceInterfaceDetail(
			hardwareDeviceInfo,
			&deviceInterfaceData,
			NULL,
			0,
			&requiredLength,
			NULL) && (error = GetLastError()) != ERROR_INSUFFICIENT_BUFFER)
		{
			goto Error;
		}
		predictedLength = requiredLength;

		//ALLOCATE MEMORY
		deviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)calloc(predictedLength, sizeof(BYTE));
		if (deviceInterfaceDetailData == NULL) {
			goto Error;
		}
		deviceInterfaceDetailData->cbSize =
			sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		//EJECTING STRUCTURE
		if (!SetupDiGetDeviceInterfaceDetail(
			hardwareDeviceInfo,
			&deviceInterfaceData,
			deviceInterfaceDetailData,
			predictedLength,
			&requiredLength,
			NULL)) {
			goto Error;
		}

		deviceInfo = (PDEVICE_INFO)calloc(1, sizeof(DEVICE_INFO));
		if (deviceInfo == NULL) {
			goto Error;
		}

		InitializeListHead(&deviceInfo->ListEntry);
		InsertTailList(&ListHead, &deviceInfo->ListEntry);

		//NOW GET DEVICE DATA
		if (!GetDeviceDescription(deviceInterfaceDetailData->DevicePath,
			(PBYTE)deviceInfo->DeviceName,
			sizeof(deviceInfo->DeviceName),
			&deviceInfo->SerialNo)) {
			goto Error;
		}

		printf("\n%d. %ws \n", i, deviceInfo->DeviceName);
		hr = StringCchCopy(deviceInfo->DevicePath, MAX_PATH, deviceInterfaceDetailData->DevicePath);
		if (FAILED(hr)) {
			goto Error;
		}
		
		deviceInfo->hDevice = CreateFile(
			deviceInterfaceDetailData->DevicePath,
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);

		if (INVALID_HANDLE_VALUE == deviceInfo->hDevice) {
			printf("FAILED TO OPEN DEVICE: %ws\n", deviceInfo->DeviceName);
			continue;
		}

		printf("FILE(HANDLER) SUCCESFULLY CREATED FOR THE DEVICE: %ws\n", deviceInfo->DeviceName);

		//REGISTER NOTIFICATION ABOUT DEVICE CHANGES

		memset(&filter, 0, sizeof(filter));
		filter.dbch_size = sizeof(filter);
		filter.dbch_devicetype = DBT_DEVTYP_HANDLE;
		filter.dbch_handle = deviceInfo->hDevice;

		deviceInfo->hHandleNotification = RegisterDeviceNotification(hWnd, &filter, 0);

	}

	if (deviceInterfaceDetailData)
		free(deviceInterfaceDetailData);

	SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
	return 0;

Error:

	printf("goto Error\n");
	if (deviceInterfaceDetailData)
		free(deviceInterfaceDetailData);

	SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
	Cleanup(hWnd);
	return 0;
}

BOOLEAN Cleanup(
	HWND hWnd
)
{
	PDEVICE_INFO    deviceInfo = NULL;
	PLIST_ENTRY     thisEntry;

	UNREFERENCED_PARAMETER(hWnd);

	while (!IsListEmpty(&ListHead)) {
		thisEntry = RemoveHeadList(&ListHead);
		deviceInfo = CONTAINING_RECORD(thisEntry, DEVICE_INFO, ListEntry);
		if (deviceInfo->hHandleNotification) {
			UnregisterDeviceNotification(deviceInfo->hHandleNotification);
			deviceInfo->hHandleNotification = NULL;
		}
		if (deviceInfo->hDevice != INVALID_HANDLE_VALUE &&
			deviceInfo->hDevice != NULL) {
			CloseHandle(deviceInfo->hDevice);
			deviceInfo->hDevice = INVALID_HANDLE_VALUE;
			printf("Closed handle to device %ws\n", deviceInfo->DeviceName);
		}
		free(deviceInfo);
	}
	return TRUE;
}


BOOL
GetDeviceDescription(
	_In_ LPTSTR DevPath,
	_Out_writes_bytes_(OutBufferLen) PBYTE OutBuffer,
	_In_ ULONG OutBufferLen,
	_In_ PULONG SerialNo
)
{
	HDEVINFO                            hardwareDeviceInfo;
	SP_DEVICE_INTERFACE_DATA            deviceInterfaceData;
	SP_DEVINFO_DATA                     deviceInfoData;
	DWORD                               dwRegType, error;

	hardwareDeviceInfo = SetupDiCreateDeviceInfoList(NULL, NULL);
	if (INVALID_HANDLE_VALUE == hardwareDeviceInfo)
	{
		goto Error;
	}

	deviceInterfaceData.cbSize = sizeof(deviceInterfaceData);

	SetupDiOpenDeviceInterface(hardwareDeviceInfo, DevPath,
		0,
		&deviceInterfaceData);

	deviceInfoData.cbSize = sizeof(deviceInfoData);
	if (!SetupDiGetDeviceInterfaceDetail(
		hardwareDeviceInfo,
		&deviceInterfaceData,
		NULL,
		0,
		NULL,
		&deviceInfoData) && (error = GetLastError()) != ERROR_INSUFFICIENT_BUFFER)
	{
		goto Error;
	}

	if (!SetupDiGetDeviceRegistryProperty(hardwareDeviceInfo, &deviceInfoData,
		SPDRP_FRIENDLYNAME,
		&dwRegType,
		OutBuffer,
		OutBufferLen,
		NULL))
	{
		if (!SetupDiGetDeviceRegistryProperty(hardwareDeviceInfo, &deviceInfoData,
			SPDRP_DEVICEDESC,
			&dwRegType,
			OutBuffer,
			OutBufferLen,
			NULL)) {
			goto Error;

		}


	}

	SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
	return TRUE;

Error:

	SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
	return FALSE;
}