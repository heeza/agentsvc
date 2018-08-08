#include <windows.h>
#include <stdio.h>


#define EXENAME "apwizard.exe"
#define SVCNAME TEXT("apwizard")

int uninstall();

int main()
{
	uninstall();

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
		printf("\nERROR : uninstall.OpenSCManager failed %d", GetLastError());

		if(schService)		CloseServiceHandle(schService);
		if(schSCManager)	CloseServiceHandle(schSCManager);

		return 0;
	}

	schService = OpenService(schSCManager, SVCNAME, SERVICE_ALL_ACCESS);
	if(schService == NULL)
	{
		printf("\nERROR : uninstall.OpenService failed %d", GetLastError());

		if(schService)		CloseServiceHandle(schService);
		if(schSCManager)	CloseServiceHandle(schSCManager);

		return 0;
	}

	QueryServiceStatus(schService, &srvStatus);

	if(srvStatus.dwCurrentState != SERVICE_STOPPED)
	{
		bGood = ControlService(schService, SERVICE_CONTROL_STOP, &srvStatus);
	}
	if(bGood)	printf("\nSuccess : [uninstall.ControlService] service stopped error code %d", GetLastError());
	else		printf("\nError : [uninstall.ControlService] service stopped error code %d", GetLastError());

	Sleep(1000);
	printf("\nService stop waiting...");
	Sleep(1000);
	printf("\nService stop waiting...");
	Sleep(1000);
	printf("\nService stop waiting...");
	Sleep(1000);
	printf("\nService stop waiting...");
	Sleep(1000);
	printf("\nService stop waiting...");
	Sleep(1000);

	if(DeleteService(schService))
	{
		printf("SUCCESS : %s service program deleted\n", EXENAME);
	}
	else	printf("FAILED : %s service program not deleted code %d", GetLastError());

	if(schService)		CloseServiceHandle(schService);
	if(schSCManager)	CloseServiceHandle(schSCManager);
	
	Sleep(3000);

	return 0;
}