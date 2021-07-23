#pragma once

#include "Logging.h"

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
