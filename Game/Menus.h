#pragma once

#define FONT_CHAR_WIDTH 6

typedef struct MENUITEM
{
	char* Name;

	int16_t x;

	int16_t y;

	void(*Action)(void);
} MENUITEM;

typedef struct MENU
{
	char* Name;

	uint8_t SelectedItem;

	uint8_t ItemCount;

	MENUITEM** Items;
} MENU;

// Actions

void MenuItem_TitleScreen_Resume(void);

void MenuItem_TitleScreen_StartNew(void);

void MenuItem_TitleScreen_Options(void);

void MenuItem_TitleScreen_Exit(void);

void MenuItem_ExitYesNo_Yes(void);

void MenuItem_ExitYesNo_No(void);

// Title Screen Menu

MENUITEM gMI_ResumeGame = { "Resume", (GAME_RES_WIDTH / 2) - (6 * FONT_CHAR_WIDTH / 2), 100, MenuItem_TitleScreen_Resume };

MENUITEM gMI_StartNewGame = { "Start New Game", (GAME_RES_WIDTH / 2) - (14 * FONT_CHAR_WIDTH / 2), 120, MenuItem_TitleScreen_StartNew };

MENUITEM gMI_Options = { "Options", (GAME_RES_WIDTH / 2) - (7 * FONT_CHAR_WIDTH / 2), 140, MenuItem_TitleScreen_Options };

MENUITEM gMI_Exit = { "Exit", (GAME_RES_WIDTH / 2) - (4 * FONT_CHAR_WIDTH / 2), 160, MenuItem_TitleScreen_Exit };

MENUITEM* gMI_TitleScreenItems[] = { &gMI_ResumeGame, &gMI_StartNewGame, &gMI_Options, &gMI_Exit };

MENU gMenu_TitleScreen = { "Title Screen Menu", 1, _countof(gMI_TitleScreenItems), gMI_TitleScreenItems };

// Exit Yes or No Screen

MENUITEM gMI_ExitYesNo_Yes = { "Yes", (GAME_RES_WIDTH / 2) - ((3 * FONT_CHAR_WIDTH) / 2), 100, MenuItem_ExitYesNo_Yes };

MENUITEM gMI_ExitYesNo_No = { "No", (GAME_RES_WIDTH / 2) - ((2 * FONT_CHAR_WIDTH) / 2), 120, MenuItem_ExitYesNo_No };

MENUITEM* gMI_ExitYesNoItems[] = { &gMI_ExitYesNo_Yes, &gMI_ExitYesNo_No };

MENU gMenu_ExitYesNo = { "Are you sure you want to exit?", 0, _countof(gMI_ExitYesNoItems), gMI_ExitYesNoItems };