#include <stdio.h>

#pragma warning(push, 3)
#include <Windows.h>	// Don't want the warnings from Windows.h
#pragma warning(pop) 

#include "Main.h"

// MAKING A VIDEO GAME FROM SCRATCH IN C - VIDEO SERIES

int WinMain(HINSTANCE Instance, HINSTANCE PreviousInstance, PSTR CommandLine, INT CmdShow) {

	UNREFERENCED_PARAMETER(Instance);
	UNREFERENCED_PARAMETER(PreviousInstance);
	UNREFERENCED_PARAMETER(CommandLine);
	UNREFERENCED_PARAMETER(CmdShow);

	if (GameIsAlreadyRunning() == TRUE) { // No extra instances of this program!!!
		MessageBoxA(NULL, "Another instance of this program is already running!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if (CreateMainGameWindow() != ERROR_SUCCESS) {
		goto Exit;
	}

	// Messages
	MSG Message = { 0 };

	while (GetMessageA(&Message, NULL, 0, 0) > 0) {
		TranslateMessage(&Message);

		DispatchMessageA(&Message);
	}

Exit:
	return 0;
}


LRESULT CALLBACK MainWindowProc(_In_ HWND WindowHandle, _In_  UINT Message, _In_  WPARAM WParam, _In_  LPARAM LParam) {

	LRESULT Result = 0;

	switch (Message)
	{
	case WM_CLOSE:
	{
		PostQuitMessage(0); // Passes a message with 0 which ends the program
		break;
	}
	default:
	{
		return DefWindowProc(WindowHandle, Message, WParam, LParam);
	}
	}
	return Result;
}

DWORD CreateMainGameWindow(void) {
	DWORD Result = ERROR_SUCCESS;

	// Window Class
	WNDCLASSEXA WindowClass = { 0 };

	WindowClass.cbSize = sizeof(WNDCLASSEX);
	WindowClass.style = 0;
	WindowClass.lpfnWndProc = MainWindowProc;
	WindowClass.cbClsExtra = 0;
	WindowClass.cbWndExtra = 0;
	WindowClass.hInstance = GetModuleHandleA(NULL);
	WindowClass.hIcon = LoadIconA(NULL, IDI_APPLICATION);
	WindowClass.hCursor = LoadCursorA(NULL, IDC_ARROW);
	WindowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	WindowClass.lpszMenuName = NULL;
	WindowClass.lpszClassName = GAME_NAME "_WINDOWCLASS";
	WindowClass.hIconSm = LoadIconA(NULL, IDI_APPLICATION);

	if (RegisterClassExA(&WindowClass) == 0) {
		Result = GetLastError();
		MessageBoxA(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	// Window Handle
	HWND WindowHandle = 0;

	WindowHandle = CreateWindowEx(WS_EX_CLIENTEDGE, GAME_NAME "_WINDOWCLASS", "Window Title", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 600, 800, NULL, NULL, WindowClass.hInstance, NULL);

	if (WindowHandle == NULL) {
		Result = GetLastError();
		MessageBoxA(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

Exit:
	return Result;
}

BOOL GameIsAlreadyRunning(void) {
	HANDLE Mutex = NULL;
	Mutex = CreateMutexA(NULL, FALSE, GAME_NAME "_GameMutex");
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}
