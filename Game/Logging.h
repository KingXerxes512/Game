#pragma once

#include <stdio.h>

#include <Windows.h>

#ifdef _DEBUG

#define ASSERT(Expression, Message) if (!(Expression)) { *(int*)0 = 0; }

#else

#define ASSERT(Expression, Message) ((void)0);

#endif

#define GAME_NAME "Game"

#define LOG_FILE_NAME GAME_NAME ".log"

#define LOGINFO(Message, ...) LogMessageA(LL_INFO, Message, __VA_ARGS__)

#define LOGWARN(Message, ...) LogMessageA(LL_WARN, Message, __VA_ARGS__)

#define LOGERROR(Message, ...) LogMessageA(LL_ERROR, Message, __VA_ARGS__)

#define LOGDEBUG(Message, ...) LogMessageA(LL_DEBUG, Message, __VA_ARGS__)

#pragma warning(disable: 4018) // can't control enum data types

typedef struct REGISTRYPARAMS
{
	DWORD LogLevel;

} REGISTRYPARAMS;

REGISTRYPARAMS gRegistryParams;

typedef enum LOGLEVEL
{
	LL_NONE = 0,

	LL_ERROR = 1,

	LL_WARN = 2,

	LL_INFO = 3,

	LL_DEBUG = 4
} LOGLEVEL;

void LogMessageA(_In_ LOGLEVEL LogLevel, _In_ char* Message, _In_ ...);
