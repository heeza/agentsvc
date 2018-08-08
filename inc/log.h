#ifndef ___LOG_H_INCLUDED___
#define ___LOG_H_INCLUDED___

#include "inc/sync_simple.h"

///////////////////////////////////////////////////////////////////////////////////////////////
//---------------------------- CLASS ----------------------------------------------------------
// A simple class to log messages to the console
// considering multithreading
class Log 
{
private:
	static QMutex m_qMutex;

public:
	static void LogMessage(const char *str,...);
	static void LogWriteConsole(const char *str,...);
};

///////////////////////////////////////////////////////////////////////////////////////////////
//---------------------------- IMPLEMENTATION -------------------------------------------------

QMutex Log::m_qMutex;

void Log::LogWriteConsole(const char *str,...)
{
	int nLen, nRet;
	char cBuffer[1025];
	va_list arglist;
//	HANDLE  hOut;

	m_qMutex.Lock();

	memset(cBuffer, 0x00, sizeof(cBuffer));
	va_start(arglist, str);

	nLen = strlen(str);
	nRet = vsprintf(cBuffer, str, arglist);
	strcat(cBuffer, "\n");

	fprintf(stderr, cBuffer);

	m_qMutex.Unlock();
}

void Log::LogMessage(const char *str,...)
{
	int nLen, nRet;
	char cBuffer[1025];
	va_list arglist;
//	HANDLE hOut;

	m_qMutex.Lock();

	memset(cBuffer, 0x00, sizeof(cBuffer));
	va_start(arglist, str);

	nLen = strlen(str);
	nRet = vsprintf(cBuffer, str, arglist);
	strcat(cBuffer, "\n");

	fprintf(stderr, cBuffer);

	m_qMutex.Unlock();
}


#endif