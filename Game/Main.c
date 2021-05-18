#include <stdio.h>

#pragma warning(push, 3)
#include <Windows.h>	// Don't want the warnings from Windows.h
#pragma warning(pop) 

#include <stdint.h> // type definitions

#include "Main.h"

// MAKING A VIDEO GAME FROM SCRATCH IN C - VIDEO SERIES

HWND gGameWindow;

BOOL gGameIsRunning;

GAMEBITMAP gBackBuffer;

MONITORINFO gMonitorInfo = { sizeof(MONITORINFO) };

int32_t gMonitorWidth;

int32_t gMonitorHeight;

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

	gBackBuffer.BitmapInfo.bmiHeader.biSize = sizeof(gBackBuffer.BitmapInfo.bmiHeader);
	gBackBuffer.BitmapInfo.bmiHeader.biWidth = GAME_RES_WIDTH;
	gBackBuffer.BitmapInfo.bmiHeader.biHeight = GAME_RES_HEIGHT;
	gBackBuffer.BitmapInfo.bmiHeader.biBitCount = GAME_BPP;
	gBackBuffer.BitmapInfo.bmiHeader.biCompression = BI_RGB;
	gBackBuffer.BitmapInfo.bmiHeader.biPlanes = 1;
	if ((gBackBuffer.Memory = VirtualAlloc(NULL, GAME_DRAWING_AREA_MEMORY_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) == NULL) {
		// If memory fails to be allocated, 0 will be returned
		MessageBoxA(NULL, "Failed to allocate memory for drawing surface!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	memset(gBackBuffer.Memory, 0x7f, GAME_DRAWING_AREA_MEMORY_SIZE);

	// Messages
	MSG Message = { 0 };

	gGameIsRunning = TRUE;

	while (gGameIsRunning == TRUE) { // Game Loop
		while (PeekMessageA(&Message, gGameWindow, 0, 0, PM_REMOVE)) { // Message Queue Loop
			DispatchMessageA(&Message);
		}

		ProcessPlayerInput();
		
		RenderFrameGraphics();

		Sleep(1); // Will be revisited for tuning fps

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
			gGameIsRunning = FALSE;
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
	WindowClass.hbrBackground = CreateSolidBrush(RGB(255, 0, 255)); // Hideous color in order to stand out
	WindowClass.lpszMenuName = NULL;
	WindowClass.lpszClassName = GAME_NAME "_WINDOWCLASS";
	WindowClass.hIconSm = LoadIconA(NULL, IDI_APPLICATION);

	// SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	if (RegisterClassExA(&WindowClass) == 0) {
		Result = GetLastError();
		MessageBoxA(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	// Window Handle
	gGameWindow = CreateWindowEx(WS_EX_CLIENTEDGE, GAME_NAME "_WINDOWCLASS", "Window Title", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, WindowClass.hInstance, NULL);

	if (gGameWindow == NULL) {
		Result = GetLastError();
		MessageBoxA(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if (GetMonitorInfoA(MonitorFromWindow(gGameWindow, MONITOR_DEFAULTTOPRIMARY), &gMonitorInfo) == 0) {
		Result = ERROR_MONITOR_NO_DESCRIPTOR;
		goto Exit;
	}

	gMonitorWidth = gMonitorInfo.rcMonitor.right - gMonitorInfo.rcMonitor.left;
	gMonitorHeight = gMonitorInfo.rcMonitor.bottom - gMonitorInfo.rcMonitor.top;

	if (SetWindowLongPtrA(gGameWindow, GWL_STYLE, (WS_OVERLAPPEDWINDOW | WS_VISIBLE) & ~WS_OVERLAPPEDWINDOW) == 0) {
		Result = GetLastError();

		goto Exit;
	}

	if (SetWindowPos(gGameWindow, HWND_TOP, gMonitorInfo.rcMonitor.left, gMonitorInfo.rcMonitor.top, gMonitorWidth, gMonitorHeight, SWP_NOOWNERZORDER | SWP_FRAMECHANGED) == 0) {
		Result = GetLastError();

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

void ProcessPlayerInput(void) {
	int16_t EscapeKeyIsDown = GetAsyncKeyState(VK_ESCAPE);

	if (EscapeKeyIsDown) {
		SendMessageA(gGameWindow, WM_CLOSE, 0, 0);
	}
}

void RenderFrameGraphics(void) {

	//memset(gBackBuffer.Memory, 0xFF, (GAME_RES_WIDTH * GAME_RES_HEIGHT) * 4);

	PIXEL32 Pixel = { 0 };

	Pixel.Blue = 0xff;
	Pixel.Green = 0;
	Pixel.Red = 0;
	Pixel.Alpha = 0xff;

	for (int x = 0; x < GAME_RES_HEIGHT * GAME_RES_WIDTH; x++) {
		memcpy((PIXEL32*)gBackBuffer.Memory + x, &Pixel, sizeof(PIXEL32));
	}

	HDC DeviceContext = GetDC(gGameWindow);

	StretchDIBits(DeviceContext,
		0,
		0,
		gMonitorWidth,
		gMonitorHeight,
		0,
		0,
		GAME_RES_WIDTH,
		GAME_RES_HEIGHT,
		gBackBuffer.Memory,
		&gBackBuffer.BitmapInfo,
		DIB_RGB_COLORS,
		SRCCOPY);

	ReleaseDC(gGameWindow, DeviceContext);
}
