#ifndef __LOG_H_INCLUDED__
#define __LOG_H_INCLUDED__

#include "inc/sync_simple.h"
#include <windows.h>
#include <time.h>
#include <direct.h>

#define FOUT	0x0001
#define DOUT	0x0002
#define BOUT	(FOUT | DOUT)

class Flog
{
private:
	static FILE *m_Fp;
	static FILE *m_FpETC;
	static QMutex m_qMutex;
	static char m_cDir;

public:
	static void InitDir(char cDir);
	static bool Init(int nMode);
	static void Close();
	static void LogMessage(int nOutMode, int nDirect, const char *str,...);
};

QMutex Flog::m_qMutex;
FILE *Flog::m_Fp;
FILE *Flog::m_FpETC;
char Flog::m_cDir;

void Flog::InitDir(char cDir)
{
	m_cDir = cDir;
}

BOOL isFile(const char *dir)
{
	if(strstr(dir, ".log") != NULL || strstr(dir, ".jnl") != NULL || strstr(dir, ".JNL") != NULL)	
		return TRUE;
	else	return FALSE;
}

void MkDir(const char *path)
{
static char seps[] = "\\";
char *token = NULL;
char str[MAX_PATH] = {0, };
char temp[MAX_PATH] = {0, };
	
	strcpy(str, path);
	
	token = strtok(str, seps);
	while(token != NULL)
	{
		if(!isFile(token))	
		{
			strcat(temp, token);
			strcat(temp, "\\");
			_mkdir(temp);
		}

		token = strtok(NULL, seps);
	}
}

bool Flog::Init(int nDirect)
{
	SYSTEMTIME st;
	char strPath[MAX_PATH] = {0, };
	char strPath2[MAX_PATH] = {0, };
	char log[MAX_PATH] = {0, };
	FILE *fp = NULL;

	GetLocalTime(&st);
	
	sprintf(&strPath[0], "%c:\\atms\\log\\", Flog::m_cDir);
	sprintf(&strPath[strlen(strPath)], "%02d\\Caster%02d.log", st.wMonth, st.wDay);
	
	sprintf(&strPath2[0], "%c:\\atms\\log\\", Flog::m_cDir);
	sprintf(&strPath2[strlen(strPath2)], "%02d\\etc%02d.log", st.wMonth, st.wDay);

	OutputDebugStringA(strPath);

	MkDir(strPath);

	if(nDirect == 0)
	{
		Flog::m_Fp = fopen(strPath, "a+b");
		if(Flog::m_Fp == NULL)	
		{
			sprintf(log, "%s fopen failed LastError Code %d", strPath, GetLastError());
			OutputDebugStringA(log);
			return false;
		}
	}
	else
	{
		Flog::m_FpETC = fopen(strPath2, "a+b");
		if(Flog::m_FpETC == NULL)
		{
			sprintf(log, "%s fopen failed LastError Code %d", strPath, GetLastError());
			OutputDebugStringA(log);
			if(Flog::m_Fp)	
			{
				fclose(Flog::m_Fp);
				Flog::m_Fp = NULL;
			}
		
			return false;
		}
	}

	time_t timer;
	struct tm *t;
	
	timer = time(NULL) + (24 * 60 * 60);
	t = localtime(&timer);

	if(nDirect == 0)
	{
		memset(&strPath[0], 0x00, MAX_PATH);
		sprintf(&strPath[0], "%c:\\atms\\log\\", Flog::m_cDir);
		sprintf(&strPath[strlen(strPath)], "%02d\\Caster%02d.log", t->tm_mon + 1, t->tm_mday);

		fp = fopen(strPath, "w+b");
		if(fp)	fclose(fp);
		fp = NULL;
	}
	else
	{
		memset(&strPath2[0], 0x00, MAX_PATH);
		sprintf(&strPath2[0], "%c:\\atms\\log\\", Flog::m_cDir);
		sprintf(&strPath2[strlen(strPath2)], "%02d\\etc%02d.log", t->tm_mon + 1, t->tm_mday);

		fp = fopen(strPath2, "w+b");
		if(fp)	fclose(fp);
		fp = NULL;
	}

	return true;
}

void Flog::Close()
{
	if(Flog::m_Fp)		fclose(Flog::m_Fp);
	if(Flog::m_FpETC)	fclose(Flog::m_FpETC);

	Flog::m_Fp = NULL;
	Flog::m_FpETC = NULL;
}

void Flog::LogMessage(int nOutMode, int nDirect, const char *str,...)
{
	char msg[1024] = {0, };
	char msgFormat[1024] ={0, };
	SYSTEMTIME st;
	va_list va_arg;
	
	m_qMutex.Lock();

	va_start(va_arg, str);
        vsprintf(msgFormat, str, va_arg);
        va_end(va_arg);

	GetLocalTime(&st);

	sprintf(msg, "Time:%02d:%02d:%02d.%03d - %s\r\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msgFormat);

	bool ret = Flog::Init(nDirect);
	if(ret)	
	{
		if((nOutMode & FOUT) && nDirect == 0)	fprintf(Flog::m_Fp, msg);		
		if((nOutMode & FOUT) && nDirect != 0)	fprintf(Flog::m_FpETC, msg);
	}

	Flog::Close();

	m_qMutex.Unlock();
}

#endif // __LOG_H_INCLUDED__