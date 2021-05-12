#pragma once

#define GAME_NAME "Game_B"

LRESULT CALLBACK MainWindowProc(_In_ HWND WindowHandle, _In_  UINT Message, _In_  WPARAM WParam, _In_  LPARAM LParam);

DWORD CreateMainGameWindow(void);

BOOL GameIsAlreadyRunning(void);

void ProcessPlayerInput(void);

void RenderFrameGraphics(void);

