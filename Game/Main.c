
#pragma warning(push , 0 )

#include <stdio.h>

#include <Windows.h>

#include <Psapi.h>

#include <emmintrin.h>

#include <stdint.h>		// type definitions

#pragma warning(pop) 

#include <Xinput.h>

#include "Main.h"

#include "Menus.h"

#pragma comment(lib, "Winmm.lib")

#pragma comment(lib, "XInput.lib")

HWND gGameWindow;

BOOL gGameIsRunning;

GAMEBITMAP gBackBuffer;

GAMEBITMAP g6x7Font;

GAMEPERFDATA gPerformanceData;

HERO gPlayer;

BOOL gWindowHasFocus;

REGISTRYPARAMS gRegistryParams;

XINPUT_STATE gGamepadState;

int8_t gGamepadID = -1;

int __stdcall WinMain(_In_ HINSTANCE Instance, _In_opt_ HINSTANCE PreviousInstance, _In_ PSTR CommandLine, _In_ INT CmdShow)
{
	UNREFERENCED_PARAMETER(Instance);
	UNREFERENCED_PARAMETER(PreviousInstance);
	UNREFERENCED_PARAMETER(CommandLine);
	UNREFERENCED_PARAMETER(CmdShow);
	
	MSG Message = { 0 };
	int64_t FrameStart = 0;
	int64_t FrameEnd = 0;
	int64_t ElapsedMicrosecondsAccumulatorRaw = 0;
	int64_t ElapsedMicroseconds = 0;
	int64_t ElapsedMicrosecondsAccumulatorCooked = 0;
	HMODULE NtDllModuleHandle = NULL;
	FILETIME ProcessCreationTime = { 0 };
	FILETIME ProcessExitTime = { 0 };
	int64_t CurrentUserCPUTime = 0;
	int64_t CurrentKernelCPUTime = 0;
	int64_t PreviousUserCPUTime = 0;
	int64_t PreviousKernelCPUTime = 0;
	HANDLE ProcessHandle = GetCurrentProcess();

	if (LoadRegistryParameters() != ERROR_SUCCESS)
	{
		goto Exit;
	}

	if ((NtDllModuleHandle = GetModuleHandleA("ntdll.dll")) == NULL)
	{
		LOGERROR("[%s] Couldn't load ntdll.dll! Error 0x%08lx!", __FUNCTION__, GetLastError());

		MessageBoxA(NULL, "Couldn't load ntdll.dll!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if ((NtQueryTimerResolution = (_NtQueryTimerResolution)GetProcAddress(NtDllModuleHandle, "NtQueryTimerResolution")) == NULL)
	{
		LOGERROR("[%s] Couldn't find NtQueryTimerResolution function in ntdll.dll! GetProcAddress failed! Error 0x%08lx!", __FUNCTION__, GetLastError());

		MessageBoxA(NULL, "Couldn't find NtQueryTimerResolution function in ntdll.dll!!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	NtQueryTimerResolution(&gPerformanceData.MinimumTimerResolution, &gPerformanceData.MaximumTimerResolution, &gPerformanceData.CurrentTimerResolution);

	GetSystemInfo(&gPerformanceData.SystemInfo);

	GetSystemTimeAsFileTime((FILETIME*)&gPerformanceData.PreviousSystemTime);

	if (GameIsAlreadyRunning() == TRUE) // No extra instances of this program!!!
	{
		LOGWARN("[%s] Another instance of this program is already running!", __FUNCTION__);

		MessageBoxA(NULL, "Another instance of this program is already running!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if (timeBeginPeriod(1) == TIMERR_NOCANDO)
	{
		LOGERROR("[%s] Failed to set global timer resolution!", __FUNCTION__);

		MessageBoxA(NULL, "Failed to set global timer resolution!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if (SetPriorityClass(ProcessHandle, HIGH_PRIORITY_CLASS) == 0)
	{
		LOGERROR("[%s] Failed to set process priority! Error 0x%08lx!", __FUNCTION__, GetLastError());

		MessageBoxA(NULL, "Failed to set process priority!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST) == 0)
	{
		LOGERROR("[%s] Failed to set thread priority! Error 0x%08lx!", __FUNCTION__, GetLastError());

		MessageBoxA(NULL, "Failed to set thread priority!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if (CreateMainGameWindow() != ERROR_SUCCESS)
	{
		LOGERROR("[%s] CreateMainGameWindow failed!", __FUNCTION__);

		goto Exit;
	}

	if (Load32BppBitmapFromFile(".\\Assets\\6x7Font.bmpx", &g6x7Font) != ERROR_SUCCESS)
	{
		LOGERROR("[%s] Load32BppBitmapFromFile failed!", __FUNCTION__);

		MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	QueryPerformanceFrequency((LARGE_INTEGER*)&gPerformanceData.PerfFrequency);

	gPerformanceData.DisplayDebugInfo = TRUE;

	gBackBuffer.BitmapInfo.bmiHeader.biSize = sizeof(gBackBuffer.BitmapInfo.bmiHeader);
	gBackBuffer.BitmapInfo.bmiHeader.biWidth = GAME_RES_WIDTH;
	gBackBuffer.BitmapInfo.bmiHeader.biHeight = GAME_RES_HEIGHT;
	gBackBuffer.BitmapInfo.bmiHeader.biBitCount = GAME_BPP;
	gBackBuffer.BitmapInfo.bmiHeader.biCompression = BI_RGB;
	gBackBuffer.BitmapInfo.bmiHeader.biPlanes = 1;
	gBackBuffer.Memory = VirtualAlloc(NULL, GAME_DRAWING_AREA_MEMORY_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	if (gBackBuffer.Memory == NULL)
	{
		LOGERROR("[%s] Failed to allocate memory for drawing surface! Error 0x%08lx!", __FUNCTION__, GetLastError());

		// If memory fails to be allocated, 0 will be returned
		MessageBoxA(NULL, "Failed to allocate memory for drawing surface!", "Error!", MB_ICONERROR | MB_OK);
		goto Exit;
	}

	memset(gBackBuffer.Memory, 0x7f, GAME_DRAWING_AREA_MEMORY_SIZE);

	if (InitializeHero() != ERROR_SUCCESS)
	{
		LOGERROR("[%s] Failed to initialize hero!", __FUNCTION__);

		MessageBoxA(NULL, "Failed to initialize hero!", "Error!", MB_ICONERROR | MB_OK);
		goto Exit;
	}

	// Messages
	gGameIsRunning = TRUE;

	while (gGameIsRunning == TRUE) // Game Loop
	{
		QueryPerformanceCounter((LARGE_INTEGER*)&FrameStart);

		while (PeekMessageA(&Message, gGameWindow, 0, 0, PM_REMOVE)) // Message Queue Loop
		{
			DispatchMessageA(&Message);
		}

		ProcessHeroInput();

		RenderFrameGraphics();

		
		QueryPerformanceCounter((LARGE_INTEGER*)&FrameEnd);

		ElapsedMicroseconds = FrameEnd - FrameStart;

		ElapsedMicroseconds *= 1000000;

		ElapsedMicroseconds /= gPerformanceData.PerfFrequency;

		gPerformanceData.TotalFramesRendered++;

		ElapsedMicrosecondsAccumulatorRaw += ElapsedMicroseconds;

		while (ElapsedMicroseconds < TARGET_MICROSECONDS_PER_FRAME) // Need to figure out what is wrong 
		{
			ElapsedMicroseconds = FrameEnd - FrameStart;

			ElapsedMicroseconds *= 1000000;

			ElapsedMicroseconds /= gPerformanceData.PerfFrequency;

			QueryPerformanceCounter((LARGE_INTEGER*)&FrameEnd);

			if (ElapsedMicroseconds < TARGET_MICROSECONDS_PER_FRAME * 0.75f)
			{
				Sleep(1);
			}
		}

		ElapsedMicrosecondsAccumulatorCooked += ElapsedMicroseconds;

		if (gPerformanceData.TotalFramesRendered % CALCULATE_AVG_FPS_EVERY_X_FRAMES == 0)
		{
			GetSystemTimeAsFileTime((FILETIME*)&gPerformanceData.CurrentSystemTime);

			FindFirstConnectedGamepad();

			GetProcessTimes(ProcessHandle,
				&ProcessCreationTime,
				&ProcessExitTime,
				(FILETIME*)&CurrentKernelCPUTime,
				(FILETIME*)&CurrentUserCPUTime);

			gPerformanceData.CPUPercent = (double)CurrentKernelCPUTime - (double)PreviousKernelCPUTime + \
				((double)CurrentUserCPUTime - (double)PreviousUserCPUTime);

			gPerformanceData.CPUPercent /= (gPerformanceData.CurrentSystemTime - gPerformanceData.PreviousSystemTime);

			gPerformanceData.CPUPercent /= gPerformanceData.SystemInfo.dwNumberOfProcessors;

			gPerformanceData.CPUPercent *= 100;

			GetProcessHandleCount(ProcessHandle, &gPerformanceData.HandleCount);

			K32GetProcessMemoryInfo(ProcessHandle, (PROCESS_MEMORY_COUNTERS*)&gPerformanceData.MemInfo, sizeof(gPerformanceData.MemInfo));
			
			gPerformanceData.RawFPSAverage = 1.0f / ((ElapsedMicrosecondsAccumulatorRaw / CALCULATE_AVG_FPS_EVERY_X_FRAMES) * 0.000001f);

			gPerformanceData.CookedFPSAverage = 1.0f / ((ElapsedMicrosecondsAccumulatorCooked / CALCULATE_AVG_FPS_EVERY_X_FRAMES) * 0.000001f);

			ElapsedMicrosecondsAccumulatorRaw = 0;

			ElapsedMicrosecondsAccumulatorCooked = 0;

			PreviousKernelCPUTime = CurrentKernelCPUTime;

			PreviousUserCPUTime = CurrentUserCPUTime;

			gPerformanceData.PreviousSystemTime = gPerformanceData.CurrentSystemTime;
		}
	}

Exit:
	return 0;
}


LRESULT CALLBACK MainWindowProc(_In_ HWND WindowHandle, _In_  UINT Message, _In_  WPARAM WParam, _In_  LPARAM LParam)
{

	LRESULT Result = 0;

	switch (Message)
	{
		case WM_CLOSE:
		{
			gGameIsRunning = FALSE;
			PostQuitMessage(0); // Passes a message with 0 which ends the program
			break;
		}
		case WM_ACTIVATE:
		{
			if (WParam == 0)
			{
				// Our window has lost focus
				gWindowHasFocus = FALSE;
			}
			else
			{
				ShowCursor(FALSE);
				gWindowHasFocus = TRUE;
			}
			break;
		}
		default:
		{
			return DefWindowProc(WindowHandle, Message, WParam, LParam);
		}
	}
	return Result;
}

DWORD CreateMainGameWindow(void)
{
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

	if (RegisterClassExA(&WindowClass) == 0)
	{
		Result = GetLastError();

		LOGERROR("[%s] RegisterClassExA failed! Error 0x%08lx", __FUNCTION__, Result);

		MessageBoxA(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	// Window Handle
	gGameWindow = CreateWindowExA(0, GAME_NAME "_WINDOWCLASS", "Window Title", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, WindowClass.hInstance, NULL);

	if (gGameWindow == NULL)
	{
		Result = GetLastError();

		LOGERROR("[%s] CreateWindowExA failed! Error 0x%08lx", __FUNCTION__, Result);

		MessageBoxA(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	gPerformanceData.MonitorInfo.cbSize = sizeof(MONITORINFO);

	if (GetMonitorInfoA(MonitorFromWindow(gGameWindow, MONITOR_DEFAULTTOPRIMARY), &gPerformanceData.MonitorInfo) == 0)
	{
		Result = ERROR_MONITOR_NO_DESCRIPTOR;
		
		LOGERROR("[%s] GetMonitorInfoA(MonitorFromWindow()) failed! Error 0x%08lx", __FUNCTION__, Result);

		goto Exit;
	}

	gPerformanceData.MonitorWidth = gPerformanceData.MonitorInfo.rcMonitor.right - gPerformanceData.MonitorInfo.rcMonitor.left;
	gPerformanceData.MonitorHeight = gPerformanceData.MonitorInfo.rcMonitor.bottom - gPerformanceData.MonitorInfo.rcMonitor.top;

	if (SetWindowLongPtrA(gGameWindow, GWL_STYLE, (WS_OVERLAPPEDWINDOW | WS_VISIBLE) & ~WS_OVERLAPPEDWINDOW) == 0)
	{
		Result = GetLastError();

		LOGERROR("[%s] SetWindowLongPtrA failed! Error 0x%08lx", __FUNCTION__, Result);

		goto Exit;
	}

	if (SetWindowPos(gGameWindow,
		HWND_TOP,
		gPerformanceData.MonitorInfo.rcMonitor.left,
		gPerformanceData.MonitorInfo.rcMonitor.top,
		gPerformanceData.MonitorWidth,
		gPerformanceData.MonitorHeight,
		SWP_NOOWNERZORDER | SWP_FRAMECHANGED) == 0)
	{
		Result = GetLastError();

		LOGERROR("[%s] SetWindowPos failed! Error 0x%08lx", __FUNCTION__, Result);

		goto Exit;
	}

Exit:
	return Result;
}

BOOL GameIsAlreadyRunning(void)
{
	HANDLE Mutex = NULL;
	Mutex = CreateMutexA(NULL, FALSE, GAME_NAME "_GameMutex");
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		return TRUE;
	}
	else 
	{
		return FALSE;
	}
}

void ProcessHeroInput(void)
{
	if (gWindowHasFocus == FALSE)
	{
		goto Exit;
	}

	int16_t EscapeKeyIsDown = GetAsyncKeyState(VK_ESCAPE);

	int16_t DebugKeyIsDown = GetAsyncKeyState(VK_F1);

	int16_t LeftKeyIsDown = GetAsyncKeyState(VK_LEFT) | GetAsyncKeyState('A');

	int16_t RightKeyIsDown = GetAsyncKeyState(VK_RIGHT) | GetAsyncKeyState('D');

	int16_t UpKeyIsDown = GetAsyncKeyState(VK_UP) | GetAsyncKeyState('W');

	int16_t DownKeyIsDown = GetAsyncKeyState(VK_DOWN) | GetAsyncKeyState('S');

	static int16_t DebugKeyWasDown = 0;

	static int16_t LeftKeyWasDown = 0;

	static int16_t RightKeyWasDown = 0;

	static int16_t UpKeyWasDown = 0;

	static int16_t DownKeyWasDown = 0;

	if (gGamepadID > -1)
	{
		if (XInputGetState(gGamepadID, &gGamepadState) == ERROR_SUCCESS)
		{
			EscapeKeyIsDown |= gGamepadState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK;

			LeftKeyIsDown	|= gGamepadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT;

			RightKeyIsDown	|= gGamepadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

			UpKeyIsDown		|= gGamepadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP;

			DownKeyIsDown	|= gGamepadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
		}
	}

	if (EscapeKeyIsDown)
	{
		SendMessageA(gGameWindow, WM_CLOSE, 0, 0);
	}

	if (DebugKeyIsDown && !DebugKeyWasDown)
	{
		gPerformanceData.DisplayDebugInfo = !gPerformanceData.DisplayDebugInfo;
	}

	if (!gPlayer.MovementRemaining)
	{
		if (DownKeyIsDown)
		{
			if (gPlayer.ScreenPosY < GAME_RES_HEIGHT - 16)
			{
				gPlayer.MovementRemaining = 16;

				gPlayer.Direction = DOWN;
			}
		}
		else if (LeftKeyIsDown)
		{
			if (gPlayer.ScreenPosX > 0)
			{
				gPlayer.MovementRemaining = 16;

				gPlayer.Direction = LEFT;
			}
		}
		else if (RightKeyIsDown)
		{
			if (gPlayer.ScreenPosX < GAME_RES_WIDTH - 16)
			{
				gPlayer.MovementRemaining = 16;

				gPlayer.Direction = RIGHT;
			}
		}
		else if (UpKeyIsDown)
		{
			if (gPlayer.ScreenPosY > 0)
			{
				gPlayer.MovementRemaining = 16;

				gPlayer.Direction = UP;
			}
		}
	}
	else
	{
		gPlayer.MovementRemaining--;

		if (gPlayer.Direction == DOWN)
		{
			gPlayer.ScreenPosY++;
		}
		else if (gPlayer.Direction == LEFT)
		{
			gPlayer.ScreenPosX--;
		}
		else if (gPlayer.Direction == RIGHT)
		{
			gPlayer.ScreenPosX++;
		}
		else if (gPlayer.Direction == UP)
		{
			gPlayer.ScreenPosY--;
		}

		switch (gPlayer.MovementRemaining)
		{
			case 16:
			{
				gPlayer.SpriteIndex = 0;

				break;
			}
			case 12:
			{
				gPlayer.SpriteIndex = 1;

				break;
			}
			case 8:
			{
				gPlayer.SpriteIndex = 0;

				break;
			}
			case 4:
			{
				gPlayer.SpriteIndex = 2;

				break;
			}
			case 0:
			{
				gPlayer.SpriteIndex = 0;

				break;
			}

			default:
			{
				// assert
			}
		}

		DebugKeyWasDown = DebugKeyIsDown;

		LeftKeyWasDown = LeftKeyIsDown;

		RightKeyWasDown = RightKeyIsDown;

		UpKeyWasDown = UpKeyIsDown;

		DownKeyWasDown = DownKeyIsDown;

	Exit:
		return;
	}
}

DWORD Load32BppBitmapFromFile(_In_ char* FileName, _Inout_ GAMEBITMAP* GameBitMap)
{
	DWORD Error = ERROR_SUCCESS;
	
	HANDLE FileHandle = INVALID_HANDLE_VALUE;

	//WORD BitmapHeader = 0;

	DWORD PixelDataOffset = 0x36;

	DWORD NumberOfBytesRead;

	if ((FileHandle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
	{
		Error = GetLastError();
		goto Exit;
	}

	//if (ReadFile(FileHandle, &BitmapHeader, 2, &NumberOfBytesRead, NULL) == 0)
	//{
	//	Error = GetLastError();
	//	goto Exit;
	//}

	//if (BitmapHeader != 0x4d42) // "BM" Backwards
	//{
	//	Error = ERROR_FILE_INVALID;

	//	goto Exit;
	//}

	/*if (SetFilePointer(FileHandle, 0xA, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
	{
		Error = GetLastError();

		goto Exit;
	}

	if (ReadFile(FileHandle, &PixelDataOffset, sizeof(DWORD), &NumberOfBytesRead, NULL) == 0)
	{
		Error = GetLastError();

		goto Exit;
	}*/

	if (SetFilePointer(FileHandle, 0xE, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
	{
		Error = GetLastError();

		goto Exit;
	}

	// Reads the BITMAPHEADERINFO FROM FILE
	if (ReadFile(FileHandle, &GameBitMap->BitmapInfo.bmiHeader, sizeof(BITMAPINFOHEADER), &NumberOfBytesRead, NULL) == 0)
	{
		Error = GetLastError();

		goto Exit;
	}

	// Allocate the memory for our bitmap
	if ((GameBitMap->Memory = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, GameBitMap->BitmapInfo.bmiHeader.biSizeImage)) == NULL)
	{
		Error = ERROR_NOT_ENOUGH_MEMORY;

		goto Exit;
	}

	if (SetFilePointer(FileHandle, PixelDataOffset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
	{
		Error = GetLastError();

		goto Exit;
	}

	// Read in the bitmap data from file
	if (ReadFile(FileHandle, GameBitMap->Memory, GameBitMap->BitmapInfo.bmiHeader.biSizeImage, &NumberOfBytesRead, NULL) == 0)
	{
		Error = GetLastError();

		goto Exit;
	}

Exit:

	if (FileHandle && FileHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(FileHandle);
	}

	if (Error == ERROR_SUCCESS)
	{
		LOGINFO("[%s] Loading successful: %s", __FUNCTION__, FileName);
	}
	else
	{
		LOGERROR("[%s] Loading failed: %s! Error 0x08lx!", __FUNCTION__, FileName, Error);
	}

	return Error;
}

DWORD InitializeHero(void)
{
	DWORD Error = ERROR_SUCCESS;

	gPlayer.ScreenPosX = 32;

	gPlayer.ScreenPosY = 32;

	gPlayer.CurrentArmor = SUIT_0;

	gPlayer.Direction = DOWN;

	gPlayer.MovementRemaining = 0;

	if ((Error = Load32BppBitmapFromFile(".\\Assets\\Hero_Suit0_Down_Standing.bmpx", &gPlayer.Sprite[SUIT_0][FACING_DOWN_0])) != ERROR_SUCCESS)
	{
		MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if ((Error = Load32BppBitmapFromFile(".\\Assets\\Hero_Suit0_Down_Walk1.bmpx", &gPlayer.Sprite[SUIT_0][FACING_DOWN_1])) != ERROR_SUCCESS)
	{
		MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if ((Error = Load32BppBitmapFromFile(".\\Assets\\Hero_Suit0_Down_Walk2.bmpx", &gPlayer.Sprite[SUIT_0][FACING_DOWN_2])) != ERROR_SUCCESS)
	{
		MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if ((Error = Load32BppBitmapFromFile(".\\Assets\\Hero_Suit0_Left_Standing.bmpx", &gPlayer.Sprite[SUIT_0][FACING_LEFT_0])) != ERROR_SUCCESS)
	{
		MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if ((Error = Load32BppBitmapFromFile(".\\Assets\\Hero_Suit0_Left_Walk1.bmpx", &gPlayer.Sprite[SUIT_0][FACING_LEFT_1])) != ERROR_SUCCESS)
	{
		MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if ((Error = Load32BppBitmapFromFile(".\\Assets\\Hero_Suit0_Left_Walk2.bmpx", &gPlayer.Sprite[SUIT_0][FACING_LEFT_2])) != ERROR_SUCCESS)
	{
		MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if ((Error = Load32BppBitmapFromFile(".\\Assets\\Hero_Suit0_Right_Standing.bmpx", &gPlayer.Sprite[SUIT_0][FACING_RIGHT_0])) != ERROR_SUCCESS)
	{
		MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if ((Error = Load32BppBitmapFromFile(".\\Assets\\Hero_Suit0_Right_Walk1.bmpx", &gPlayer.Sprite[SUIT_0][FACING_RIGHT_1])) != ERROR_SUCCESS)
	{
		MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if ((Error = Load32BppBitmapFromFile(".\\Assets\\Hero_Suit0_Right_Walk2.bmpx", &gPlayer.Sprite[SUIT_0][FACING_RIGHT_2])) != ERROR_SUCCESS)
	{
		MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if ((Error = Load32BppBitmapFromFile(".\\Assets\\Hero_Suit0_Up_Standing.bmpx", &gPlayer.Sprite[SUIT_0][FACING_UPWARD_0])) != ERROR_SUCCESS)
	{
		MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if ((Error = Load32BppBitmapFromFile(".\\Assets\\Hero_Suit0_Up_Walk1.bmpx", &gPlayer.Sprite[SUIT_0][FACING_UPWARD_1])) != ERROR_SUCCESS)
	{
		MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if ((Error = Load32BppBitmapFromFile(".\\Assets\\Hero_Suit0_Up_Walk2.bmpx", &gPlayer.Sprite[SUIT_0][FACING_UPWARD_2])) != ERROR_SUCCESS)
	{
		MessageBoxA(NULL, "Load32BppBitmapFromFile failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

Exit:
	return Error;
}

void BlitStringToBuffer(_In_ char* String, _In_ GAMEBITMAP* FontSheet, _In_ PIXEL32* Color, _In_ uint16_t x, _In_ uint16_t y)
{
	uint16_t CharWidth = (uint16_t)FontSheet->BitmapInfo.bmiHeader.biWidth / FONT_SHEET_CHARACTERS_PER_ROW;

	uint16_t CharHeight = (uint16_t)FontSheet->BitmapInfo.bmiHeader.biHeight;

	uint16_t BytesPerCharacter = (CharWidth * CharHeight * (uint16_t)(FontSheet->BitmapInfo.bmiHeader.biBitCount / 8));

	uint16_t StringLength = (uint16_t)strlen(String);

	GAMEBITMAP StringBitmap = { 0 };

	StringBitmap.BitmapInfo.bmiHeader.biBitCount = GAME_BPP;

	StringBitmap.BitmapInfo.bmiHeader.biHeight = CharHeight;

	StringBitmap.BitmapInfo.bmiHeader.biWidth = CharWidth * StringLength;

	StringBitmap.BitmapInfo.bmiHeader.biPlanes = 1;

	StringBitmap.BitmapInfo.bmiHeader.biCompression = BI_RGB;

	StringBitmap.Memory = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (size_t)BytesPerCharacter * StringLength);

	for (int Character = 0; Character < StringLength; Character++)
	{
		int StartingFontSheetPixel = 0;

		int FontSheetOffset = 0;

		int StringBitmapOffset = 0;

		PIXEL32 FontSheetPixel = { 0 };

		// Giant Switch statement for every character we could want to use
		switch (String[Character])
		{
			case 'A':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth;

				break;
			}
			case 'B':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + CharWidth;

				break;
			}
			case 'C':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 2);

				break;
			}
			case 'D':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 3);

				break;
			}
			case 'E':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 4);

				break;
			}
			case 'F':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 5);

				break;
			}
			case 'G':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 6);

				break;
			}
			case 'H':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 7);

				break;
			}
			case 'I':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 8);

				break;
			}
			case 'J':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 9);

				break;
			}
			case 'K':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 10);

				break;
			}
			case 'L':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 11);

				break;
			}
			case 'M':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 12);

				break;
			}
			case 'N':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 13);

				break;
			}
			case 'O':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 14);

				break;
			}
			case 'P':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 15);

				break;
			}
			case 'Q':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 16);

				break;
			}
			case 'R':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 17);

				break;
			}
			case 'S':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 18);

				break;
			}
			case 'T':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 19);

				break;
			}
			case 'U':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 20);

				break;
			}
			case 'V':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 21);

				break;
			}
			case 'W':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 22);

				break;
			}
			case 'X':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 23);

				break;
			}
			case 'Y':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 24);

				break;
			}
			case 'Z':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 25);

				break;
			}
			case 'a':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 26);

				break;
			}
			case 'b':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 27);

				break;
			}
			case 'c':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 28);

				break;
			}
			case 'd':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 29);

				break;
			}
			case 'e':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 30);

				break;
			}
			case 'f':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 31);

				break;
			}
			case 'g':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 32);

				break;
			}
			case 'h':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 33);

				break;
			}
			case 'i':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 34);

				break;
			}
			case 'j':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 35);

				break;
			}
			case 'k':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 36);

				break;
			}
			case 'l':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 37);

				break;
			}
			case 'm':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 38);

				break;
			}
			case 'n':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 39);

				break;
			}
			case 'o':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 40);

				break;
			}
			case 'p':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 41);

				break;
			}
			case 'q':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 42);

				break;
			}
			case 'r':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 43);

				break;
			}
			case 's':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 44);

				break;
			}
			case 't':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 45);

				break;
			}
			case 'u':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 46);

				break;
			}
			case 'v':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 47);

				break;
			}
			case 'w':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 48);

				break;
			}
			case 'x':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 49);

				break;
			}
			case 'y':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 50);

				break;
			}
			case 'z':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 51);

				break;
			}
			case '0':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 52);

				break;
			}
			case '1':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 53);

				break;
			}
			case '2':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 54);

				break;
			}
			case '3':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 55);

				break;
			}
			case '4':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 56);

				break;
			}
			case '5':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 57);

				break;
			}
			case '6':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 58);

				break;
			}
			case '7':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 59);

				break;
			}
			case '8':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 60);

				break;
			}
			case '9':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 61);

				break;
			}
			case '`':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 62);

				break;
			}
			case '~':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 63);

				break;
			}
			case '!':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 64);

				break;
			}
			case '@':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 65);

				break;
			}
			case '#':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 66);

				break;
			}
			case '$':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 67);

				break;
			}
			case '%':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 68);

				break;
			}
			case '^':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 69);

				break;
			}
			case '&':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 70);

				break;
			}
			case '*':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 71);

				break;
			}
			case '(':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 72);

				break;
			}
			case ')':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 73);

				break;
			}
			case '-':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 74);

				break;
			}
			case '=':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 75);

				break;
			}
			case '_':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 76);

				break;
			}
			case '+':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 77);

				break;
			}
			case '\\':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 78);

				break;
			}
			case '|':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 79);

				break;
			}
			case '[':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 80);

				break;
			}
			case ']':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 81);

				break;
			}
			case '{':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 82);

				break;
			}
			case '}':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 83);

				break;
			}
			case ';':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 84);

				break;
			}
			case '\'':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 85);

				break;
			}
			case ':':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 86);

				break;
			}
			case '"':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 87);

				break;
			}
			case ',':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 88);

				break;
			}
			case '<':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 89);

				break;
			}
			case '>':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 90);

				break;
			}
			case '.':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 91);

				break;
			}
			case '/':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 92);

				break;
			}
			case '?':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 93);

				break;
			}
			case ' ':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 94);

				break;
			}
			case '»':
			{
				// Github did not preserve this character and instead turned it into a "question mark inside of a diamond" character. 
				// It is extended ASCII 0xBB, decimal 187, right double-angle quotes, which I use as an arrow to mark menu items.

				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 95);

				break;
			}
			case '«':
			{
				// Github did not preserve this character and instead turned it into a "question mark inside of a diamond" character. 
				// It is extended ASCII 0xAB, decimal 171, left double-angle quotes, which I use as an arrow to mark menu items.

				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 96);

				break;
			}
			case '\xf2':
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 97);

				break;
			}
			default:
			{
				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 93);
			}
		}

		for (int YPixel = 0; YPixel < CharHeight; YPixel++)
		{
			for (int XPixel = 0; XPixel < CharWidth; XPixel++)
			{
				FontSheetOffset = StartingFontSheetPixel + XPixel - (FontSheet->BitmapInfo.bmiHeader.biWidth * YPixel);

				StringBitmapOffset = (Character * CharWidth) + ((StringBitmap.BitmapInfo.bmiHeader.biWidth * StringBitmap.BitmapInfo.bmiHeader.biHeight) - \
					StringBitmap.BitmapInfo.bmiHeader.biWidth) + XPixel - (StringBitmap.BitmapInfo.bmiHeader.biWidth) * YPixel;

				memcpy_s(&FontSheetPixel, sizeof(PIXEL32), (PIXEL32*)FontSheet->Memory + FontSheetOffset, sizeof(PIXEL32));

				FontSheetPixel.Red = Color->Red;

				FontSheetPixel.Green = Color->Green;

				FontSheetPixel.Blue = Color->Blue;

				memcpy_s((PIXEL32*)StringBitmap.Memory + StringBitmapOffset, sizeof(PIXEL32), &FontSheetPixel, sizeof(PIXEL32));
			}
		}
	}

	Blit32BppBitmapToBuffer(&StringBitmap, x, y);

	if (StringBitmap.Memory)
	{
		HeapFree(GetProcessHeap(), 0, StringBitmap.Memory);
	}
}

void RenderFrameGraphics(void)
{

#ifdef SIMD
	__m128i QuadPixel = { 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff };

	ClearScreen(&QuadPixel);
#else
	PIXEL32 Pixel = { 0x7f, 0x00, 0x00, 0xff };

	ClearScreen(&Pixel);
	
#endif 

	//PIXEL32 Green = { 0x00, 0xff, 0x00, 0xff };

	// BlitStringToBuffer("GAME OVER", &g6x7Font, &Green, 60, 60);

	Blit32BppBitmapToBuffer(&gPlayer.Sprite[gPlayer.CurrentArmor][gPlayer.Direction + gPlayer.SpriteIndex],
		gPlayer.ScreenPosX,
		gPlayer.ScreenPosY);

	HDC DeviceContext = GetDC(gGameWindow);

	if (gPerformanceData.DisplayDebugInfo == TRUE)
	{
		DrawDebugInfo();
	}

	StretchDIBits(DeviceContext,
		0,
		0,
		gPerformanceData.MonitorWidth,
		gPerformanceData.MonitorHeight,
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

#ifdef SIMD
__forceinline void ClearScreen(_In_ __m128i* Color)
{	
	for (int x = 0; x < GAME_RES_WIDTH * GAME_RES_HEIGHT; x += 4)
	{
		_mm_store_si128((PIXEL32*)gBackBuffer.Memory + x, *Color);
	}
}

#else
__forceinline void ClearScreen(_In_ PIXEL32* Pixel)
{
	for (int x = 0; x < GAME_RES_WIDTH * GAME_RES_HEIGHT; x++)
	{
		// memcpy((PIXEL32*)gBackBuffer.Memory + x, Pixel, sizeof(PIXEL32)); // This version is the unsecured version of memcpy but is WAY faster
		memcpy_s((PIXEL32*)gBackBuffer.Memory + x, sizeof(PIXEL32), Pixel, sizeof(PIXEL32));
	}
}
#endif

void Blit32BppBitmapToBuffer(_In_ GAMEBITMAP* GameBitMap, _In_ uint16_t x, _In_ uint16_t y)
{
	int32_t StartingScreenPixel = ((GAME_RES_WIDTH * GAME_RES_HEIGHT) - GAME_RES_WIDTH - (GAME_RES_WIDTH * y) + x);

	int32_t StartingBitmapPixel = ((GameBitMap->BitmapInfo.bmiHeader.biWidth * GameBitMap->BitmapInfo.bmiHeader.biHeight) - \
		GameBitMap->BitmapInfo.bmiHeader.biWidth);
	
	int32_t MemoryOffset = 0;

	int32_t BitmapOffset = 0;

	PIXEL32 BitmapPixel = { 0 };

	// PIXEL32 BackgroundPixel = { 0 };

	for (int16_t YPixel = 0; YPixel < GameBitMap->BitmapInfo.bmiHeader.biHeight; YPixel++)
	{
		for (int16_t XPixel = 0; XPixel < GameBitMap->BitmapInfo.bmiHeader.biWidth; XPixel++)
		{
			MemoryOffset = StartingScreenPixel + XPixel - (GAME_RES_WIDTH * YPixel);

			BitmapOffset = StartingBitmapPixel + XPixel - (GameBitMap->BitmapInfo.bmiHeader.biWidth * YPixel);

			memcpy_s(&BitmapPixel, sizeof(PIXEL32), (PIXEL32*)GameBitMap->Memory + BitmapOffset, sizeof(PIXEL32));

			if (BitmapPixel.Alpha == 255)
			{
				memcpy_s((PIXEL32*)gBackBuffer.Memory + MemoryOffset, sizeof(PIXEL32), &BitmapPixel, sizeof(PIXEL32));
			}
		}
	}

}

DWORD LoadRegistryParameters(void)
{
	DWORD Result = ERROR_SUCCESS;

	HKEY RegKey = NULL;

	DWORD RegDisposition = 0;

	DWORD RegBytesRead = sizeof(DWORD);

	Result = RegCreateKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\" GAME_NAME, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &RegKey, &RegDisposition);

	if (Result != ERROR_SUCCESS)
	{
		LOGERROR("[%s] RegCreatedKey failed with error code 0x%08lx!", __FUNCTION__, Result);

		//LogMessageA(LOG_LEVEL_ERROR, "[%s] RegCreatedKey failed with error code 0x%08lx!", __FUNCTION__, Result);

		goto Exit;
	}

	if (RegDisposition == REG_CREATED_NEW_KEY)
	{
		LOGINFO("[%s] Registry key did not exist; created new key HKCU\\SOFTWARE\\%s.", __FUNCTION__, GAME_NAME);
	}
	else
	{
		LOGINFO("[%s] Opened existing registry key HKCU\\SOFTWARE\\%s.", __FUNCTION__, GAME_NAME);
	}

	Result = RegGetValueA(RegKey, NULL, "LogLevel", RRF_RT_DWORD, NULL, (BYTE*)&gRegistryParams.LogLevel, &RegBytesRead);

	if (Result != ERROR_SUCCESS)
	{
		if (Result == ERROR_FILE_NOT_FOUND)
		{
			Result = ERROR_SUCCESS;

			LOGINFO("[%s] Registry value 'LogLevel' not found. Using default of 0. (LOG_LEVEL_NONE)", __FUNCTION__);

			gRegistryParams.LogLevel = LL_NONE;
		}
		else
		{
			LOGERROR("[%s] Failed to read the 'LogLevel' registry value! Error 0x%08lx!", __FUNCTION__, Result);

			goto Exit;
		}
	}

	LOGINFO("[%s] Log Level is %d.", __FUNCTION__, gRegistryParams.LogLevel);

Exit:

	if (RegKey)
	{
		RegCloseKey(RegKey);
	}

	return Result;
}

void LogMessageA(_In_ LOGLEVEL LogLevel, _In_ char* Message, _In_ ...)
{
	size_t MessageLength = strlen(Message);

	SYSTEMTIME Time = { 0 };

	HANDLE LogFileHandle = INVALID_HANDLE_VALUE;

	DWORD EndOfFile = 0;

	DWORD NumberOfBytesWritten = 0;

	char DateTimeString[96] = { 0 };

	char SeverityString[8] = { 0 };

	char FormattedString[4096] = { 0 };

	int Error = 0;

	if (gRegistryParams.LogLevel < LogLevel)
	{
		return;
	}

	if (MessageLength < 1 || MessageLength > 4096)
	{
		ASSERT(FALSE, "Message was either too short or too long!");
	}

	switch (LogLevel)
	{
		case LL_NONE:
		{
			return;
		}
		case LL_INFO:
		{
			strcpy_s(SeverityString, sizeof(SeverityString), "[INFO] ");

			break;
		}
		case LL_WARN:
		{
			strcpy_s(SeverityString, sizeof(SeverityString), "[WARN] ");

			break;
		}
		case LL_ERROR:
		{
strcpy_s(SeverityString, sizeof(SeverityString), "[ERROR]");

break;
		}
		case LL_DEBUG:
		{
			strcpy_s(SeverityString, sizeof(SeverityString), "[DEBUG]");

			break;
		}
		default:
		{
			ASSERT(FALSE, "Unrecognized log level!");
		}
	}

	GetLocalTime(&Time);

	va_list ArgPointer = NULL;

	va_start(ArgPointer, Message);

	_vsnprintf_s(FormattedString, sizeof(FormattedString), _TRUNCATE, Message, ArgPointer);

	va_end(ArgPointer);

	Error = _snprintf_s(DateTimeString, sizeof(DateTimeString), _TRUNCATE, "\r\n[%02u/%02u/%u %02u:%02u:%02u.%03u]",
		Time.wMonth, Time.wDay, Time.wYear, Time.wHour, Time.wMinute, Time.wSecond, Time.wMilliseconds);

	if ((LogFileHandle = CreateFileA(LOG_FILE_NAME, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
	{
		ASSERT(FALSE, "Failed to access log file!");
	}

	EndOfFile = SetFilePointer(LogFileHandle, 0, NULL, FILE_END);

	WriteFile(LogFileHandle, DateTimeString, (DWORD)strlen(DateTimeString), &NumberOfBytesWritten, NULL);

	WriteFile(LogFileHandle, SeverityString, (DWORD)strlen(SeverityString), &NumberOfBytesWritten, NULL);

	WriteFile(LogFileHandle, FormattedString, (DWORD)strlen(FormattedString), &NumberOfBytesWritten, NULL);

	if (LogFileHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(LogFileHandle);
	}
}

void DrawDebugInfo(void)
{
	PIXEL32 White = { 0xff, 0xff, 0xff, 0xff };

	char DebugTextBuffer[64] = { 0 };

	sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "FPSRaw:  %.01f", gPerformanceData.RawFPSAverage);

	BlitStringToBuffer(DebugTextBuffer, &g6x7Font, &White, 0, 0);

	sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "FPSCookd:%.01f", gPerformanceData.CookedFPSAverage);

	BlitStringToBuffer(DebugTextBuffer, &g6x7Font, &White, 0, 8);

	sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "MinTimer:%.02f", gPerformanceData.MinimumTimerResolution / 10000.0f);

	BlitStringToBuffer(DebugTextBuffer, &g6x7Font, &White, 0, 16);

	sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "MaxTimer:%.02f", gPerformanceData.MaximumTimerResolution / 10000.0f);

	BlitStringToBuffer(DebugTextBuffer, &g6x7Font, &White, 0, 24);

	sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "CurTimer:%.02f", gPerformanceData.CurrentTimerResolution / 10000.0f);

	BlitStringToBuffer(DebugTextBuffer, &g6x7Font, &White, 0, 32);

	sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "Handles: %lu", gPerformanceData.HandleCount);

	BlitStringToBuffer(DebugTextBuffer, &g6x7Font, &White, 0, 40);

	sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "Memory:  %zu kB", gPerformanceData.MemInfo.PrivateUsage / 1000);

	BlitStringToBuffer(DebugTextBuffer, &g6x7Font, &White, 0, 48);

	sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "CPU:     %.02f %%", gPerformanceData.CPUPercent);

	BlitStringToBuffer(DebugTextBuffer, &g6x7Font, &White, 0, 56);

	sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "FramesT: %llu", gPerformanceData.TotalFramesRendered);

	BlitStringToBuffer(DebugTextBuffer, &g6x7Font, &White, 0, 64);

	sprintf_s(DebugTextBuffer, _countof(DebugTextBuffer), "ScreenXY:%d,%d", gPlayer.ScreenPosX, gPlayer.ScreenPosY);

	BlitStringToBuffer(DebugTextBuffer, &g6x7Font, &White, 0, 72);
}

void FindFirstConnectedGamepad(void)
{
	gGamepadID = -1;

	for (int8_t GamepadIndex = 0; GamepadIndex < XUSER_MAX_COUNT && gGamepadID == -1; GamepadIndex++)
	{
		XINPUT_STATE State = { 0 };

		if (XInputGetState(GamepadIndex, &State) == ERROR_SUCCESS)
		{
			gGamepadID = GamepadIndex;
		}
	}
}

void MenuItem_TitleScreen_Resume(void)
{

}

void MenuItem_TitleScreen_StartNew(void)
{

}

void MenuItem_TitleScreen_Options(void)
{

}

void MenuItem_TitleScreen_Exit(void)
{

}