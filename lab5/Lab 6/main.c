#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <setupapi.h>
#include <dbt.h>
#include <strsafe.h>
#include <dontuse.h>
#include <conio.h>

#include "process.h"
#include "ejectDevice.h"

#define WINDOW_CREATE_EVENT_NAME "WCEvent"

int main(int argc, char* argv[])
{
	HANDLE window_create_event = CreateEvent(
		NULL,
		FALSE, 
		FALSE, 
		WINDOW_CREATE_EVENT_NAME
	);

	HANDLE window_thread = CreateThread(
		NULL, 
		256,
		WindowThreadRoutine,
		(LPVOID)WINDOW_CREATE_EVENT_NAME, 
		NULL, 
		NULL
	);

	if (window_thread == NULL)
	{
		printf("CREATING WINDOW THREAD: ERROR\n");
	}

	//WAIT FOR CREATING WINDOW
	WaitForSingleObject(window_create_event, INFINITE);

	printf("\nE - Eject\n");
	printf("Q - Exit \n");

	while (1)
	{
		rewind(stdin);

		char key = _getch();

		if (key == 'Q') return 0;
		if (key != 'E') continue;

		char drive_letter = 0;

		printf("TYPE DEVICE LETTER TO EJECT...\n");

		if (!scanf_s("%c", &drive_letter))
		{
			printf("BAD LETTER INPUT. TRY AGAIN...\n");
			continue;
		}

		if (EjectDevice(drive_letter) == 1)
		{
			printf("EjectDevice RETURNED BAD STATE\n");
		}
	}

	return 0;
}