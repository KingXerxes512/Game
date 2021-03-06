
#pragma warning(push , 0 )

#include <stdio.h>
#include <Windows.h>
#include <Psapi.h>
#include <stdint.h>		// type definitions
#include <xaudio2.h>	// audio

#define AVX

#ifdef SSE2

#include <emmintrin.h>

#endif

#ifdef AVX

#include <immintrin.h>

#endif

#pragma warning(pop)

#include <Xinput.h>
#include "Main.h"
#include "Menus.h"
#include "Logging/Logging.h"

#pragma comment(lib, "Winmm.lib") // Windows Multimedia Library, used for timeBeginPeriod to adjust the global system timer resolution

#pragma comment(lib, "XAudio2.lib") // Audio library

#pragma comment(lib, "XInput.lib") // Xbox 360 gamepad input

#define FONT_CHAR_WIDTH  6
#define FONT_CHAR_HEIGHT 7

// Globals
HWND gGameWindow;
BOOL gGameIsRunning; // Set this to FALSE to exit the game immediately. This controls the main game loop in WinMain
GAMEBITMAP gBackBuffer;
GAMEBITMAP g6x7Font;
GAMEPERFDATA gPerformanceData;
HERO gPlayer;
BOOL gWindowHasFocus;
XINPUT_STATE gGamepadState;
int8_t gGamepadID = -1;
GAMESTATE gCurrentGameState = GAMESTATE_TITLESCREEN;
GAMESTATE gPreviousGameState;
GAMESTATE gDesiredGameState;
GAMEINPUT gGameInput;
IXAudio2* gXAudio;
IXAudio2MasteringVoice* gXAudioMasteringVoice;
IXAudio2SourceVoice* gXAudioSFXSourceVoice[NUMBER_OF_SFX_SOURCE_VOICES];
IXAudio2SourceVoice* gXAudioMusicSouceVoice;
uint8_t gSFXSourceVoiceSelector;
FLOAT gSFXVolume = 0.5f;
FLOAT gMusicVolume = 0.5f;
GAMESOUND gMenuNavigate;
GAMESOUND gMenuChoose;

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

	LOGINFO("[%s] %s is starting.", __FUNCTION__, GAME_NAME);

	if (GameIsAlreadyRunning() == TRUE) // No extra instances of this program!!!
	{
		LOGERROR("[%s] Another instance of this program is already running!", __FUNCTION__);

		MessageBoxA(NULL, "Another instance of this program is already running!", "Error!", MB_ICONEXCLAMATION | MB_OK);
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

	LOGINFO("[%s] Number of CPUs: %d", __FUNCTION__, gPerformanceData.SystemInfo.dwNumberOfProcessors);

	switch (gPerformanceData.SystemInfo.wProcessorArchitecture)
	{
		case PROCESSOR_ARCHITECTURE_INTEL:
		{
			LOGINFO("[%s] CPU Architecture: x86", __FUNCTION__);

			break;
		}
		case PROCESSOR_ARCHITECTURE_IA64:
		{
			LOGINFO("[%s] CPU Architecture: Itanium (lol what?)", __FUNCTION__);

			break;
		}
		case PROCESSOR_ARCHITECTURE_ARM64:
		{
			LOGINFO("[%s] CPU Architecture: ARM64", __FUNCTION__);

			break;
		}
		case PROCESSOR_ARCHITECTURE_ARM:
		{
			LOGINFO("[%s] CPU Architecture: ARM", __FUNCTION__);

			break;
		}
		case PROCESSOR_ARCHITECTURE_AMD64:
		{
			LOGINFO("[%s] CPU Architecture: x64", __FUNCTION__);

			break;
		}
		default:
		{
			LOGINFO("[%s] CPU Architecture: Unknown", __FUNCTION__);

			break;
		}
	}

	GetSystemTimeAsFileTime((FILETIME*)&gPerformanceData.PreviousSystemTime);

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

	if (InitializeSoundEngine() != S_OK)
	{
		MessageBoxA(NULL, "InitializeSoundEngine failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if (LoadWavFromFile(".\\Assets\\Assets_MenuSelect.wav", &gMenuNavigate) != ERROR_SUCCESS)
	{
		MessageBoxA(NULL, "LoadWavFromFile failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		goto Exit;
	}

	if (LoadWavFromFile(".\\Assets\\Assets_MenuChoose.wav", &gMenuChoose) != ERROR_SUCCESS)
	{
		MessageBoxA(NULL, "LoadWavFromFile failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
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
	gGameWindow = CreateWindowExA(0, GAME_NAME "_WINDOWCLASS", GAME_NAME, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, WindowClass.hInstance, NULL);

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
	gGameInput.EscapeKeyIsDown = GetAsyncKeyState(VK_ESCAPE);

	gGameInput.DebugKeyIsDown = GetAsyncKeyState(VK_F1);

	gGameInput.LeftKeyIsDown = GetAsyncKeyState(VK_LEFT) | GetAsyncKeyState('A');

	gGameInput.RightKeyIsDown = GetAsyncKeyState(VK_RIGHT) | GetAsyncKeyState('D');

	gGameInput.UpKeyIsDown = GetAsyncKeyState(VK_UP) | GetAsyncKeyState('W');

	gGameInput.DownKeyIsDown = GetAsyncKeyState(VK_DOWN) | GetAsyncKeyState('S');

	gGameInput.ChooseKeyIsDown = GetAsyncKeyState(VK_RETURN);

	if (gGamepadID > -1)
	{
		if (XInputGetState(gGamepadID, &gGamepadState) == ERROR_SUCCESS)
		{
			gGameInput.EscapeKeyIsDown |= gGamepadState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK;

			gGameInput.LeftKeyIsDown |= gGamepadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT;

			gGameInput.RightKeyIsDown |= gGamepadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

			gGameInput.UpKeyIsDown |= gGamepadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP;

			gGameInput.DownKeyIsDown |= gGamepadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN;

			gGameInput.ChooseKeyIsDown |= gGamepadState.Gamepad.wButtons & XINPUT_GAMEPAD_A;
		}
		else
		{
			// Gamepad unplugged?

			gGamepadID = -1;

			gPreviousGameState = gCurrentGameState;

			gCurrentGameState = GAMESTATE_GAMEPADUNPLUGGED;
		}
	}

	switch (gCurrentGameState)
	{
		case GAMESTATE_OPENINGSPLASHSCREEN:
		{
			PPI_OpeningSplashScreen();

			break;
		}
		case GAMESTATE_GAMEPADUNPLUGGED:
		{
			PPI_GamepadUnplugged();

			break;
		}
		case GAMESTATE_TITLESCREEN:
		{
			PPI_TitleScreen();

			break;
		}
		case GAMESTATE_OVERWORLD:
		{
			PPI_Overworld();

			break;
		}
		case GAMESTATE_BATTLE:
		{
			PPI_Battle();

			break;
		}
		case GAMESTATE_OPTIONSSCREEN:
		{
			PPI_OptionsScreen();

			break;
		}
		case GAMESTATE_EXITYESNOSCREEN:
		{
			PPI_ExitYesNoScreen();

			break;
		}
		default:
		{
			ASSERT(FALSE, "Unknown Gamestate!");
		}
	}

	gGameInput.DebugKeyWasDown = gGameInput.DebugKeyIsDown;

	gGameInput.LeftKeyWasDown = gGameInput.LeftKeyIsDown;

	gGameInput.RightKeyWasDown = gGameInput.RightKeyIsDown;

	gGameInput.UpKeyWasDown = gGameInput.UpKeyIsDown;

	gGameInput.DownKeyWasDown = gGameInput.DownKeyIsDown;

	gGameInput.ChooseKeyWasDown = gGameInput.ChooseKeyIsDown;

	gGameInput.EscapeKeyWasDown = gGameInput.EscapeKeyIsDown;

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
			case '?':
			{
				// Github did not preserve this character and instead turned it into a "question mark inside of a diamond" character. 
				// It is extended ASCII 0xBB, decimal 187, right double-angle quotes, which I use as an arrow to mark menu items.

				StartingFontSheetPixel = (FontSheet->BitmapInfo.bmiHeader.biWidth * FontSheet->BitmapInfo.bmiHeader.biHeight) - FontSheet->BitmapInfo.bmiHeader.biWidth + (CharWidth * 95);

				break;
			}
			case '?':
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
	switch (gCurrentGameState)
	{
		case GAMESTATE_OPENINGSPLASHSCREEN:
		{
			DrawOpeningSplashScreen();

			break;
		}
		case GAMESTATE_GAMEPADUNPLUGGED:
		{
			DrawGamepadUnplugged();

			break;
		}
		case GAMESTATE_TITLESCREEN:
		{
			DrawTitleScreen();

			break;
		}
		case GAMESTATE_OVERWORLD:
		{
			DrawOverworld();

			break;
		}
		case GAMESTATE_BATTLE:
		{
			DrawBattle();

			break;
		}
		case GAMESTATE_OPTIONSSCREEN:
		{
			DrawOptionsScreen();

			break;
		}
		case GAMESTATE_EXITYESNOSCREEN:
		{
			DrawExitYesNoScreen();

			break;
		}
		default:
		{
			ASSERT(FALSE, "Gamestate not implemented!");
		}
	}

	//#ifdef AVX
	//	__m256i OctoPixel = { 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, \
	//						  0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff };
	//
	//	ClearScreen(&OctoPixel);
	//#endif
	//
	//#ifdef SSE2
	//	__m128i QuadPixel = { 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff, 0x7f, 0x00, 0x00, 0xff };
	//
	//	ClearScreen(&QuadPixel);
	//#endif
	//
	//#ifndef ADVANCED_REGISTERS
	//	PIXEL32 Pixel = { 0x7f, 0x00, 0x00, 0xff };
	//
	//	ClearScreen(&Pixel);
	//#endif
	//
	//	//PIXEL32 Green = { 0x00, 0xff, 0x00, 0xff };
	//
	//	// BlitStringToBuffer("GAME OVER", &g6x7Font, &Green, 60, 60);
	//
	//	Blit32BppBitmapToBuffer(&gPlayer.Sprite[gPlayer.CurrentArmor][gPlayer.Direction + gPlayer.SpriteIndex],
	//		gPlayer.ScreenPosX,
	//		gPlayer.ScreenPosY);

	if (gPerformanceData.DisplayDebugInfo == TRUE)
	{
		DrawDebugInfo();
	}

	HDC DeviceContext = GetDC(gGameWindow);

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

#ifdef AVX
__forceinline void ClearScreen(_In_ __m256i* Color)
{
	for (int x = 0; x < (GAME_RES_WIDTH * GAME_RES_HEIGHT) / 8 /* 8 PIXEL32s inside 256 bits */; x++)
	{
		_mm256_store_si256((__m256i*)gBackBuffer.Memory + x, *Color);
	}
}

#elif defined SSE2

__forceinline void ClearScreen(_In_ __m128i* Color)
{
	for (int x = 0; x < (GAME_RES_WIDTH * GAME_RES_HEIGHT) / 4; x++)
	{
		_mm_store_si128((__m128i*)gBackBuffer.Memory + x, *Color);
	}

	/*for (int x = 0; x < GAME_RES_WIDTH * GAME_RES_HEIGHT; x += 4)
	{
		_mm_store_si128((PIXEL32*)gBackBuffer.Memory + x, *Color);
	}*/
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
	gPreviousGameState = gCurrentGameState;

	gCurrentGameState = GAMESTATE_OPTIONSSCREEN;
}

void MenuItem_TitleScreen_Exit(void)
{
	gPreviousGameState = gCurrentGameState;

	gCurrentGameState = GAMESTATE_EXITYESNOSCREEN;
}

void MenuItem_ExitYesNo_Yes(void)
{
	SendMessageA(gGameWindow, WM_CLOSE, 0, 0);
}

void MenuItem_ExitYesNo_No(void)
{
	gCurrentGameState = gPreviousGameState;

	gPreviousGameState = GAMESTATE_EXITYESNOSCREEN;
}

void MenuItem_OptionsScreen_SFXVolume(void)
{
	gSFXVolume += 0.1f;

	if ((uint8_t)(gSFXVolume * 10) > 10)
	{
		gSFXVolume = 0.0;
	}

	for (uint8_t Counter = 0; Counter < NUMBER_OF_SFX_SOURCE_VOICES; Counter++)
	{
		gXAudioSFXSourceVoice[Counter]->lpVtbl->SetVolume(gXAudioSFXSourceVoice[Counter], gSFXVolume, XAUDIO2_COMMIT_NOW);
	}
}

void MenuItem_OptionsScreen_MusicVolume(void)
{
	gMusicVolume += 0.1f;

	if ((uint8_t)(gMusicVolume * 10) > 10)
	{
		gMusicVolume = 0.0;
	}

	gXAudioMusicSouceVoice->lpVtbl->SetVolume(gXAudioMusicSouceVoice, gMusicVolume, XAUDIO2_COMMIT_NOW);
}

void MenuItem_OptionsScreen_ScreenSize(void)
{

}

void MenuItem_OptionsScreen_Back(void)
{
	gCurrentGameState = gPreviousGameState;

	gPreviousGameState = GAMESTATE_OPTIONSSCREEN;
}

void DrawOpeningSplashScreen(void)
{

}

void DrawTitleScreen(void)
{
	PIXEL32 White = { 0xff, 0xff, 0xff, 0xff };

	static uint64_t LocalFrameCounter;

	static uint64_t LastFrameSeen;

	if (gPerformanceData.TotalFramesRendered > (LastFrameSeen + 1))
	{
		if (gPlayer.Active == TRUE)
		{
			gMenu_TitleScreen.SelectedItem = 0;
		}
		else
		{
			gMenu_TitleScreen.SelectedItem = 1;
		}
	}

	// Sets the screen to black & removes ghosting
	memset(gBackBuffer.Memory, 0, GAME_DRAWING_AREA_MEMORY_SIZE);

	BlitStringToBuffer(GAME_NAME, &g6x7Font, &White, (GAME_RES_WIDTH / 2) - ((uint16_t)strlen(GAME_NAME) * FONT_CHAR_WIDTH / 2), 60);

	for (uint8_t MenuItem = 0; MenuItem < gMenu_TitleScreen.ItemCount; MenuItem++)
	{
		if (gMenu_TitleScreen.Items[MenuItem]->Enabled == TRUE)
		{
				BlitStringToBuffer(gMenu_TitleScreen.Items[MenuItem]->Name,
								   &g6x7Font,
								   &White,
								   gMenu_TitleScreen.Items[MenuItem]->x,
								   gMenu_TitleScreen.Items[MenuItem]->y);
		}
	}

	BlitStringToBuffer("?",
					   &g6x7Font,
					   &White,
					   gMenu_TitleScreen.Items[gMenu_TitleScreen.SelectedItem]->x - 2 * FONT_CHAR_WIDTH,
					   gMenu_TitleScreen.Items[gMenu_TitleScreen.SelectedItem]->y);

	LocalFrameCounter++;

	LastFrameSeen = gPerformanceData.TotalFramesRendered;
}

void DrawOverworld(void)
{

}

void DrawBattle(void)
{

}

void DrawOptionsScreen(void)
{
	PIXEL32 White = { 0xff, 0xff, 0xff, 0xff };

	PIXEL32 Grey = { 0x6f, 0x6f, 0x6f, 0x6f };

	uint16_t SFXSlider_y = 100;

	uint16_t MusicSlider_y = 115;

	static uint64_t LocalFrameCounter;

	static uint64_t LastFrameSeen;

	if (gPerformanceData.TotalFramesRendered > (LastFrameSeen + 1))
	{
		gMenu_OptionsScreen.SelectedItem = 0;
	}

	// Sets the screen to black & removes ghosting
	memset(gBackBuffer.Memory, 0, GAME_DRAWING_AREA_MEMORY_SIZE);

	for (uint8_t MenuItem = 0; MenuItem < gMenu_OptionsScreen.ItemCount; MenuItem++)
	{
		if (gMenu_OptionsScreen.Items[MenuItem]->Enabled == TRUE)
		{
			BlitStringToBuffer(gMenu_OptionsScreen.Items[MenuItem]->Name,
							   &g6x7Font,
							   &White,
							   gMenu_OptionsScreen.Items[MenuItem]->x,
							   gMenu_OptionsScreen.Items[MenuItem]->y);
		}
	}

	for (uint8_t Volume = 0; Volume < 10; Volume++)
	{
		if (Volume >= (uint8_t)(gSFXVolume * 10))
		{
			BlitStringToBuffer("\xf2", &g6x7Font, &Grey, 240 + (Volume * FONT_CHAR_WIDTH), SFXSlider_y);
		}
		else
		{
			BlitStringToBuffer("\xf2", &g6x7Font, &White, 240 + (Volume * FONT_CHAR_WIDTH), SFXSlider_y);
		}
	}

	for (uint8_t Volume = 0; Volume < 10; Volume++)
	{
		if (Volume >= (uint8_t)(gMusicVolume * 10))
		{
			BlitStringToBuffer("\xf2", &g6x7Font, &Grey, 240 + (Volume * FONT_CHAR_WIDTH), MusicSlider_y);
		}
		else
		{
			BlitStringToBuffer("\xf2", &g6x7Font, &White, 240 + (Volume * FONT_CHAR_WIDTH), MusicSlider_y);
		}
	}

	BlitStringToBuffer("?",
					   &g6x7Font,
					   &White,
					   gMenu_OptionsScreen.Items[gMenu_OptionsScreen.SelectedItem]->x - 2 * FONT_CHAR_WIDTH,
					   gMenu_OptionsScreen.Items[gMenu_OptionsScreen.SelectedItem]->y);

	LocalFrameCounter++;

	LastFrameSeen = gPerformanceData.TotalFramesRendered;
}

void DrawExitYesNoScreen(void)
{
	PIXEL32 White = { 0xff, 0xff, 0xff, 0xff };

	static uint64_t LocalFrameCounter;

	static uint64_t LastFrameSeen;

	if (gPerformanceData.TotalFramesRendered > (LastFrameSeen + 1))
	{
		gMenu_ExitYesNo.SelectedItem = 0;
	}

	// Sets the screen to black & removes ghosting
	memset(gBackBuffer.Memory, 0, GAME_DRAWING_AREA_MEMORY_SIZE);

	BlitStringToBuffer(gMenu_ExitYesNo.Name,
					   &g6x7Font,
					   &White,
					   (GAME_RES_WIDTH / 2) - ((uint16_t)strlen(gMenu_ExitYesNo.Name) * FONT_CHAR_WIDTH / 2),
					   60);

	BlitStringToBuffer(gMenu_ExitYesNo.Items[0]->Name,
					   &g6x7Font,
					   &White,
					   gMenu_ExitYesNo.Items[0]->x,
					   gMenu_ExitYesNo.Items[0]->y);

	BlitStringToBuffer(gMenu_ExitYesNo.Items[1]->Name,
					   &g6x7Font,
					   &White,
					   gMenu_ExitYesNo.Items[1]->x,
					   gMenu_ExitYesNo.Items[1]->y);

	BlitStringToBuffer("?",
					   &g6x7Font,
					   &White,
					   gMenu_ExitYesNo.Items[gMenu_ExitYesNo.SelectedItem]->x - 2 * FONT_CHAR_WIDTH,
					   gMenu_ExitYesNo.Items[gMenu_ExitYesNo.SelectedItem]->y);

	LocalFrameCounter++;

	LastFrameSeen = gPerformanceData.TotalFramesRendered;
}

void DrawGamepadUnplugged(void)
{
#define GAMEPADUNPLUGGEDSTRING1 "Gamepad Disconnected!"

#define GAMEPADUNPLUGGEDSTRING2 "Reconnect it, or press escape to continue using the keyboard."

	PIXEL32 White = { 0xff, 0xff, 0xff, 0xff };

	static uint64_t LocalFrameCounter;

	static uint64_t LastFrameSeen;

	// Sets the screen to black & removes ghosting
	memset(gBackBuffer.Memory, 0, GAME_DRAWING_AREA_MEMORY_SIZE);

	BlitStringToBuffer(GAMEPADUNPLUGGEDSTRING1,
					   &g6x7Font,
					   &White,
					   (GAME_RES_WIDTH / 2) - ((uint16_t)strlen(GAMEPADUNPLUGGEDSTRING1) * FONT_CHAR_WIDTH / 2),
					   100);

	BlitStringToBuffer(GAMEPADUNPLUGGEDSTRING2,
					   &g6x7Font,
					   &White,
					   (GAME_RES_WIDTH / 2) - ((uint16_t)strlen(GAMEPADUNPLUGGEDSTRING2) * FONT_CHAR_WIDTH / 2),
					   120);
}

void PPI_OpeningSplashScreen(void)
{

}

void PPI_TitleScreen(void)
{
	/*if (gGameInput.EscapeKeyIsDown)
	{
		SendMessageA(gGameWindow, WM_CLOSE, 0, 0);
	}*/

	if (gGameInput.DebugKeyIsDown && !gGameInput.DebugKeyWasDown)
	{
		gPerformanceData.DisplayDebugInfo = !gPerformanceData.DisplayDebugInfo;
	}

	if (gGameInput.DownKeyIsDown && !gGameInput.DownKeyWasDown)
	{
		if (gMenu_TitleScreen.SelectedItem < gMenu_TitleScreen.ItemCount - 1)
		{
			gMenu_TitleScreen.SelectedItem++;
			PlayGameSound(&gMenuNavigate);
		}
	}

	if (gGameInput.UpKeyIsDown && !gGameInput.UpKeyWasDown)
	{
		if (gMenu_TitleScreen.SelectedItem > 0 && gMenu_TitleScreen.Items[gMenu_TitleScreen.SelectedItem - 1]->Enabled == TRUE)
		{
			gMenu_TitleScreen.SelectedItem--;
			PlayGameSound(&gMenuNavigate);
		}
	}

	if (gGameInput.ChooseKeyIsDown && !gGameInput.ChooseKeyWasDown)
	{
		gMenu_TitleScreen.Items[gMenu_TitleScreen.SelectedItem]->Action();
		PlayGameSound(&gMenuChoose);
	}
}

void PPI_Overworld(void)
{
	if (gGameInput.EscapeKeyIsDown)
	{
		SendMessageA(gGameWindow, WM_CLOSE, 0, 0);
	}

	if (gGameInput.DebugKeyIsDown && !gGameInput.DebugKeyWasDown)
	{
		gPerformanceData.DisplayDebugInfo = !gPerformanceData.DisplayDebugInfo;
	}

	if (!gPlayer.MovementRemaining)
	{
		if (gGameInput.DownKeyIsDown)
		{
			if (gPlayer.ScreenPosY < GAME_RES_HEIGHT - 16)
			{
				gPlayer.MovementRemaining = 16;

				gPlayer.Direction = DOWN;
			}
		}
		else if (gGameInput.LeftKeyIsDown)
		{
			if (gPlayer.ScreenPosX > 0)
			{
				gPlayer.MovementRemaining = 16;

				gPlayer.Direction = LEFT;
			}
		}
		else if (gGameInput.RightKeyIsDown)
		{
			if (gPlayer.ScreenPosX < GAME_RES_WIDTH - 16)
			{
				gPlayer.MovementRemaining = 16;

				gPlayer.Direction = RIGHT;
			}
		}
		else if (gGameInput.UpKeyIsDown)
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

			}
		}
	}
}

void PPI_Battle(void)
{

}

void PPI_OptionsScreen(void)
{
	if (gGameInput.DebugKeyIsDown && !gGameInput.DebugKeyWasDown)
	{
		gPerformanceData.DisplayDebugInfo = !gPerformanceData.DisplayDebugInfo;
	}

	if (gGameInput.DownKeyIsDown && !gGameInput.DownKeyWasDown)
	{
		if (gMenu_OptionsScreen.SelectedItem < gMenu_OptionsScreen.ItemCount - 1)
		{
			gMenu_OptionsScreen.SelectedItem++;
			PlayGameSound(&gMenuNavigate);
		}
	}

	if (gGameInput.UpKeyIsDown && !gGameInput.UpKeyWasDown)
	{
		if (gMenu_OptionsScreen.SelectedItem > 0)
		{
			gMenu_OptionsScreen.SelectedItem--;
			PlayGameSound(&gMenuNavigate);
		}
	}

	if (gGameInput.ChooseKeyIsDown && !gGameInput.ChooseKeyWasDown)
	{
		gMenu_OptionsScreen.Items[gMenu_OptionsScreen.SelectedItem]->Action();
		PlayGameSound(&gMenuChoose);
	}
}

void PPI_ExitYesNoScreen(void)
{
	if (gGameInput.DebugKeyIsDown && !gGameInput.DebugKeyWasDown)
	{
		gPerformanceData.DisplayDebugInfo = !gPerformanceData.DisplayDebugInfo;
	}

	if (gGameInput.DownKeyIsDown && !gGameInput.DownKeyWasDown)
	{
		if (gMenu_ExitYesNo.SelectedItem < gMenu_ExitYesNo.ItemCount - 1)
		{
			gMenu_ExitYesNo.SelectedItem++;
			PlayGameSound(&gMenuNavigate);
		}
	}

	if (gGameInput.UpKeyIsDown && !gGameInput.UpKeyWasDown)
	{
		if (gMenu_ExitYesNo.SelectedItem > 0)
		{
			gMenu_ExitYesNo.SelectedItem--;
			PlayGameSound(&gMenuNavigate);
		}
	}

	if (gGameInput.ChooseKeyIsDown && !gGameInput.ChooseKeyWasDown)
	{
		gMenu_ExitYesNo.Items[gMenu_ExitYesNo.SelectedItem]->Action();
		PlayGameSound(&gMenuChoose);
	}
}

void PPI_GamepadUnplugged(void)
{
	if (gGamepadID > -1 || (gGameInput.EscapeKeyIsDown && !gGameInput.EscapeKeyWasDown))
	{
		gCurrentGameState = gPreviousGameState;

		gPreviousGameState = GAMESTATE_GAMEPADUNPLUGGED;
	}
}

HRESULT InitializeSoundEngine(void)
{
	HRESULT Result = S_OK;

	WAVEFORMATEX SfxWaveFormat = { 0 };

	WAVEFORMATEX MusicWaveFormat = { 0 };

	Result = CoInitializeEx(NULL, COINIT_MULTITHREADED);

	if (Result != S_OK)
	{
		LOGERROR("[%s] CoInitializeEx failed with 0x%08lx!", __FUNCTION__, Result);

		goto Exit;
	}

	Result = XAudio2Create(&gXAudio, 0, XAUDIO2_ANY_PROCESSOR); // allows for music to play on separate thread

	if (FAILED(Result))
	{
		LOGERROR("[%s] XAudio2Create failed with 0x%08lx!", __FUNCTION__, Result);

		goto Exit;
	}

	Result = gXAudio->lpVtbl->CreateMasteringVoice(gXAudio, &gXAudioMasteringVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, NULL, 0);

	if (FAILED(Result))
	{
		LOGERROR("[%s] CreateMasteringVoice failed with 0x%08lx!", __FUNCTION__, Result);

		goto Exit;
	}

	SfxWaveFormat.wFormatTag = WAVE_FORMAT_PCM;
	SfxWaveFormat.nChannels = 1; // Mono
	SfxWaveFormat.nSamplesPerSec = 44100;
	SfxWaveFormat.nAvgBytesPerSec = SfxWaveFormat.nSamplesPerSec * SfxWaveFormat.nChannels * 2;
	SfxWaveFormat.nBlockAlign = SfxWaveFormat.nChannels * 2;
	SfxWaveFormat.wBitsPerSample = 16;
	SfxWaveFormat.cbSize = 0x6164;

	for (uint8_t Counter = 0; Counter < NUMBER_OF_SFX_SOURCE_VOICES; Counter++)
	{
		Result = gXAudio->lpVtbl->CreateSourceVoice(gXAudio, &gXAudioSFXSourceVoice[Counter], &SfxWaveFormat, 0, XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL);

		if (Result != S_OK)
		{
			LOGERROR("[%s] CreateSourceVoice failed with 0x%08lx!", __FUNCTION__, Result);

			goto Exit;
		}

		gXAudioSFXSourceVoice[Counter]->lpVtbl->SetVolume(gXAudioSFXSourceVoice[Counter], gSFXVolume, XAUDIO2_COMMIT_NOW);
	}

	MusicWaveFormat.wFormatTag = WAVE_FORMAT_PCM;
	MusicWaveFormat.nChannels = 2; // Stereo
	MusicWaveFormat.nSamplesPerSec = 44100;
	MusicWaveFormat.nAvgBytesPerSec = MusicWaveFormat.nSamplesPerSec * MusicWaveFormat.nChannels * 2;
	MusicWaveFormat.nBlockAlign = MusicWaveFormat.nChannels * 2;
	MusicWaveFormat.wBitsPerSample = 16;
	MusicWaveFormat.cbSize = 0x6164;

	Result = gXAudio->lpVtbl->CreateSourceVoice(gXAudio, &gXAudioMusicSouceVoice, &MusicWaveFormat, 0, XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL);

	if (Result != S_OK)
	{
		LOGERROR("[%s] CreateSourceVoice failed with 0x%08lx!", __FUNCTION__, Result);

		goto Exit;
	}

	gXAudioMusicSouceVoice->lpVtbl->SetVolume(gXAudioMusicSouceVoice, gMusicVolume, XAUDIO2_COMMIT_NOW);

Exit:
	return Result;
}

DWORD LoadWavFromFile(_In_ char* FileName, _Inout_ GAMESOUND* GameSound)
{
	DWORD Error = ERROR_SUCCESS;

	DWORD NumberOfBytesRead = 0;

	DWORD RIFF = 0;

	uint16_t DataChunkOffset = 0;

	DWORD DataChunkSearcher = 0;

	BOOL DataChunkFound = FALSE;

	DWORD DataChunkSize = 0;

	HANDLE FileHandle = INVALID_HANDLE_VALUE;

	void* AudioData = NULL;

	if ((FileHandle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
	{
		Error = GetLastError();

		LOGERROR("[%s] CreateFileA failed with 0x08lx!", __FUNCTION__, Error);

		goto Exit;
	}

	if (ReadFile(FileHandle, &RIFF, sizeof(DWORD), &NumberOfBytesRead, NULL) == 0)
	{
		Error = GetLastError();

		LOGERROR("[%s] ReadFile failed with 0x08lx!", __FUNCTION__, Error);

		goto Exit;
	}

	if (RIFF != 0x46464952) // "RIFF" backwards
	{
		Error = ERROR_FILE_INVALID;

		LOGERROR("[%s] First Four Bytes of '%s' are not 'RIFF'!", __FUNCTION__, FileName);

		goto Exit;
	}

	if (SetFilePointer(FileHandle, 20, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
	{
		Error = GetLastError();

		LOGERROR("[%s] SetFilePointer failed with 0x08lx!", __FUNCTION__, Error);

		goto Exit;
	}

	if (ReadFile(FileHandle, &GameSound->WaveFormat, sizeof(WAVEFORMATEX), &NumberOfBytesRead, NULL) == 0)
	{
		Error = GetLastError();

		LOGERROR("[%s] ReadFile failed with 0x08lx!", __FUNCTION__, Error);

		goto Exit;
	}

	if (GameSound->WaveFormat.nBlockAlign != (GameSound->WaveFormat.nChannels * GameSound->WaveFormat.wBitsPerSample / 8) ||
		GameSound->WaveFormat.wFormatTag != WAVE_FORMAT_PCM ||
		GameSound->WaveFormat.wBitsPerSample != 16)
	{
		Error = ERROR_DATATYPE_MISMATCH;

		LOGERROR("[%s] This wav file did not meet the format requirements! Only PCM format, 44100Hz 16 bits per sample wav files are supported. 0x08lx!", __FUNCTION__, Error);

		goto Exit;
	}

	while (DataChunkFound == FALSE)
	{
		if (SetFilePointer(FileHandle, DataChunkOffset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
		{
			Error = GetLastError();

			LOGERROR("[%s] SetFilePointer failed with 0x08lx!", __FUNCTION__, Error);

			goto Exit;
		}

		if (ReadFile(FileHandle, &DataChunkSearcher, sizeof(DWORD), &NumberOfBytesRead, NULL) == 0)
		{
			Error = GetLastError();

			LOGERROR("[%s] ReadFile failed with 0x08lx!", __FUNCTION__, Error);

			goto Exit;
		}

		if (DataChunkSearcher == 0x61746164) // 'data' backwards
		{
			DataChunkFound = TRUE;
		}
		else
		{
			DataChunkOffset += 4;
		}

		if (DataChunkOffset > 256) // If DataChunkOffset larger than here, give up searching
		{
			Error = ERROR_DATATYPE_MISMATCH;

			LOGERROR("[%s] Data chunk not found within first 256 bytes of this file! 0x08lx!", __FUNCTION__, Error);

			goto Exit;
		}
	}

	if (SetFilePointer(FileHandle, DataChunkOffset + 4, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
	{
		Error = GetLastError();

		LOGERROR("[%s] SetFilePointer failed with 0x08lx!", __FUNCTION__, Error);

		goto Exit;
	}

	if (ReadFile(FileHandle, &DataChunkSize, sizeof(DWORD), &NumberOfBytesRead, NULL) == 0)
	{
		Error = GetLastError();

		LOGERROR("[%s] ReadFile failed with 0x08lx!", __FUNCTION__, Error);

		goto Exit;
	}

	AudioData = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, DataChunkSize);

	if (AudioData == NULL)
	{
		Error = ERROR_NOT_ENOUGH_MEMORY;

		LOGERROR("[%s] HeapAlloc failed with 0x08lx!", __FUNCTION__, Error);

		goto Exit;
	}

	GameSound->Buffer.Flags = XAUDIO2_END_OF_STREAM;

	GameSound->Buffer.AudioBytes = DataChunkSize;

	if (SetFilePointer(FileHandle, DataChunkOffset + 8, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
	{
		Error = GetLastError();

		LOGERROR("[%s] SetFilePointer failed with 0x08lx!", __FUNCTION__, Error);

		goto Exit;
	}

	if (ReadFile(FileHandle, AudioData, DataChunkSize, &NumberOfBytesRead, NULL) == 0)
	{
		Error = GetLastError();

		LOGERROR("[%s] ReadFile failed with 0x08lx!", __FUNCTION__, Error);

		goto Exit;
	}

	GameSound->Buffer.pAudioData = AudioData;

Exit:
	if (Error == ERROR_SUCCESS)
	{
		LOGINFO("[%s] Successfully loaded %s!", __FUNCTION__, FileName);
	}
	else
	{
		LOGERROR("[%s] Failed to load %s! Error: 0x08lx!", __FUNCTION__, FileName, Error);
	}

	if (FileHandle && (FileHandle != INVALID_HANDLE_VALUE))
	{
		CloseHandle(FileHandle);
	}
	return Error;
}

void PlayGameSound(_In_ GAMESOUND* GameSound)
{
	gXAudioSFXSourceVoice[gSFXSourceVoiceSelector]->lpVtbl->SubmitSourceBuffer(gXAudioSFXSourceVoice[gSFXSourceVoiceSelector], &GameSound->Buffer, NULL);

	gXAudioSFXSourceVoice[gSFXSourceVoiceSelector]->lpVtbl->Start(gXAudioSFXSourceVoice[gSFXSourceVoiceSelector], 0, XAUDIO2_COMMIT_NOW);

	gSFXSourceVoiceSelector++;

	if (gSFXSourceVoiceSelector > (NUMBER_OF_SFX_SOURCE_VOICES - 1))
		gSFXSourceVoiceSelector = 0;
}
