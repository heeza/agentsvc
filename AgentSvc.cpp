// AgentWizard.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#define _WIN32_WINNT 0x0400 

#include "inc/simple_caster.h"
#include "inc/flog.h"
#include <winsock.h>
#include <conio.h>
#include "inc/simple_debug.h"

SERVICE_STATUS_HANDLE ghServiceHandle;
HANDLE ghExitEvent;

void MySimpleServiceSetStatus(DWORD dwState, DWORD dwAccept);
void MySimpleServiceMain();
DWORD gCurrentState;

int SystemCall(char *pCmd)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	// Start the child process. 
	if(!CreateProcess(NULL,		// No module name (use command line)
		pCmd,			// Command line
		NULL,			// Process handle not inheritable
		NULL,			// Thread handle not inheritable
		FALSE,			// Set handle inheritance to FALSE
		CREATE_NO_WINDOW,	// No creation flags
		NULL,			// Use parent's environment block
		NULL,			// Use parent's starting directory 
		&si,			// Pointer to STARTUPINFO structure
		&pi)			// Pointer to PROCESS_INFORMATION structure
		)
		return 0;

	WaitForSingleObject(pi.hProcess, 60 * 1000 * 10);	// 10분 대기

	// Close process and thread handles. 
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return 0;
}

SERVICE_TABLE_ENTRY ste[] = 
{

	{"apwizard", (LPSERVICE_MAIN_FUNCTION)MySimpleServiceMain},
	{ NULL, NULL }
};

BOOL IsFile(const char *dir)
{
	if(strstr(dir, ".LOG") != NULL || strstr(dir, ".JNL") != NULL ||
	   strstr(dir, ".jnl") != NULL || strstr(dir, ".log") != NULL)
					return TRUE;
	else				return FALSE;
}

void MakeDir(const char *path)
{
	static char seps[] = "\\";
	char *token = NULL;
	char str[MAX_PATH] = {0, };
	char temp[MAX_PATH] = {0, };

	strcpy(str, path);

	token = strtok(str, seps);
	while(token != NULL)
	{
		if(!IsFile(token))	
		{
			strcat(temp, token);
			strcat(temp, "\\");
			_mkdir(temp);
		}

		token = strtok(NULL, seps);
	}
}

char m_cCurrDir;
bool m_bStart = true;

bool Logger(char *str,...)
{
	char msg[4096] = {0, };
	char msgFmt[2048] ={0, };
	SYSTEMTIME st;
	va_list va_arg;
	char strPath[MAX_PATH] = {0, };
	FILE *fp = NULL;
	int err = 0;

	va_start(va_arg, str);
	vsprintf(msgFmt, str, va_arg);
	va_end(va_arg);

	GetLocalTime(&st);

	sprintf(msg, "Time:%02d:%02d:%02d.%03d - %s\r\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msgFmt);

	memset(strPath, 0x00, sizeof(strPath));
	strPath[0] = m_cCurrDir;
	strcat(strPath, ":\\atms\\log\\");
	sprintf(&strPath[strlen(strPath)], "%02d\\AgentSvcMain%02d.LOG", st.wMonth, st.wDay);

	MakeDir(strPath);

	if((fp = fopen(strPath, "a+b")) == NULL)
		return false;

	fwrite(msg, strlen(msg), 1, fp);
	fclose(fp);

	if(m_bStart)
	{
		time_t timer;
		struct tm *t;

		timer = time(NULL) + (24 * 60 * 60);
		t = localtime(&timer);

		for(int i = 0; i < 7; i++)
		{
			memset(strPath, 0x00, MAX_PATH);
			strPath[0] = m_cCurrDir;
			strcat(strPath, ":\\atms\\log\\");

			sprintf(&strPath[strlen(strPath)], "%02d\\AgentSvcMain%02d.LOG", t->tm_mon + 1, t->tm_mday + i);
			DeleteFile(strPath);
		}

		m_bStart = false;
	}

	return true;
}

int main()
{
	WSAData wsData;
	DWORD dwCode;
	char m_szServAddr[MAX_PATH] = {0, };
	char szTemp[MAX_PATH] = {0, };

	SystemCall("netsh firewall add portopening TCP 20015 apwizard");
	SystemCall("netsh firewall add portopening TCP 20019 apwizard");

	char szFileName[MAX_PATH];
	char szDrive[5], szDir[MAX_PATH], fname[30], ext[5];

	GetModuleFileName(NULL, szFileName, sizeof(szFileName));
	_splitpath(szFileName, szDrive, szDir, fname, ext);

	m_cCurrDir = szDrive[0];

	GetPrivateProfileStringA("ATMS", "ATMSADDR", "0.0.0.0", &m_szServAddr[0], sizeof(m_szServAddr), "c:\\atms\\atms.ini");

	if (strncmp(m_szServAddr, "0.0.0.0", 7) == 0)	// dirve C --> D로
		m_cCurrDir = 'd';
	else
		m_cCurrDir = 'c';

	Logger("Info: Start AgentSvc main process version 2012-07-19 v1.6.4, root dir drive is %c", m_cCurrDir);

	CSimpleCaster *sCaster = NULL;
	CSimpleThreadPool *thr = NULL;

	dwCode = WSAStartup(MAKEWORD(2, 2), &wsData);

	thr = new CSimpleThreadPool(2);
	sCaster = new CSimpleCaster(m_cCurrDir);
	sCaster->setUp(m_cCurrDir);

	thr->startAll();
	thr->submit(sCaster);

	while(!_kbhit())	Sleep(500);	// 무한으로 돌기 시작...

	thr->shutdown();
	delete sCaster;
	delete thr;

	return 0;
}

// SERVICE의 현재 상태 기록
void MySimpleServiceSetStatus(DWORD dwState, DWORD dwAccept = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE)
{
	SERVICE_STATUS st;

	st.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	st.dwCurrentState = dwState;
	st.dwControlsAccepted = dwAccept;
	st.dwWin32ExitCode = 0;
	st.dwServiceSpecificExitCode = 0;
	st.dwCheckPoint = 0;
	st.dwWaitHint = 0;

	// 현재 상태 기록
	gCurrentState = dwState;

	SetServiceStatus(ghServiceHandle, &st);
}

// SERVICE PROGRAM의 외부 제어의 ENTRY POINT 제공
void MySimpleServiceHandler(DWORD fdwControl)
{
	if(fdwControl == gCurrentState)	return;

	switch(fdwControl)
	{
	case SERVICE_CONTROL_STOP:
		MySimpleServiceSetStatus(SERVICE_STOP_PENDING, 0);
		SetEvent(ghExitEvent);
		break;

	case SERVICE_CONTROL_INTERROGATE:
	default:
		MySimpleServiceSetStatus(gCurrentState);
		break;
	}
}

void MySimpleServiceMain()
{
	CSimpleCaster *sCaster = NULL;
	CSimpleThreadPool *thr = NULL;
	WSADATA wsData;

	int nRet = WSAStartup(MAKEWORD(2, 2), &wsData);
	if(nRet < 0)
	{
		return;
	}

	char m_szServAddr[MAX_PATH] = {0, };
	char szTemp[MAX_PATH] = {0, };
	char m_cCurrDir;

	SystemCall("netsh firewall add portopening TCP 20015 AgentSvc");

	ghServiceHandle = RegisterServiceCtrlHandler("apwizard", (LPHANDLER_FUNCTION)MySimpleServiceHandler);
	ghExitEvent = CreateEvent(NULL, TRUE, FALSE, "apwizardexit");

	strcpy(szTemp, "c:\\atms\\atms.ini");

	GetPrivateProfileStringA("ATMS", "ATMSADDR", "0.0.0.0", &m_szServAddr[0], sizeof(m_szServAddr), szTemp);
	
	if(strncmp(m_szServAddr, "0.0.0.0", 7) == 0)	// dirve C --> D로
		m_cCurrDir = 'd';
	else	
		m_cCurrDir = 'c';
	
	Flog::InitDir(m_cCurrDir);

	// service가 시작중 알림
	MySimpleServiceSetStatus(SERVICE_START_PENDING);
	MySimpleServiceSetStatus(SERVICE_RUNNING);
	
	thr = new CSimpleThreadPool(2);
	sCaster = new CSimpleCaster(m_cCurrDir);
	
	thr->startAll();
	thr->submit(sCaster);

	while(1)
	{
		if(WaitForSingleObject(ghExitEvent, INFINITE) == WAIT_OBJECT_0)
		{
			thr->shutdown();

			delete sCaster;
			delete thr;

			break;
		}
	}

	MySimpleServiceSetStatus(SERVICE_STOPPED);
}
