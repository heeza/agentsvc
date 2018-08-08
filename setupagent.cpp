// install.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>

#define EXENAME "apwizard.exe"
#define SVCNAME TEXT("apwizard")

int install();
int uninstall();
void Logger(const char *str,...);

int main(int argc, char* argv[])
{
	int i = 0;

	Logger("Install apwizard service");
	
	uninstall();
	install();
	
	return 0;
}

int uninstall()
{
	SC_HANDLE schSCManager = NULL;
	SC_HANDLE schService = NULL;
	SERVICE_STATUS srvStatus;
	BOOL bGood;

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if(schSCManager == NULL)	
	{
		Logger("ERROR : uninstall.OpenSCManager failed %d", GetLastError());

		if(schService)		CloseServiceHandle(schService);
		if(schSCManager)	CloseServiceHandle(schSCManager);

		return 0;
	}

	schService = OpenService(schSCManager, SVCNAME, SERVICE_ALL_ACCESS);
	if(schService == NULL)
	{
		Logger("ERROR : uninstall.OpenService failed %d", GetLastError());

		if(schService)		CloseServiceHandle(schService);
		if(schSCManager)	CloseServiceHandle(schSCManager);

		return 0;
	}

	QueryServiceStatus(schService, &srvStatus);

	if(srvStatus.dwCurrentState != SERVICE_STOPPED)
	{
		bGood = ControlService(schService, SERVICE_CONTROL_STOP, &srvStatus);
	}
	if(bGood)	Logger("Success : [uninstall.ControlService] service stopped error code %d", GetLastError());
	else		Logger("Error : [uninstall.ControlService] service stopped error code %d", GetLastError());

	if(schService)		CloseServiceHandle(schService);
	if(schSCManager)	CloseServiceHandle(schSCManager);

	return 0;
}

int install()
{
	SC_HANDLE schSCManager = NULL, schService = NULL;
	DWORD dwCode;
	char szCurPath[MAX_PATH] = {0, };
	char szTempPath[MAX_PATH] = {0, };
	char szTgtPath[MAX_PATH] = {0, };
	char szSrcPath[MAX_PATH] = {0, };
	char szActivePath[MAX_PATH] = {0, };
	HANDLE hFind;
	WIN32_FIND_DATA wd;
	BOOL bGood = TRUE;

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if(schSCManager == NULL)
	{
		dwCode = GetLastError();
		Logger("Error : [install.OpenSCManager] failed err code %d", dwCode);
		
		if(schService)		CloseServiceHandle(schService);
		if(schSCManager)	CloseServiceHandle(schSCManager);

		return 0;
	}

	GetCurrentDirectory(MAX_PATH, szCurPath);
	
	szSrcPath[0] = szCurPath[0];
	szActivePath[0] = szCurPath[0];

	strcat(szActivePath, ":\\ccms\\agent\\");
	strcat(szActivePath, EXENAME);

	Logger("Info : Service program directory %s", szActivePath);

	strcat(szSrcPath, ":\\ccms");
	CreateDirectory(szSrcPath, NULL);
	strcat(szSrcPath, "\\agent");
	CreateDirectory(szSrcPath, NULL);

	Logger("Info : Make Directory %s", szSrcPath);

	strcpy(szTempPath, szCurPath);
	strcat(szCurPath, "\\*.*");

	Logger("Info : Current Directory %s", szCurPath);

	hFind = FindFirstFile(szCurPath, &wd);
	if(hFind == INVALID_HANDLE_VALUE)
	{
		if(schService)		CloseServiceHandle(schService);
		if(schSCManager)	CloseServiceHandle(schSCManager);

		Logger("Error : Current %s Directory file not found", szCurPath);
		return 0;
	}
	else
	{
		while(bGood)
		{
			Sleep(2);

			if(!(wd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				strcpy(szTgtPath, szTempPath);
				strcat(szTgtPath, "\\");
				strcat(szTgtPath, wd.cFileName);
				
				memset(szSrcPath, 0x00, sizeof(MAX_PATH));
				szSrcPath[0] = szCurPath[0];
				strcat(szSrcPath, ":\\ccms");

				if(strncmp("ccms.ini", wd.cFileName, 8) !=0)	
					strcat(szSrcPath, "\\agent");

				strcat(szSrcPath, "\\");
				strcat(szSrcPath, wd.cFileName);

				bGood = CopyFile(szTgtPath, szSrcPath, FALSE);
				if(bGood)	Logger("Success : %s file --> %s file copy excuted", szTgtPath, szSrcPath);
				else		Logger("Failed : %s file --> %s file copy excuted", szTgtPath, szSrcPath);
				
				if(strncmp("install.log", wd.cFileName, 11) != 0)
					DeleteFile(szTgtPath);
			}

			bGood = FindNextFile(hFind, &wd);
		}
	}

	FindClose(hFind);

	hFind = FindFirstFile(szActivePath, &wd);
	if(hFind == INVALID_HANDLE_VALUE)
	{
		if(schService)		CloseServiceHandle(schService);
		if(schSCManager)	CloseServiceHandle(schSCManager);

		Logger("Error : [install.FindFirstFile] %s program not found", EXENAME);
		return 0;
	}
	FindClose(hFind);

	schService = OpenService(schSCManager, SVCNAME, SERVICE_ALL_ACCESS);
	if(schService == NULL)
	{
		Logger("ERROR : uninstall.OpenService failed %d", GetLastError());
		Logger("Info : Try CreateService function");
		
		schService = CreateService(
				schSCManager,			// SCM database 
				SVCNAME,			// name of service 
				SVCNAME,			// service name to display 
				SERVICE_ALL_ACCESS,		// desired access 
				SERVICE_WIN32_OWN_PROCESS,	// service type 
				SERVICE_AUTO_START,		// start type 
				SERVICE_ERROR_NORMAL,		// error control type 
				szActivePath,			// path to service's binary 
				NULL,				// no load ordering group 
				NULL,				// no tag identifier 
				NULL,				// no dependencies 
				NULL,				// LocalSystem account 
				NULL);
		if(schService == NULL)
		{
			Logger("Error : [install.CreateService] failed err code %d", GetLastError());

			if(schService)		CloseServiceHandle(schService);
			if(schSCManager)	CloseServiceHandle(schSCManager);
		}
		else
		{
			Logger("Success : [install.CreateService] %s service program install success", EXENAME);
		
			bGood = StartService(schService, NULL, NULL);
		
			if(bGood)	Logger("Success : [install.StartService] %s service program started", EXENAME);
		}
	}
	else
	{
		Logger("Success : [install.OpenService] %s service program open success", EXENAME);
		
		bGood = StartService(schService, NULL, NULL);
		
		if(bGood)	Logger("Success : [install.StartService] %s service program started", EXENAME);
		else		Logger("Error : [install.StartService] %s service program started", EXENAME);

		if(schService)		CloseServiceHandle(schService);
		if(schSCManager)	CloseServiceHandle(schSCManager);
	}
	

	if(schService)		CloseServiceHandle(schService);
	if(schSCManager)	CloseServiceHandle(schSCManager);

	return 0;
}

void Logger(const char *str,...)
{
	char msg[1024] = {0, };
	char msgFormat[1024] ={0, };
	char strPath[MAX_PATH] = {0, };
	SYSTEMTIME st;
	va_list va_arg;
	FILE *fp = NULL;

	va_start(va_arg, str);
        vsprintf(msgFormat, str, va_arg);
        va_end(va_arg);

	GetLocalTime(&st);

	GetCurrentDirectory(MAX_PATH, strPath);

	sprintf(msg, "Time:%02d:%02d:%02d.%03d - %s\r\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msgFormat);
	strcat(strPath, "\\install.log");

	fp = fopen(strPath, "a+b");
	if(fp == NULL)	return;

	fprintf(fp, msg);
	fclose(fp);
}
