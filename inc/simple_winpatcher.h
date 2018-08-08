#ifndef __SIMPLE_WINPATCHER_H_INCLUDED__
#define __SIMPLE_WINPATCHER_H_INCLUDED__

#include "inc/threading.h"
#include "inc/log.h"
#include "inc/simple_define.h"
#include "inc/flog.h"
#include "inc/sys_info.h"
#include "inc/simple_commander.h"
#include "inc/J2KEncoder.h"
#include "inc/J2KDecoder.h"
#include "inc/simple_debug.h"
#include "inc/simple_client.h"
#include <time.h>
#include <io.h>
#include <fcntl.h>
#include <vector>
#include <queue>
#include <string>

using namespace std;

class SimpleWindowsPatcher: public IRunnable
{
private:
	int m_nTimeCount;
	clock_t m_nLastTime, m_nStartTime;
	ClientSocket *m_Client;
	UCHAR m_txBuffer[1024 * 11];
	UCHAR m_rxBuffer[1024 * 11];
	string m_prefix;
	string m_extension;
	string m_HostAddr;
	string m_remoteVersion;
	string m_localPath;
	int m_Port;
	char m_drive;
	bool m_start;

	int m_enTimeCount;
	clock_t m_enLastTime, m_enStartTime;

public:
	SimpleWindowsPatcher(char drive) 
	{
		m_Client = NULL;
		m_drive = drive;
		m_start = true;
		
		Logger("Info: Start windows patchers version 1.0");

		char szTemp[MAX_PATH] = {0};
		sprintf(szTemp, "%c:\\atms\\", m_drive);
		m_localPath = szTemp;

		m_Client = new ClientSocket();

		m_prefix = "w";
		m_extension = "zip";

	//	SetClock(60 * 60 * 2);		// 2시간당 1회 windows patch checking
		SetClock(60);
	}

	virtual ~SimpleWindowsPatcher() 
	{ 
		if(m_Client)	delete m_Client;

		m_Client = NULL;
	}

	void SetClock(int nTime)
	{
		m_nStartTime = clock();
		m_nTimeCount = nTime;                 /* time defined -----------------*/
	}

	int GetClock()
	{
		m_nLastTime = clock();
		return (m_nTimeCount - ((m_nLastTime - m_nStartTime) / CLOCKS_PER_SEC));
	}

	virtual void run() 
	{
		int nStep = STATE_A;
		bool bRet = false;
		int nRet = 0;
		bool bExit = false;
		SOCKET socket;
		int local_ver, offset, request_ver;
		string address0, address1;
		char szTemp[20] = {0};
		int ip1, ip2, ip3, ip4;

		Logger("Info: Start windows patcher biz routine");

		SetClock(60 * 60 * 2);	// 2시간

		while(!CThread::currentThread().isInterrupted())
		{
			Sleep(60 * 1000);
			
			Logger("Info: CThread::currentThread() start routine");

			if(GetClock() <= 0)
			{
				Logger("Info: Starting time occured, start windows patch agent");
				nStep = STATE_A;

				bExit = false;

				while(!bExit)
				{
					switch(nStep)
					{
					case STATE_A:
						
						bRet = ReadConfigurations();
						if(bRet) nStep++;
						else	 bExit = true;

						break;

					case STATE_B:
						
						if(m_HostAddr.length() >= 15)
						{
							memset(szTemp, 0x00, sizeof(szTemp));
							memcpy(szTemp, m_HostAddr.c_str(), min(m_HostAddr.length(), sizeof(szTemp) - 1));
							
							// 012345678901234
							// 123.123.123.123
							ip1 = a2i(&szTemp[0],  3);
							ip2 = a2i(&szTemp[4],  3);
							ip3 = a2i(&szTemp[8],  3);
							ip4 = a2i(&szTemp[12], 3);

							sprintf(szTemp, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);

							m_HostAddr = szTemp;
						}

						address0 = m_HostAddr;
						address1 = m_HostAddr;

						ip1 = address0.find_last_of(".");
						if(ip1 > 0)
						{
							memset(szTemp, 0x00, sizeof(szTemp));

							string s = address0.substr(ip1 + 1);
							if(s.length() >= 2)
								memcpy(szTemp, &address0[ip1 + 1], 2);
							else	memcpy(szTemp, &address0[ip1 + 1], 1);

							if(strncmp(szTemp, "41", 2) == 0)
							{
								ip1 = m_HostAddr.find_last_of(".");
								if(ip1 > 0)
								{
									address1 = m_HostAddr.substr(0, ip1 + 1);
									address1 += "42";

									Logger("Info: Cluster Download ip address0 %s, address1 %s", address0.c_str(), address1.c_str());
								}
							}
							else
							{
								ip1 = m_HostAddr.find_last_of(".");
								if(ip1 > 0)
								{
									address1 = m_HostAddr.substr(0, ip1 + 1);
									address1 += "41";

									Logger("Info: Cluster Download ip address0 %s, address1 %s", address0.c_str(), address1.c_str());
								}
							}
						}

						for(nRet = 0; nRet < 10; nRet++)
						{
							Logger("Info: windows patch server request connect %s address", m_HostAddr.c_str());

							if((nRet % 2) == 0)	
								m_HostAddr = address0;
							else
								m_HostAddr = address1;

							socket = m_Client->Connect(m_HostAddr, m_Port);
							if(socket != SOCKET_ERROR)	
							{
								Logger("Succ: %s connected", m_HostAddr.c_str());
								nStep++;
								break;
							}
							else 
								Logger("Error: %s connection time out", m_HostAddr.c_str());
						}

						if(m_Client->IsConnected())	
							break;
						else					
						{
							Logger("Error: %s address, %d port connection failed", m_HostAddr.c_str(), m_Port);
							bExit = true;
						}
						
						break;

					case STATE_C:

						nRet = MkData(T_LOGIN, 0, 0);
						if(nRet > 0)	nStep++;
						else			bExit = true;

						Logger("Info: Create Login message");

						break;

					case STATE_D:

						nRet = m_Client->WriteToSocket(nRet, (char *)m_txBuffer, 5);
						if(nRet == SOCKET_ERROR) 
						{
							bExit = true;
							Logger("Error: %s address login message send failed", m_HostAddr.c_str());
						}
						else 
						{
							Logger("Succ: %s address login message send successful, next step recv", m_HostAddr.c_str());
							nStep++;
						}

						break;

					case STATE_E:

						memset(m_rxBuffer, 0x00, sizeof(m_rxBuffer));
						nRet = m_Client->ReadFromSocket((char *)m_rxBuffer, 30);
						if(nRet == SOCKET_ERROR)
						{
							Logger("Error: %s address login message recv time over occured, exit this program", m_HostAddr.c_str());
							bExit = true;
						}
						else
						{
							Logger("Succ: %s address login message recv successful, next step", m_HostAddr.c_str());
							nStep++;
						}

						break;

					case STATE_F:

						bRet = Analysis(T_LOGIN, m_txBuffer, m_rxBuffer);
						if(bRet == true) 
						{
							nStep++;
							Logger("Succ: %s address login message verify, compare version", m_HostAddr.c_str());
						}
						else 
						{
							Logger("Error: %s address login message verify", m_HostAddr.c_str());
							bExit = true;
						}

						break;

					case STATE_G:

						bRet = VersionCompare(local_ver, offset, request_ver);
						if(bRet)		
						{
							nStep++;

							Logger("Info: Found new version windows patch, atm windows patch upgrade, wait");
							Sleep(2 * 1000);
						}
						else			
						{
							Logger("Info: Host version %s, atm windows version %03d, version equal, exit this program", this->m_remoteVersion.c_str(), local_ver);
							Sleep(1000 * 3);

							TransVersionInfo(local_ver, 100);

							bExit = true;
						}

						break;

					case STATE_H:

						bRet = DoWorker(request_ver, offset);
						Logger("Info: End DoWorker, loop exit");

						bExit = true;

						break;

					default:
						break;
					}
				}

				Logger("Info: End loop , client socket close");

				m_Client->Close();

				SetClock(60 * 60 * 2);	// 30분당 1회
			}
		}
	}

	void TransVersionInfo(int req_ver, int ratio)
	{
		int nLen;
		int retry = 0;
		int nStep = STATE_A;
		BOOL bRet;

		Sleep(50);

		Logger("Info: Start TransVersionInfo");

		while(1)
		{
			switch(nStep)
			{
			case STATE_A:

				nLen = MkData(T_FIELVER, req_ver, ratio);
				if(nLen > 0)	nStep++;
				else			return;

				break;

			case STATE_B:

				nLen = m_Client->WriteToSocket(nLen, (char *)m_txBuffer, 10);
				if(nLen > 0)	nStep++;
				else			return;

				break;

			case STATE_C:

				memset(m_rxBuffer, 0x00, sizeof(m_rxBuffer));			
				nLen = m_Client->ReadFromSocket((char *)m_rxBuffer, 30);
				if(nLen > 0)	nStep++;
				else			return;

				break;

			case STATE_D:
				
				bRet = Analysis(T_FIELVER, m_txBuffer, m_rxBuffer);
				if(bRet == TRUE)	
				{
					Logger("Succ: TransVersionInfo version %d, ratio %d", req_ver, ratio);
					return;
				}
				else			
				{
					Logger("Error: TransVersionInfo version %d, ratio %d", req_ver, ratio);
					return;
				}

				break;

			default:
				return;
				break;
			}
		}
	}

	bool DoWorker(int request_ver, int offset)
	{
		bool bExit = false;
		int nStep = STATE_A;
		BOOL bRet;
		int nLen;
		string state;
		int curVersion;
		int recvRatio = 0;
		char crc_code[6] = {0};

		curVersion = request_ver - 1;
		if(curVersion < 0)	curVersion = 0;
	
		Set_eClock(60 * 60);	// 1시간 수신

		while(!bExit)
		{
			if(Get_eClock() <= 0)
			{
				TransVersionInfo(request_ver, 0);

				return false;
			}

			switch(nStep)
			{
			case STATE_A:

				nLen = MkData(T_FILEREQ, request_ver, offset);
				if(nLen > 0)	nStep++;
				else			
				{
				//	Logger("Error: 업그레이드 버전 전문 생성 오류, 종료합니다.");
					Logger("Error: windows patch upgrade version message creation error occured, exit this program");
					return false;
				}
				
				break;

			case STATE_B:

				nLen = m_Client->WriteToSocket(nLen, (char *)m_txBuffer, 10);
				if(nLen > 0)	
					nStep++;
				else			
				{
				//	Logger("Error: 업그레이드 버전 전문 전송 오류, 종료합니다.");
					Logger("Error: windows patch version message send failed, exit this program");

					return false;
				}

				break;

			case STATE_C:

				memset(m_rxBuffer, 0x00, sizeof(m_rxBuffer));
				nLen = m_Client->ReadFromSocket((char *)m_rxBuffer, 30);
				if(nLen > 0)	
					nStep++;
				else			
				{
					Logger("Error: Windows patch upgrade version recv error occured, exit this program");
					return false;
				}

				break;

			case STATE_D:
			
				bRet = this->Analysis(T_FILEREQ, m_txBuffer, m_rxBuffer);
				if(bRet)		
					nStep++;
				else			
				{
					Logger("Error: Windows version message verify error occured, exit this program");

					return false;
				}

				break;

			case STATE_E:

				nStep = DataParse(m_rxBuffer, request_ver, offset);

				break;

			case STATE_F:

				memset(&crc_code[0], 0x00, sizeof(crc_code));
				memcpy(&crc_code[0], &m_rxBuffer[39], 5);

				nStep = CompleteDataParse(crc_code, request_ver);

				break;
		
			case STATE_X:
			
				VersionUpdate();
				
				Logger("Info: End Version update phase, return biz routine");

				return true;

				break;

			case STATE_Z:
				Logger("Fault: ATM windows upgrade file creation error occured, exit this program");
				return false;
				break;

			default:
				return false;
				break;
			}
		}

		return false;
	}

	void VersionUpdate()
	{
		WIN32_FIND_DATA wd, fd;
		HANDLE hFind, hFile;
		string path;
		string srcfile;
		BOOL bGood = TRUE;
		BOOL bRet;
		string tmp;

		path = m_localPath + "patch\\";
		path += "complete\\*.";
		path += m_extension;

		hFind = FindFirstFile(path.c_str(), &wd);
		if(hFind == INVALID_HANDLE_VALUE)	return;

		while(bGood)
		{
			if(!(wd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				srcfile = m_localPath + "patch\\";
				srcfile += "complete\\";

				srcfile += wd.cFileName;

				bRet = SumCheck(srcfile, m_localPath + "agent\\7z.exe");
				if(!bRet)	
				{
					Logger("Error: %s file sum check error occured, delete", srcfile.c_str());
					DeleteFile(srcfile.c_str());
					FindClose(hFind);
					return;

					break;
				}
			}
			bGood = FindNextFile(hFind, &wd);
		}

		FindClose(hFind);

	//	if(!bRet)	// sum check error , all file delete
	//	{
	//		path = m_localPath + "patch\\";
	//		path += "complete\\";

	//		TruncateFile(path, "*.*");

	//		return;
	//	}

		bGood = TRUE;

		path = m_localPath + "patch\\";
		path += "complete\\*.";
		path += m_extension;

		hFind = FindFirstFile(path.c_str(), &wd);
		if(hFind != INVALID_HANDLE_VALUE)
		{
			while(bGood)
			{
				if(!(wd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					tmp = m_localPath + "patch\\";
					tmp += "complete\\";
					tmp += wd.cFileName;

					srcfile = m_localPath + "agent\\7z.exe x ";
					srcfile += tmp;
					srcfile += " -o";
				//	srcfile += m_localPath;
				//	srcfile += "patch\\complete";
				//	srcfile += " -y";
					srcfile += "c:\\windows_patch -y";

					hFile = FindFirstFile("c:\\windows_patch", &fd);
					if(hFile == INVALID_HANDLE_VALUE)	
						CreateDirectory("c:\\windows_patch", NULL);
					else	FindClose(hFile);

					Logger("Info: Decompress %s directory", srcfile.c_str());

					bRet = SystemCall(srcfile, true, m_localPath + "result.txt");
					if(bRet)
					{	
					//	path = m_localPath + "patch\\complete\\*.bat";
						path = "c:\\windows_patch\\*.bat";
						hFile = FindFirstFile(path.c_str(), &fd);
						if(hFile != INVALID_HANDLE_VALUE)
						{
							FindClose(hFile);

						//	path = m_localPath + "patch\\complete\\";
							path = "c:\\windows_patch\\";
							path += fd.cFileName;
						
							Logger("Info: Found windows patch %s file, patch wait...", fd.cFileName);
							Logger("Info: System call %s command", path.c_str());

							bRet = SystemCall(path, false, "");
							if(bRet)	Logger("Succ: %s system call successful", path.c_str());
							else		Logger("Error: %s system call failed", path.c_str());

							string patch_ver_file = m_localPath + "patch.ini";
							string patch_ver = "";
							patch_ver.insert(0, wd.cFileName, 4);

							Logger("Info: windows patch version information file %s", patch_ver_file.c_str());
							Logger("Info: windows patch version info %s", patch_ver.c_str());

							WritePrivateProfileString("PCL", "PATCH_VER", patch_ver.c_str(), patch_ver_file.c_str());
						}
					}
					else 
					{
						Logger("Error: %s system call failed, windows patch not upgrade", srcfile.c_str());
					}
				}

				bGood = FindNextFile(hFind, &wd);
			}

			FindClose(hFind);
		}

		Logger("Info: End system call phase");

		char tmp00[5] = {0};
		memcpy(tmp00, wd.cFileName, 4);

		int ver = atoi(&tmp00[1]);
		TransVersionInfo(ver, 100);

		path = m_localPath + "patch\\";
		path += "complete";

		TruncateFile(path, "*.*");
	}

	BOOL SystemCall(string pCmdExe, bool isblock, string rstFile)
	{
		PROCESS_INFORMATION pi = {0};
		STARTUPINFO si = {0};
		string cmd;
		HANDLE hFile;
		BOOL bInherit = TRUE;

		cmd = pCmdExe;

		if(!rstFile.empty())
		{
			Logger("Info: %s File Delete", rstFile.c_str());

			DeleteFile(rstFile.c_str());

			hFile = CreateFile(rstFile.c_str(),
					   GENERIC_READ|GENERIC_WRITE,
					   FILE_SHARE_READ|FILE_SHARE_WRITE,
					   0,
					   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
		}
		

		if(!rstFile.empty() && hFile == INVALID_HANDLE_VALUE)	return FALSE;

		if(!rstFile.empty())	
			SetHandleInformation(hFile, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
		else	bInherit = FALSE;
		
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));

		if(!rstFile.empty())
		{
			si.hStdOutput = hFile;	// 도스창의 결과를 위에 생성한 파일로 연결
			si.dwFlags = STARTF_USESTDHANDLES;
		}

		if(!CreateProcess(
			0,
			(char *)cmd.c_str(),
			NULL,
			NULL,
			bInherit,
			CREATE_NO_WINDOW,
			NULL,
			NULL,
			&si,
			&pi)
			)
		{

			Logger("Error: SystemCall.%s system call command error occured", cmd.c_str());
			if(!rstFile.empty())	CloseHandle(hFile);

			return FALSE;
		}
		else	
			Logger("Succ: Download.SystemCall.%s system call command success", cmd.c_str());

		if(isblock)
			WaitForSingleObject(pi.hProcess, 30 * 60 * 1000);	// 30분 대기
		else	WaitForSingleObject(pi.hProcess, 5 * 1000);		// 5초 대기

		// Close process and thread handles. 
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		if(!rstFile.empty()) CloseHandle(hFile);

		return TRUE;
	}

	void TruncateFile(string path, string file)
	{
		WIN32_FIND_DATA wd;
		HANDLE hFind;
		BOOL bGood = TRUE;
		string fullname;
		string tmp, src;

		fullname = path;

		if(fullname.length() > 0)	
		{
			if(fullname[fullname.length() - 1] != '\\')
				fullname += "\\";
		}

		tmp = fullname;
		fullname += file;

		hFind = FindFirstFile(fullname.c_str(), &wd);
		if(hFind != INVALID_HANDLE_VALUE)
		{
			while(bGood)
			{
				if(!(wd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					src = tmp;
					src += wd.cFileName;

					Logger("Info: TruncateFile Delete file %s", src.c_str());

					DeleteFile(src.c_str());
				}

				bGood = FindNextFile(hFind, &wd);
			}

			FindClose(hFind);
		}
	}

	BOOL SumCheck(string srcFile, string verifyMedia)
	{
		FILE *fp = NULL;
		char szCmd[MAX_PATH] = {0, };
		char szBuf[MAX_PATH] = {0, };
		string tmp0;
		string tmp1;

		tmp0 = m_localPath + "patch\\";
		tmp0 += "sum.check";

		sprintf(szCmd, "%s t %s", verifyMedia.c_str(), srcFile.c_str());

		SystemCall(szCmd, true, tmp0);

		if((fp = fopen(tmp0.c_str(), "r+b")) != NULL)
		{
			while(!feof(fp))
			{
				Sleep(5);

				fscanf(fp, "%s", szBuf);

				if(strstr(szBuf, "error") || strstr(szBuf, "can't") || strstr(szBuf, "Bad"))
				{
					if(fp)	fclose(fp);
					fp = NULL;

					Logger("Info: SumCheck %s File Delete", tmp0.c_str());

					DeleteFile(tmp0.c_str());
									
					Logger("Error: %s file sum check error", srcFile.c_str());

					return FALSE;
				}
			}
		}
		else 
		{
			if(fp)	fclose(fp);
			fp = NULL;

			Logger("Error: Download.SumCheck.fopen.%s", srcFile.c_str());
		
			return TRUE;
		}

		if(fp)	fclose(fp);
		fp = NULL;

		Logger("Info: SumCheck.. %s File Delete", tmp0.c_str());

		DeleteFile(tmp0.c_str());

		Logger("Succ: SumCheck %s file sum check succesfull", srcFile.c_str());

		return TRUE;
	}

	string MakeCrcCode(string file)
	{
		WIN32_FIND_DATA wd;
		HANDLE hFind;
		string tmp, stmp;
		char crc_code[20] = {0};

		CreateDirectory("c:\\sh", NULL);

		hFind = FindFirstFile("c:\\sh\\sh.exe", &wd);
		if(hFind == INVALID_HANDLE_VALUE)
		{
			CopyFile("c:\\kfbdown\\sh.exe", "c:\\sh\\sh.exe", FALSE);
			CopyFile("d:\\lgatm\\down\\sh.exe", "c:\\sh\\sh.exe", FALSE);
		}
		else	FindClose(hFind);

		hFind = FindFirstFile("c:\\sh\\sum.exe", &wd);
		if(hFind == INVALID_HANDLE_VALUE)
		{
			CopyFile("c:\\kfbdown\\sum.exe", "c:\\sh\\sum.exe", FALSE);
			CopyFile("d:\\lgatm\\down\\sum.exe", "c:\\sh\\sum.exe", FALSE);
		}
		else	FindClose(hFind);

		hFind = FindFirstFile("c:\\sh\\awk", &wd);
		if(hFind == INVALID_HANDLE_VALUE)
		{
			CopyFile("c:\\kfbdown\\awk", "c:\\sh\\awk", FALSE);
			CopyFile("d:\\lgatm\\down\\awk", "c:\\sh\\awk", FALSE);
		}
		else	FindClose(hFind);

		hFind = FindFirstFile("c:\\sh\\cygwin1.dll", &wd);
		if(hFind == INVALID_HANDLE_VALUE)
		{
			CopyFile("c:\\kfbdown\\cygwin1.dll", "c:\\sh\\cygwin1.dll", FALSE);
			CopyFile("d:\\lgatm\\down\\cygwin1.dll", "c:\\sh\\cygwin1.dll", FALSE);
		}
		else	FindClose(hFind);

		tmp = "c:\\sh\\";
		stmp = "c:\\sh\\";

		tmp += "sum.sh";
		stmp += "sum.log";

		Logger("Info: Start %s file crc code make, %s file sh file verify", file.c_str(), tmp.c_str());

		DeleteFile(stmp.c_str());

		hFind = FindFirstFile(tmp.c_str(), &wd);
		if(hFind != INVALID_HANDLE_VALUE)
		{
			FindClose(hFind);

			DeleteFile(tmp.c_str());
		}

		string tmp0 = "CMDir=\"c:/sh\"\r\n";
		
		int index = 0;
		string converted_file_name = file;
		while((index = converted_file_name.find("\\")) > 0)
			converted_file_name[index] = '/';

		string tmp2 = "$CMDir/sum -r " + converted_file_name + " | $CMDir/awk '{print $1}' > $CMDir/sum.log";

		FILE *fp;
		string tmp1;

		tmp1 = tmp0 + tmp2;
		
		Logger("Info: %s file content %s", tmp.c_str(), tmp1.c_str());

		fp = fopen(tmp.c_str(), "w+b");
		if(fp == NULL)	
		{
			Logger("Error: %s file create error occured", tmp.c_str());
			return "";
		}

		fwrite(tmp1.c_str(), tmp1.length(), 1, fp);
		fclose(fp);

		string cmd = "c:\\sh\\";
		cmd += "sh.exe ";
		cmd += "c:\\sh\\";
		cmd += "sum.sh";
		
		Logger("Info: %s file create complete, and process command %s", tmp.c_str(), cmd.c_str());

		STARTUPINFO	si = { sizeof(si) };
		PROCESS_INFORMATION pi;

		int success = CreateProcess(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
		if(success)
		{
			Logger("Succ: %s command call success", cmd.c_str());

			WaitForSingleObject(pi.hProcess, 1000 * 60 * 5);	 // 5분 대기
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
		else	
		{
			Logger("Error: %s command call error occured", cmd.c_str());
			return "";
		}

		index = 0;

		tmp = "c:\\sh\\";
		tmp += "sum.log";
		bool bFind = false;

		Logger("Info: start %s file searching...", tmp.c_str());

		while(index < 10)
		{
			hFind = FindFirstFile(tmp.c_str(), &wd);
			if(hFind == INVALID_HANDLE_VALUE)	
			{
				Sleep(500);
				index++;
				continue;
			}
			else	
			{
				FindClose(hFind);
				bFind = true;
				break;
			}
		}

		Logger("Info: end %s file searching...", tmp.c_str());

		fp = fopen(tmp.c_str(), "r+b");
		if(fp == NULL)	
		{
			Logger("Error: %s file not found", tmp.c_str());
			return "";
		}

		fread(crc_code, 5, 1, fp);
		fclose(fp);

		tmp = crc_code;

		return tmp;
	}

	int CompleteDataParse(const char *crc_code, const int request_ver)
	{
		string path, dst;
		char tmp[10];
		WIN32_FIND_DATA wd;
		HANDLE hFind;
		BOOL bRet;
		string mk_crc_code;

		path = m_localPath + "patch\\";
		path += "uncomplete\\";
		path += m_prefix;

		sprintf(tmp, "%03d", request_ver - 1);
		path += tmp;
		path += ".";
		path += m_extension;
		
		dst = m_localPath + "patch\\";
		dst += "complete";

		Logger("Info: %s file crc code check", path.c_str());
		
		mk_crc_code = MakeCrcCode(path);
		if(mk_crc_code.length() <= 0)
		{
			Logger("Error: %s file crc code creation error occured", path.c_str());
			DeleteFile(path.c_str());

			return STATE_Z;
		}

		if(mk_crc_code.compare(crc_code) != 0)
		{
			Logger("Error: %s file make crc code value %s, host crc code %s not equal, exit this program", path.c_str(), mk_crc_code.c_str(), crc_code);
			DeleteFile(path.c_str());

			return STATE_Z;
		}
		else 
			Logger("Succ: %s file crc code same", path.c_str());

		hFind = FindFirstFile(dst.c_str(), &wd);
		if(hFind == INVALID_HANDLE_VALUE)	CreateDirectory(dst.c_str(), NULL);
		else						FindClose(hFind);

		dst += "\\";
		dst += m_prefix;
		dst += tmp;
		dst += ".";
		dst += m_extension;

		bRet = CopyFile(path.c_str(), dst.c_str(), FALSE);
		if(bRet == FALSE)	
		{
			Logger("Error: %s file --> %s copy error occured", path.c_str(), dst.c_str());

			return STATE_Z;
		}
		else
			Logger("Succ: %s file --> %s file copy success", path.c_str(), dst.c_str());

		DeleteFile(path.c_str());

	//	if(a2i((char *)m_remoteVersion.c_str(), 3) >= request_ver)
	//		return STATE_A;
	//	else	
		// 윈도우 패치는 한번에 하나씩만,
		return STATE_X;
	}

	/*
	int CompleteDataParse(const int request_ver)
	{
		string path, dst;
		char tmp[10];
		WIN32_FIND_DATA wd;
		HANDLE hFind;
		BOOL bRet;

		path = m_localPath + "patch\\";
		path += "uncomplete\\";
		path += m_prefix;

		sprintf(tmp, "%03d", request_ver - 1);
		path += tmp;
		path += ".";
		path += m_extension;
		
		dst = m_localPath + "patch\\";
		dst += "complete";
		
		hFind = FindFirstFile(dst.c_str(), &wd);
		if(hFind == INVALID_HANDLE_VALUE)	CreateDirectory(dst.c_str(), NULL);
		else						FindClose(hFind);

		dst += "\\";
		dst += m_prefix;
		dst += tmp;
		dst += ".";
		dst += m_extension;

		bRet = CopyFile(path.c_str(), dst.c_str(), false);
		if(bRet == false)	return STATE_Z;

		Logger("Info: CompleteDataParse %s File Delete File", path.c_str());

		DeleteFile(path.c_str());

		if(a2i((char *)m_remoteVersion.c_str(), 3) >= request_ver)	
			return STATE_A;
		else	return STATE_X;
	}
	*/

	int DataParse(UCHAR *rxBuffer, int &request_ver, int &offset)
	{
		WIN32_FIND_DATA wd;
		HANDLE hFind;
		string path;
		char tmp[20] = {0};
		FILE *file;
		int srv_filesize = 0;
		int srv_datasize = 0;

		path = m_localPath + "patch\\";
		path += "uncomplete";

		hFind = FindFirstFile(path.c_str(), &wd);
		if(hFind == INVALID_HANDLE_VALUE)	CreateDirectory(path.c_str(), NULL);
		else					FindClose(hFind);

		path += "\\";
		path += m_prefix;
		sprintf(tmp, "%03d", request_ver);
		path += tmp;

		path += ".";
		path += m_extension;

		file = fopen(path.c_str(), "a+b");
		if(file == NULL)	return STATE_Z;

		fseek(file, 0, SEEK_END);

		srv_filesize = a2i((char *)&rxBuffer[19], 10);
		srv_datasize = a2i((char *)&rxBuffer[50], 10);

		fwrite(&rxBuffer[60], srv_datasize, 1, file);
		fclose(file);
	
		offset += srv_datasize;

	//	if(srv_filesize <= offset)
	//	{
	//		offset = 0;
	//		request_ver++;

	//		return STATE_F;	// NEXT 파일 요청 송신
	//	}
	//	else  
	//		return STATE_A;	// 다음 OFFSET 위치의 FILE DATA 요청

		if(srv_filesize == offset)
		{
			offset = 0;
			request_ver++;

			return STATE_F;
		}
		else if(srv_filesize < offset)
		{
			// may be something is wrong
			DeleteFile(path.c_str());

			return STATE_Z;
		}
		else  
			return STATE_A;	// 다음 OFFSET 위치의 FILE DATA 요청

		return 0;
	}

	bool VersionCompare(int &local_ver, int &offset, int &request_ver)
	{
		WIN32_FIND_DATA wd;
		HANDLE hFind;
		BOOL bGood = TRUE;
		char loc_ver[10] = {0};
		bool uncomplete = false;
		string path;

		for(int i = 0; i < 3; i++)
		{
			if(isdigit(m_rxBuffer[13 + i])) 
				continue;
			else  return false;
		}

		char szTemp[MAX_PATH] = {0};

		sprintf(&szTemp[0], "%c:\\atms\\patch", m_drive);

		hFind = FindFirstFile(szTemp, &wd);
		if(hFind == INVALID_HANDLE_VALUE) 
		{
			CreateDirectory(szTemp, NULL);
			strcat(szTemp, "\\uncomplete");
			CreateDirectory(szTemp, NULL);
		}
		else	FindClose(hFind);

		path = m_localPath;
		path += "patch\\uncomplete\\*.";
		path += m_extension;

		hFind = FindFirstFile(path.c_str(), &wd);
		if(hFind != INVALID_HANDLE_VALUE)
		{
			while(bGood)
			{
				if(!(wd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					offset = (wd.nFileSizeHigh <= 0) ? wd.nFileSizeLow : wd.nFileSizeHigh;
					memcpy(loc_ver, &wd.cFileName[1], 3);

					local_ver = a2i(loc_ver, 3);

					uncomplete = true;
				}

				bGood = FindNextFile(hFind, &wd);
			}
			FindClose(hFind);
		}
		else // uncomplete file 없음
		{
			offset = 0;
		
			string path = m_localPath + "patch.ini";
			GetPrivateProfileString("PCL", "PATCH_VER", "w000", loc_ver, sizeof(loc_ver) - 1, path.c_str());
		
			local_ver = a2i(&loc_ver[1], 3);	
		}

		if(uncomplete == true && (a2i((char *)m_remoteVersion.c_str(), 3) >= local_ver))
		{
			request_ver = local_ver;

			return true;
		}

		if(a2i((char *)m_remoteVersion.c_str(), 3) > local_ver)
		{
			request_ver = local_ver + 1;

			return true;
		}

		return false;
	}

	int a2i(char *buffer, int length)
	{
		char szTemp[15] = {0};

		memcpy(szTemp, buffer, min(sizeof(szTemp) - 1, length));

		return atoi(szTemp);
	}

	bool Analysis(int type, UCHAR *src, UCHAR *dst)
	{
		bool bRet = false;
		UCHAR src_code[3], dst_code[3];

		memset(src_code, 0x00, sizeof(src_code));
		memset(dst_code, 0x00, sizeof(dst_code));

		memcpy(src_code, &src[10], 2);
		memcpy(dst_code, &dst[10], 2);

		src_code[1] += 1;

		if(strncmp((char *)src_code, (char *)dst_code, 2) == 0)
			bRet = true;

		if(type == T_LOGIN)	
		{
			m_remoteVersion = "";
			this->m_remoteVersion.insert(0, (char *)&m_rxBuffer[13], 3);
		}

		return bRet;
	}

	int MkData(int mode, int version, int offset)
	{
		char tmp[MAX_PATH] = {0, };

		if(mode == T_LOGIN)
		{
			memset(&this->m_txBuffer[0], 0x00, sizeof(m_txBuffer));

			memcpy(&this->m_txBuffer[0], "0000000050", 10);
			memcpy(&this->m_txBuffer[10], "A2", 2);
			memcpy(&this->m_txBuffer[12], m_prefix.c_str(), min(m_prefix.length(), 1));
			memcpy(&this->m_txBuffer[16], m_extension.c_str(), min(m_extension.length(), 3));

			return 50;
		}

		if(mode == T_FILEREQ)
		{
			memset(&this->m_txBuffer[0], 0x00, sizeof(m_txBuffer));
			memcpy(&this->m_txBuffer[0], "0000000050", 10);
			memcpy(&this->m_txBuffer[10], "A3", 2);
			memcpy(&this->m_txBuffer[12], m_prefix.c_str(), min(m_prefix.length(), 1));
		
			sprintf(tmp, "%03d", version);

			memcpy(&this->m_txBuffer[13], tmp, 3);
			memcpy(&this->m_txBuffer[16], m_extension.c_str(), min(m_extension.length(), 3));

			sprintf(tmp, "%010d", offset);

			memcpy(&this->m_txBuffer[29], tmp, 10);

			return 50;
		}

		string ccmsIni = m_localPath;
		ccmsIni += "atms.ini";
		string patch_file = m_localPath + "patch.ini";

		if(mode == T_FIELVER)
		{
			memset(&this->m_txBuffer[0], 0x00, sizeof(m_txBuffer));

			memcpy(&this->m_txBuffer[0], "0000000030", 10);
			memcpy(&this->m_txBuffer[10], "A4", 2);
			
			memset(tmp, 0x00, sizeof(tmp));
			GetPrivateProfileString("ATMINFO", "JUMCODE", "9999", tmp, sizeof(tmp) - 1, ccmsIni.c_str());

			memcpy(&this->m_txBuffer[12], tmp, 4);

			memset(tmp, 0x00, sizeof(tmp));
			GetPrivateProfileString("ATMINFO", "ATMNUM", "999", tmp, sizeof(tmp) - 1, ccmsIni.c_str());

			if(tmp[0] != '9')	tmp[0] = '0';
			memcpy(&this->m_txBuffer[16], tmp, 3);

			memset(tmp, 0x00, sizeof(tmp));
			GetPrivateProfileString("PCL", "PATCH_VER", "w000", tmp, sizeof(tmp) - 1, patch_file.c_str());
			memcpy(tmp, m_prefix.c_str(), 1);
			memcpy(&this->m_txBuffer[19], tmp, 4);
			
			this->m_txBuffer[23] = tmp[0];
			
			memset(tmp, 0x00, sizeof(tmp));
			sprintf((char *)&this->m_txBuffer[24], "%03d", version);

			sprintf((char *)&this->m_txBuffer[27], "%03d", offset);

			return 30;
		}

		return 0;
	}

	bool ReadConfigurations()
	{
		WIN32_FIND_DATA wd;
		HANDLE hFind;
		string dir;
		char szTemp[MAX_PATH] = {0};

		hFind = FindFirstFile("c:\\atms\\atms.ini", &wd);
		if(hFind == INVALID_HANDLE_VALUE)
			dir = "d:\\atms\\atms.ini";
		else 
		{
			dir = "c:\\atms\\atms.ini";
			FindClose(hFind);
		}

		GetPrivateProfileString("PCL", "TERM_IP", "999.999.999.999", szTemp, sizeof(szTemp) - 1, dir.c_str());
		m_HostAddr = szTemp;

		m_Port = 20019;

	//	Logger("Info: 패치 서버 어드레스 %s, 포트 %d", m_HostAddr.c_str(), m_Port);
		Logger("Info: Windows patch server address %s, port %d", m_HostAddr.c_str(), m_Port);

		return true;
	}

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
		sprintf(strPath, "%c:\\atms\\log\\", m_drive);
		sprintf(&strPath[strlen(strPath)], "%02d\\PatchSvc%02d.LOG", st.wMonth, st.wDay);

		if((fp = fopen(strPath, "a+b")) == NULL)
			return false;

		fwrite(msg, strlen(msg), 1, fp);
		fclose(fp);

		if(m_start)
		{
			time_t timer;
			struct tm *t;

			timer = time(NULL) + (24 * 60 * 60);
			t = localtime(&timer);

			memset(strPath, 0x00, MAX_PATH);
			sprintf(strPath, "%c:\\atms\\log\\", m_drive);
			sprintf(&strPath[strlen(strPath)], "%02d\\PatchSvc%02d.LOG", t->tm_mon + 1, t->tm_mday);
			DeleteFile(strPath);

			m_start = false;
		}

		return true;
	}

	void Set_eClock(int nTime)
	{
		m_enStartTime = clock();
		m_enTimeCount = nTime;                 /* time defined -----------------*/
	}

	int Get_eClock()
	{
		m_enLastTime = clock();
		return (m_enTimeCount - ((m_enLastTime - m_enStartTime) / CLOCKS_PER_SEC));
	}
};

#endif // __SIMPLE_WINPATCHER_H_INCLUDED__