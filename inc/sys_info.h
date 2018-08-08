#ifndef __SYS_INFO_H_INCLUDED__
#define __SYS_INFO_H_INCLUDED__

#include <Iphlpapi.h>

#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "iphlpapi.lib")

typedef struct _ASTAT_
{
	ADAPTER_STATUS adapt;
	NAME_BUFFER    NameBuff[30];
}ASTAT, * PASTAT;

ASTAT Adapter; 

class SystemInformation
{
private:
	unsigned int m_nHddFreeSpace;
	unsigned int m_nHddTotalSpace;
	char m_szProcessor[MAX_PATH];
	char m_szProcessorClock[MAX_PATH];
	char m_szOSPatchStat[MAX_PATH];
	char m_szIpAddress[MAX_PATH];
	char m_szMac[MAX_PATH];
	float m_fMemory;
	
public:
	SystemInformation()
	{
		int i;

		memset(this->m_szProcessor, 0x00, sizeof(m_szProcessor));
		memset(this->m_szProcessorClock, 0x00, sizeof(m_szProcessorClock));
		memset(this->m_szOSPatchStat, 0x00, sizeof(m_szOSPatchStat));
		memset(this->m_szIpAddress, 0x00, sizeof(m_szIpAddress));
		memset(this->m_szMac, 0x00, sizeof(m_szMac));
		
		SysOSInformation();
		SysHddStatus();
		MemStatus();
		ProcessorClock();
		ProcessorInformation();
		LocalIPAddress();
		i = MacAddress2();
		if(i == 0)	MacAddress();
	}

	int GetProcessorInformation(char *pszBuffer)
	{
		memcpy(pszBuffer, m_szProcessor, min(strlen(m_szProcessor), 50));

		return strlen(pszBuffer);
	}
	
	int GetProcessorClock(char *pszBuffer)
	{
		memcpy(pszBuffer, m_szProcessorClock, min(strlen(m_szProcessorClock), 60));

		return strlen(pszBuffer);	
	}
	
	float GetMemStatus()
	{
		return m_fMemory;
	}

	float GetHddFreeSpace()
	{
		return (float)m_nHddFreeSpace;
	}

	float GetHddTotalSpace()
	{
		return (float)m_nHddTotalSpace;
	}
	
	int GetOsPatchInformation(char *pBuffer)
	{
		memcpy(pBuffer, this->m_szOSPatchStat, min(strlen(this->m_szOSPatchStat), 100));

		return strlen(pBuffer);
	}
	
	int GetLocalIPAddress(char *pBuffer)
	{
		memcpy(pBuffer, this->m_szIpAddress, min(strlen(this->m_szIpAddress), 15));

		return strlen(pBuffer);
	}

	int GetMacAddress(char *pBuffer)
	{
		memcpy(pBuffer, this->m_szMac, min(strlen(this->m_szMac), 12));

		return strlen(pBuffer);
	}
	
	bool SetMACAddress(PIP_ADAPTER_INFO pAdapter)
	{
		if(strncmp(pAdapter->IpAddressList.IpAddress.String, "127.0.0.1", 9) == 0)	
			return false;
		if(strncmp(pAdapter->IpAddressList.IpAddress.String, "0.0.0.0", 7) == 0)	
			return false;
		if(pAdapter->IpAddressList.IpAddress.String == NULL)
			return false;
		if(strncmp(pAdapter->IpAddressList.IpAddress.String, "169.254", 7) == 0)
			return false;

		sprintf((char *)m_szMac, "%02X%02X%02X%02X%02X%02X", pAdapter->Address[0],
								   pAdapter->Address[1],
								   pAdapter->Address[2],
							           pAdapter->Address[3],
							           pAdapter->Address[4],
							           pAdapter->Address[5]);
		
		int i;

		for(i = 0; i < 12; i++)	
			if(!isdigit(m_szMac[i]))	
				m_szMac[i] = toupper(m_szMac[i]);

		strcpy((char *)m_szIpAddress, pAdapter->IpAddressList.IpAddress.String);

		return true;
	}

	int MacAddress2(void)
	{
		IP_ADAPTER_INFO AdapterInfo[16];			// Allocate information for up to 16 NICs
		DWORD dwBufLen = sizeof(AdapterInfo);			// Save the memory size of buffer
		bool bRet, bGood = false;
		int nCnt = 0;

		DWORD dwStatus = GetAdaptersInfo(			// Call GetAdapterInfo
				AdapterInfo,				// [out] buffer to receive data
				&dwBufLen);				// [in] size of receive data buffer

		if(dwStatus != ERROR_SUCCESS)				// Verify return value is valid, no buffer overflow
			return 0;

		PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;		// Contains pointer to current adapter info
		do {
			bRet = SetMACAddress(pAdapterInfo);
			
			if(bRet == true)	bGood = true;

			pAdapterInfo = pAdapterInfo->Next;		// Progress through linked list
			nCnt++;
		}
		while(pAdapterInfo && nCnt < 16);					// Terminate if last adapter
		
		if(bGood)	return 1;
		else		return 0;
	}


	int MacAddress()
	{
		NCB Ncb;
		UCHAR uRetCode;
		LANA_ENUM   lenum;
		int      i;

		memset( &Ncb, 0, sizeof(Ncb) );
		Ncb.ncb_command = NCBENUM;
		Ncb.ncb_buffer = (UCHAR *)&lenum;
		Ncb.ncb_length = sizeof(lenum);
		uRetCode = Netbios(&Ncb);
 
		for(i=0; i < lenum.length ;i++)
		{
			memset( &Ncb, 0, sizeof(Ncb) );
			Ncb.ncb_command = NCBRESET;
			Ncb.ncb_lana_num = lenum.lana[i];
 
			uRetCode = Netbios( &Ncb );
 
			memset( &Ncb, 0, sizeof (Ncb) );
			Ncb.ncb_command = NCBASTAT;
			Ncb.ncb_lana_num = lenum.lana[i];
 
			strcpy((char *)Ncb.ncb_callname,  "*               " );
			Ncb.ncb_buffer = (PUCHAR)&Adapter;
			Ncb.ncb_length = sizeof(Adapter);
 
			uRetCode = Netbios(&Ncb);
			if(uRetCode == 0)
			{
				sprintf((char *)m_szMac, "%02x%02x%02x%02x%02x%02x\n", 
						Adapter.adapt.adapter_address[0],
						Adapter.adapt.adapter_address[1],
						Adapter.adapt.adapter_address[2],
						Adapter.adapt.adapter_address[3],
						Adapter.adapt.adapter_address[4],
						Adapter.adapt.adapter_address[5]);
			}
		}
		
		for(i = 0; i < 12; i++)	
			if(!isdigit(m_szMac[i]))	
				m_szMac[i] = toupper(m_szMac[i]);

		return lenum.length;
	}

	int LocalIPAddress()
	{
		struct sockaddr_in server_sin;
		struct hostent *pHOST;
		char strBuffer[30] = {0};
		int i = 0;
		
		pHOST = gethostbyname("localhost");
		if (pHOST == NULL)      return FALSE;
		
		pHOST = gethostbyname(pHOST->h_name);
		if (pHOST == NULL)      return FALSE;

		// 자동설정IP(169.254.xxx.xxx) 제거
		if(((PUCHAR)pHOST->h_addr_list[i])[0] == 169 && ((PUCHAR)pHOST->h_addr_list[i])[1] == 254) i++;
		// Lan Card 하나인 경우...
		if(pHOST->h_addr_list[i] == NULL) i--;
		
		// memcpy(&server_sin.sin_addr, pHOST->h_addr, pHOST->h_length);
		memcpy(&server_sin.sin_addr, pHOST->h_addr_list[i], pHOST->h_length);
		strcpy(strBuffer, inet_ntoa(server_sin.sin_addr));
		
		memcpy(m_szIpAddress, strBuffer, strlen(strBuffer));
		
		return strlen(m_szIpAddress);
	}

	/*
	#define WIN_MAJOR_MAX	    10    // Windows 10
	#define WIN_MAJOR_MIN	    5    // Windows 2000 or XP

	BOOL GetWinVer(DWORD &dwMajor, DWORD &dwMinor)
	{
		DWORD dwTypeMask;
		DWORDLONG dwlConditionMask = 0;
		OSVERSIONINFOEX ovi;

		dwMajor = 0;
		dwMinor = 0;

		for (int mjr = WIN_MAJOR_MAX; mjr >= WIN_MAJOR_MIN; mjr--) {
			for (int mnr = 0; mnr <= 3; mnr++) {
				memset(&ovi, 0, sizeof(OSVERSIONINFOEX));
				ovi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

				ovi.dwMajorVersion = mjr;
				ovi.dwMinorVersion = mnr;

				dwlConditionMask = 0;

				VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_EQUAL);

				VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, VER_EQUAL);

				dwTypeMask = VER_MAJORVERSION | VER_MINORVERSION;

				if (TRUE == VerifyVersionInfo(&ovi, dwTypeMask, dwlConditionMask)) {
					dwMajor = mjr;
					dwMinor = mnr;
					break;
				}
			}
		}
	}
	*/

	BOOL SysOSInformation()
	{
		OSVERSIONINFOEX osvi;
		BOOL bOsVersionInfoEx;

		// Try calling GetVersionEx using the OSVERSIONINFOEX structure.
		// If that fails, try using the OSVERSIONINFO structure.
		ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

		if(!(bOsVersionInfoEx = GetVersionEx((OSVERSIONINFO *) &osvi)))
		{
			osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
			if(!GetVersionEx((OSVERSIONINFO *) &osvi))
				return FALSE;
		}

		switch(osvi.dwPlatformId)
		{
		// Test for the Windows NT product family.
		case VER_PLATFORM_WIN32_NT:
			strcpy(m_szOSPatchStat, "Microsoft Windows XP ");
			strcat(m_szOSPatchStat, "Professional ");
/*
			// Test for the specific product.
			if(osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2)
				strcpy(m_szOSPatchStat, "Microsoft Windows Server 2003, ");

			if(osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1)
				strcpy(m_szOSPatchStat, "Microsoft Windows XP ");

			if(osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0)
				strcpy(m_szOSPatchStat, "Microsoft Windows 2000 ");

			if(osvi.dwMajorVersion <= 4)
				strcpy(m_szOSPatchStat, "Microsoft Windows NT ");

			// Test for specific product on Windows NT 4.0 SP6 and later.
			if(bOsVersionInfoEx)
			{
				// Test for the workstation type.
				if(osvi.wProductType == VER_NT_WORKSTATION)
				{
					if(osvi.dwMajorVersion == 4)
						strcat(m_szOSPatchStat, "Workstation 4.0 ");
					else if(osvi.wSuiteMask & VER_SUITE_PERSONAL)
						strcat(m_szOSPatchStat, "Home Edition ");
					else	strcat(m_szOSPatchStat, "Professional ");
				}
				// Test for the server type.
				else if(osvi.wProductType == VER_NT_SERVER || osvi.wProductType == VER_NT_DOMAIN_CONTROLLER)
				{
					if(osvi.dwMajorVersion==5 && osvi.dwMinorVersion==2)
					{
						if(osvi.wSuiteMask & VER_SUITE_DATACENTER)
							strcat(m_szOSPatchStat, "Datacenter Edition ");
						else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
							strcat(m_szOSPatchStat, "Enterprise Edition ");
						else if ( osvi.wSuiteMask == VER_SUITE_BLADE )
							strcat(m_szOSPatchStat, "Web Edition ");
						else	strcat(m_szOSPatchStat, "Standard Edition ");
					}
					else if(osvi.dwMajorVersion==5 && osvi.dwMinorVersion==0)
					{
						if(osvi.wSuiteMask & VER_SUITE_DATACENTER )
							strcat(m_szOSPatchStat, "Datacenter Server ");
						else if(osvi.wSuiteMask & VER_SUITE_ENTERPRISE)
							strcat(m_szOSPatchStat, "Advanced Server ");
						else	strcat(m_szOSPatchStat, "Server ");
					}
					else  // Windows NT 4.0 
					{
						if(osvi.wSuiteMask & VER_SUITE_ENTERPRISE)
							strcat(m_szOSPatchStat, "Server 4.0, Enterprise Edition ");
						else	strcat(m_szOSPatchStat, "Server 4.0 " );
					}
				}
			}
			// Test for specific product on Windows NT 4.0 SP5 and earlier
			else  
			{
				HKEY hKey;
				char szProductType[MAX_PATH];
				DWORD dwBufLen=MAX_PATH;
				LONG lRet;
				BOOL bGood = TRUE;
				
				lRet = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
							"SYSTEM\\CurrentControlSet\\Control\\ProductOptions",
							0, KEY_QUERY_VALUE, &hKey );
				if(lRet != ERROR_SUCCESS )
					bGood = FALSE;

				if(bGood)
				{
					lRet = RegQueryValueEx(hKey, "ProductType", NULL, NULL,
								(LPBYTE) szProductType, &dwBufLen);
					if((lRet != ERROR_SUCCESS) || (dwBufLen > MAX_PATH))
						bGood = FALSE;

					RegCloseKey(hKey);

					if(bGood)
					{
						if(strcmpi("WINNT", szProductType) == 0)
							strcat(m_szOSPatchStat, "Workstation ");
						if(strcmpi("LANMANNT", szProductType) == 0)
							strcat(m_szOSPatchStat, "Server ");
						if(strcmpi("SERVERNT", szProductType) == 0)
							strcat(m_szOSPatchStat, "Advanced Server ");

						sprintf(&m_szOSPatchStat[min(strlen(m_szOSPatchStat), 100)], " %d.%d ", osvi.dwMajorVersion, osvi.dwMinorVersion);
					}
				}
			}

			// Display service pack (if any) and build number.
			if(osvi.dwMajorVersion == 4 && 
				strcmpi(osvi.szCSDVersion, "Service Pack 6") == 0)
			{ 
				HKEY hKey;
				LONG lRet;

				// Test for SP6 versus SP6a.
				lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
							"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Hotfix\\Q246009",
							0, KEY_QUERY_VALUE, &hKey );
				if(lRet == ERROR_SUCCESS)
					sprintf(&m_szOSPatchStat[strlen(m_szOSPatchStat)], "Service Pack 6a (Build %d)\n", osvi.dwBuildNumber & 0xFFFF);
				else // Windows NT 4.0 prior to SP6a
				{
					sprintf(&m_szOSPatchStat[strlen(m_szOSPatchStat)], "%s (Build %d)\n", osvi.szCSDVersion, osvi.dwBuildNumber & 0xFFFF);
				}

				RegCloseKey(hKey);
			}
			else // not Windows NT 4.0 
			{
				sprintf(&m_szOSPatchStat[strlen(m_szOSPatchStat)], "%s (Build %d)\n", osvi.szCSDVersion, osvi.dwBuildNumber & 0xFFFF);
			}
*/
			
			break;

			// Test for the Windows Me/98/95.
		case VER_PLATFORM_WIN32_WINDOWS:

			if(osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0)
			{
				strcat(m_szOSPatchStat, "Microsoft Windows 95 ");
				if(osvi.szCSDVersion[1] == 'C' || osvi.szCSDVersion[1] == 'B')
					strcat(m_szOSPatchStat, "OSR2 ");
			} 

			if(osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 10)
			{
				strcat(m_szOSPatchStat, "Microsoft Windows 98 ");
				if(osvi.szCSDVersion[1] == 'A' )
					strcat(m_szOSPatchStat, "SE " );
			} 

			if(osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 90)
			{
				strcat(m_szOSPatchStat, "Microsoft Windows Millennium Edition\n");
			} 
			break;

		case VER_PLATFORM_WIN32s:

			strcat(m_szOSPatchStat, "Microsoft Win32s\n");

			break;
		}
		return TRUE;
	}

	BOOL SysHddStatus()
	{
		int i = 0;
		BOOL bRet = FALSE, bGood = FALSE;
		char szPath[5] = {0 , };
		unsigned int FreeGigaM, TotalGigaM;
		unsigned int Free[2] = {0}, Total[2] = {0}, Call[2] = {0};

		m_nHddFreeSpace = m_nHddTotalSpace = 0;

		for(i = 0; i < 5; i++)
		{
			sprintf(szPath, "%c:\\", 'C' + i);

			bRet = GetDiskFreeSpaceEx(szPath, (PULARGE_INTEGER)&Call, (PULARGE_INTEGER)&Total, (PULARGE_INTEGER)&Free);
			if(bRet)
			{
				FreeGigaM  = (unsigned int)((unsigned int)( Free[0] / 1024) / 1024);
				FreeGigaM += (unsigned int)( Free[1] * 4 * 1024);

				TotalGigaM  = (unsigned int)((unsigned int)(Total[0] / 1024) / 1024);
				TotalGigaM += (unsigned int)(Total[1] * 4 * 1024);

				m_nHddFreeSpace += FreeGigaM;
				m_nHddTotalSpace += TotalGigaM;

				bGood = TRUE;
			}
		}

		return bGood;
	}

	BOOL MemStatus()
	{
		MEMORYSTATUS memoryStatus;

		ZeroMemory(&memoryStatus, sizeof(MEMORYSTATUS));
		memoryStatus.dwLength = sizeof(MEMORYSTATUS);

		GlobalMemoryStatus(&memoryStatus);

		m_fMemory = (float)(memoryStatus.dwTotalPhys / 1024 / 1024);

		return TRUE;
	}

	BOOL ProcessorClock()
	{
		// CPU speed info
		HKEY hKey;
		LONG regResult;
		char szBuff[MAX_PATH] = {0};

		DWORD buffersize = sizeof(szBuff);
		
		regResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
			"Hardware\\Description\\System\\CentralProcessor\\0", 
			0,
			KEY_QUERY_VALUE, 
			&hKey);
		
		if(regResult == ERROR_SUCCESS)
		{
			regResult = RegQueryValueEx(hKey, "ProcessorNameString", NULL, NULL,
				(LPBYTE)szBuff, &buffersize);
			
			if(regResult == ERROR_SUCCESS)
				memcpy(m_szProcessorClock, szBuff, min(strlen(szBuff), 255));
		}
		
		RegCloseKey(hKey);
		
		return (regResult == ERROR_SUCCESS);	
	}

	BOOL ProcessorInformation()
	{
		SYSTEM_INFO sysInfo;

		GetSystemInfo(&sysInfo);
	
		switch(sysInfo.wProcessorArchitecture)
		{
		case PROCESSOR_ARCHITECTURE_INTEL:
			sprintf(&m_szProcessor[0], "Intel Processor");
			break;
		case PROCESSOR_ARCHITECTURE_MIPS:
			sprintf(&m_szProcessor[0], "MIPS Processor");
			break;
		case PROCESSOR_ARCHITECTURE_ALPHA:
			sprintf(&m_szProcessor[0], "Alpha Processor");
			break;
		case PROCESSOR_ARCHITECTURE_PPC:
			sprintf(&m_szProcessor[0], "PPC Processor");
			break;
		case PROCESSOR_ARCHITECTURE_UNKNOWN :
			sprintf(&m_szProcessor[0], "Unknown Processor");
			break;
		default:
			sprintf(&m_szProcessor[0], "Intel Processor");
			break;
		}

		// process level
		switch(sysInfo.wProcessorLevel)
		{
		case 4 :
			strcat(&m_szProcessor[0], " 486");
			break;
		case 5 :
			strcat(&m_szProcessor[0], " Pentium III");
			break;

		case 6 :
			strcat(&m_szProcessor[0], " Pentium VI");
			break;
			
		default:
			strcat(&m_szProcessor[0], " Unknown Pentium 4 family");
			break;
		}

		return TRUE;
	}
};

#endif // __SYS_INFO_H_INCLUDED__