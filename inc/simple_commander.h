#ifndef __SIMPLE_COMMANDER_H_INCLUDED__
#define __SIMPLE_COMMANDER_H_INCLUDED__

#include "inc/threading.h"
#include "inc/log.h"
#include "inc/simple_define.h"
#include "inc/flog.h"
#include "inc/safe_queue.h"
#include "inc/simple_caster.h"
#include "inc/simple_define.h"
#include "inc/J2KEncoder.h"
#include "inc/J2KDecoder.h"
#include "inc/simple_caster.h"
#include <time.h>
#include <io.h>
#include <fcntl.h>
#include <string>

#pragma warning(disable:4996)

using namespace std;

/*
초기화중	1
오프라인	2
거래대기	3
거래중		4
계원모드	5
장애모드	6
기타모드	7
*/

extern CRITICAL_SECTION GCS;

enum IO_CURRENT_MODE { _INIT = 1, _OFFLINE, _IDLE, _TRANSAT, _SUPERVIOR, _ERROR, _ETC, _UNKNOWN };

#define BUFF_LEN 2048

typedef struct _tagSharedMemory
{
	unsigned char LEN[8];		// DATA부 크기
	unsigned char LCODE[4];		// CMD LARGE CODE
	unsigned char SCODE[4];		// CMD SMALL CODE
	unsigned char RCODE[4];		// RESULT CODE
	unsigned char SID[4];		// Send의 ID
	unsigned char CRC_CODE[5];
	unsigned char DATA[BUFF_LEN];	// DATA 
} TSharedMemory;

class CSimpleCaster;

unsigned char m_TxBuffer[10000000];	// 10 메가 할당	// 1,000,000(1메가) 10,000,000(10메가)

class ServerProcessor : public IRunnable
{
private:
	SOCKET m_sCmdSock, m_sClient;
	QMutex m_qMutex;
	int m_nPort, nLen;
	struct sockaddr_in mServerAddr;
	struct sockaddr_in mClientAddr;
	bool m_bInCreateAsync;
	unsigned char m_RxBuffer[BUFF_LEN];
	clock_t m_nLastTime, m_nStartTime;
	int m_nTimeCount;
	clock_t m_nSLastTime, m_nSStartTime;
	int m_nSTimeCount;
	TSharedMemory m_SharedMem;
	IO_CURRENT_MODE m_CurrentMode;
	CSimpleCaster *m_pCaster;
	char m_szDwnFileName[MAX_PATH];
	char m_szDwnFileSize[MAX_PATH];
	char m_cCurrDir;
	bool m_bStart;
	char m_atmsDir[MAX_PATH];
	char m_atmsFull[MAX_PATH];

protected:
	void run()
	{
		SOCKET sClient;
		DWORD dwRetCode;
		int nRet;
		bool bRecv;
		char strBuf[3];
		int nMode;
		
		while(!CThread::currentThread().isInterrupted())
		{
			Sleep(1000);
			
			bRecv = false;

			sClient = Accept();

			if(sClient != INVALID_SOCKET)	Logger("Accept client socket next step Descriptor %d", sClient);

			// in comming client .... read data
			if((sClient == INVALID_SOCKET || sClient <= 0))
				continue;

			Logger("COMMON : Accept completion, Descriptor %d", sClient);

			if(sClient != INVALID_SOCKET)	
			{
				Logger("COMMON : Start ReadFromSocket");

				dwRetCode = ReadFromSocket(sClient);	// 명령어 읽기,...

				Logger("COMMON : After ReadFromSocket return value %d", dwRetCode);

				if(dwRetCode > 0)
				{
					dwRetCode = VerifyData(this->m_RxBuffer);

					memset(m_TxBuffer, 0x00, sizeof(m_TxBuffer));

					switch(dwRetCode)
					{
					case REQ_IMG:	// 이미지 요청, 바로 찾아서 덤짐 (2048씩 던진다)
					case REQ_FIL:	// 비규격 데이터 요청
						
						Logger("Find data %s", m_SharedMem.DATA);

						memset(m_TxBuffer, 0x00, sizeof(m_TxBuffer));
						nRet = FindCmdData(m_SharedMem.DATA, m_TxBuffer, dwRetCode);

						break;

					case REQ_RST:	// 원격 리셋
						
						if(dwRetCode == REQ_RST)	//전원 리셋인 경우 먼저 현재 자동화기기 전달 가능 상태를 알아봄
						{
							memset(strBuf, 0x00, sizeof(strBuf));
							//GetPrivateProfileString("ATM", "CURSTAT", "1", strBuf,  sizeof(strBuf), "c:\\atms\\atms.ini");
							GetPrivateProfileString("ATM", "CURSTAT", "1", strBuf, sizeof(strBuf), this->m_atmsFull);
							nMode = strBuf[0] - 0x30;
							
							Logger("원격 리셋 요청을 위한 ATM 현재 모드 검사값 %d", nMode);

							if(nMode != WZ_IDLE && nMode != WZ_MFAULT)	
							{
								memset((char *)&m_TxBuffer[0], 0x00, sizeof(m_TxBuffer));
								memcpy((char *)&m_TxBuffer[0], (char *)this->m_RxBuffer, a2i(&this->m_RxBuffer[0], 10) + 10);
				
								memcpy((char *)&m_TxBuffer[21], "01", 2);	

								if(nMode == WZ_SERVICE)		memcpy((char *)&m_TxBuffer[21], "06", 2);
								else if(nMode == WZ_POWERUP)	memcpy((char *)&m_TxBuffer[21], "07", 2);
								else if(nMode == WZ_OFFLINE)	memcpy((char *)&m_TxBuffer[21], "08", 2);
								else if(nMode == WZ_MFAULT)	memcpy((char *)&m_TxBuffer[21], "09", 2);
								else if(nMode == WZ_MANUAL)	memcpy((char *)&m_TxBuffer[21], "10", 2);
								else				memcpy((char *)&m_TxBuffer[21], "11", 2);

								break;
							}
							
						}
						nRet = SendToRegacySystem(dwRetCode);
						bRecv = true;	// 자동화기기의 응답 대기 모드로
						
						SetSClock(10);

						break;

					case REQ_RCM:	// 이미지전송 모드 변경
					case REQ_JCM:	// 이미지전송 & 저널운영모드 전달
					case REQ_CHG:	// 저널 운영모드 변경 요청 AP에 전달

						Logger("Remote Command received %x", dwRetCode);
						
						if(dwRetCode == REQ_CHG || dwRetCode == REQ_JCM)
						{
							memset(strBuf, 0x00, sizeof(strBuf));
							//GetPrivateProfileString("ATM", "CURSTAT", "1", strBuf,  sizeof(strBuf), "c:\\atms\\atms.ini");
							GetPrivateProfileString("ATM", "CURSTAT", "1", strBuf, sizeof(strBuf), this->m_atmsFull);
							nMode = strBuf[0] - 0x30;
							
							Logger("저널운영 모드변경요청을 위한 ATM 현재 모드 검사값 %d", nMode);

							if(nMode == WZ_SERVICE || nMode == WZ_MANUAL || nMode == WZ_MFAULT)
							{
								memset((char *)&m_TxBuffer[0], 0x00, sizeof(m_TxBuffer));
								memcpy((char *)&m_TxBuffer[0], (char *)this->m_RxBuffer, a2i(&this->m_RxBuffer[0], 10) + 10);
								if(nMode == WZ_SERVICE) memcpy((char *)&m_TxBuffer[21], "06", 2);
								if(nMode == WZ_MANUAL)	memcpy((char *)&m_TxBuffer[21], "10", 2);
								if(nMode == WZ_MFAULT)	memcpy((char *)&m_TxBuffer[21], "09", 2);

								if(dwRetCode == REQ_JCM)	// 이미지 & 저널 전송모드 변경
								{
									EnterCriticalSection(&GCS);
									if(m_SharedMem.DATA[0] == 'R')
										//WritePrivateProfileString("AGENT", "IMGMODE", "R", "c:\\atms\\atms.ini");
										WritePrivateProfileString("AGENT", "IMGMODE", "R", this->m_atmsFull);
									else if(m_SharedMem.DATA[0] == 'C')
									//	WritePrivateProfileString("AGENT", "IMGMODE", "C", "c:\\atms\\atms.ini");
										WritePrivateProfileString("AGENT", "IMGMODE", "C", this->m_atmsFull);
									LeaveCriticalSection(&GCS);
								}

								break;
							}
						}

						memset((char *)&m_TxBuffer[0], 0x00, sizeof(m_TxBuffer));
						memcpy((char *)&m_TxBuffer[0], (char *)this->m_RxBuffer, a2i(&this->m_RxBuffer[0], 10) + 10);
						memcpy((char *)&m_TxBuffer[21], "00", 2);

						SendToRegacySystem(dwRetCode);

						break;

					case REQ_DWN:

					//	Logger("원격 다운로드 메세지 수신");
						memset((char *)&m_TxBuffer[0], 0x00, sizeof(m_TxBuffer));
						memcpy((char *)&m_TxBuffer[0], (char *)this->m_RxBuffer, 50);	// 수신 받은 데이터의 50byte echo
						memcpy((char *)&m_TxBuffer[0], "0000000040", 10);
						memcpy((char *)&m_TxBuffer[21], "00", 2);
						
						break;

					default:
						break;
					}
				}

				if(bRecv)
				{
					memset(&this->m_SharedMem, 0x00, sizeof(TSharedMemory));

					while(1)
					{
						Sleep(300);

						if(GetSClock() <= 0)	
						{
							memset((char *)&m_TxBuffer[0], 0x00, sizeof(m_TxBuffer));
							memcpy((char *)&m_TxBuffer[0], (char *)this->m_RxBuffer, a2i(&this->m_RxBuffer[0], 10) + 10);
				
							memcpy((char *)&m_TxBuffer[21], "01", 2);	
							
							//WritePrivateProfileString("ATM",  "RESULT", "9", "c:\\atms\\atms.ini");
							WritePrivateProfileString("ATM", "RESULT", "9", this->m_atmsFull);

							break;
						}

						nRet = ReadFromRegacySystem(dwRetCode, &this->m_SharedMem);
						
						Logger("원격 리셋 요청에 대한 ATM-->AGENT 응답값 %d", nRet);

						if(nRet == WZ_CONTINUE)	
						{
							Sleep(1000);
							continue;
						}
						else	
						{
							memset((char *)&m_TxBuffer[0], 0x00, sizeof(m_TxBuffer));
							memcpy((char *)&m_TxBuffer[0], (char *)this->m_RxBuffer, a2i(&this->m_RxBuffer[0], 10) + 10);
				
							memcpy((char *)&m_TxBuffer[21], "01", 2);	

							if(nRet == WZ_SUCCESS)	memcpy((char *)&m_TxBuffer[21], "00", 2);
							else			memcpy((char *)&m_TxBuffer[21], "01", 2);
							
						//	WritePrivateProfileString("ATM",  "RESULT", "9", "c:\\atms\\atms.ini");
							WritePrivateProfileString("ATM", "RESULT", "9", this->m_atmsFull);

							break;
						}
					}
				}

				nRet = WriteToSocket(sClient);

				if(nRet != SOCKET_ERROR && dwRetCode == REQ_DWN)
				{
					Logger("파일 다운로드 수신 모드");

					nRet = ReadFromSocket(sClient, true);
					
					memset((char *)&m_TxBuffer[0], 0x00, sizeof(m_TxBuffer));
					memcpy((char *)&m_TxBuffer[0], "0000000040", 10);

					if(nRet == 1)	memcpy((char *)&m_TxBuffer[21], "00", 2);
					else		memcpy((char *)&m_TxBuffer[21], "01", 2);
					
					nRet = WriteToSocket(sClient);
				}
				
				Sleep(1000);

				closesocket(sClient);
			}
		}
	}
public:
	ServerProcessor() : m_sCmdSock(INVALID_SOCKET), m_sClient(INVALID_SOCKET), m_qMutex(), m_nPort(0) { }
	
	ServerProcessor(unsigned int nPort, char cDirve, bool bInCreateAsync = true) : m_sCmdSock(INVALID_SOCKET), m_qMutex()
	{
		m_bInCreateAsync = bInCreateAsync;
		m_cCurrDir = cDirve;
		m_CurrentMode = _UNKNOWN;
		m_bStart = true;
		
		sprintf(this->m_atmsDir, "%c:\\atms\\", cDirve);
		sprintf(this->m_atmsFull, "%c:\\atms\\atms.ini", cDirve);

		Logger("\r\n\r\nInfo: Start Agent Svc remote svc version 1.6.1, 2012-07-04 ----------------------------------");
		Logger("working drive is %c", m_cCurrDir);

		Init(nPort, bInCreateAsync);
	}

	~ServerProcessor()
	{
		closesocket(m_sCmdSock);
	}

	int SendToRegacySystem(DWORD dwCode)
	{
		int nCode = 0;
		
		EnterCriticalSection(&GCS);
		if(dwCode == REQ_JCM)	// 이미지 & 저널 전송모드 변경
		{
			if(m_SharedMem.DATA[1] == 'A')	// 전자 + 종이
				WritePrivateProfileString("AGENT", "MODE", "3", this->m_atmsFull);
			else if(m_SharedMem.DATA[1] == 'P')
				WritePrivateProfileString("AGENT", "MODE", "2", this->m_atmsFull);
			else if(m_SharedMem.DATA[1] == 'E')
				WritePrivateProfileString("AGENT", "MODE", "4", this->m_atmsFull);

			if(m_SharedMem.DATA[0] == 'R')
				WritePrivateProfileString("AGENT", "IMGMODE", "R", this->m_atmsFull);
			else if(m_SharedMem.DATA[0] == 'C')
				WritePrivateProfileString("AGENT", "IMGMODE", "C", this->m_atmsFull);
		}
		else if(dwCode == REQ_RST)	// 원격 리셋 요청
		{
			WritePrivateProfileString("AGENT", "POWER", "1", this->m_atmsFull);
		}
		else if(dwCode == REQ_CHG)	// 저널 운영 모드 변경만
		{
			if(m_SharedMem.DATA[0] == 'A')	// 전자 + 종이
				WritePrivateProfileString("AGENT", "MODE", "3", this->m_atmsFull);
			else if(m_SharedMem.DATA[0] == 'P')
				WritePrivateProfileString("AGENT", "MODE", "2", this->m_atmsFull);
			else if(m_SharedMem.DATA[0] == 'E')
				WritePrivateProfileString("AGENT", "MODE", "4", this->m_atmsFull);

		}
		else if(dwCode == REQ_RCM)	// 이미지 전송 모드 변경만,,..
		{	
			if(m_SharedMem.DATA[0] == 'R')
				WritePrivateProfileString("AGENT", "IMGMODE", "R", this->m_atmsFull);
			else if(m_SharedMem.DATA[0] == 'C')
				WritePrivateProfileString("AGENT", "IMGMODE", "C", this->m_atmsFull);
		}
		LeaveCriticalSection(&GCS);

		return nCode;
	}

	// 원격 리셋 요청했을때만 결과값을 확인함...
	int ReadFromRegacySystem(DWORD dwCode, TSharedMemory *sharedMem)
	{
		int nCode = WZ_CONTINUE;
		char szBuffer[3] = {0, };
		int nResult = 0;

		EnterCriticalSection(&GCS);
		if (dwCode == REQ_RST)	GetPrivateProfileString("ATM", "RESULT", "9", szBuffer, sizeof(szBuffer), this->m_atmsFull);
		LeaveCriticalSection(&GCS);

		nResult = szBuffer[0] - 0x30;
		
		Logger("ATM, RESULT %d", nResult);

		if(nResult == 7)	nCode = WZ_SUCCESS;
		else if(nResult == 8)	nCode = WZ_FAILED;
		else if(nResult == 9)	nCode = WZ_CONTINUE;
		
		return nCode;
	}

	int FindCmdData(unsigned char *pKey, unsigned char *pBuffer, DWORD dwCode)
	{
		unsigned char szKey[MAX_PATH] = {0, };
		unsigned char szPath[MAX_PATH] = {0, };
		char m_szServAddr[MAX_PATH] = {0, };
	//	char m_cCurrDir;
		WIN32_FIND_DATA wd;
		HANDLE hFind;
		FILE *fp;
		long nFileSize;
		int nLen;
		SYSTEMTIME st;
		string str = (char *)pKey;
		char szTgtFile[MAX_PATH] = {0, };
		char szBatch[MAX_PATH] = {0, };
		DWORD dwRet;
		bool bRet = false;

		GetLocalTime(&st);

		GetPrivateProfileStringA("ATMS", "ATMSADDR", "0.0.0.0", &m_szServAddr[0], sizeof(m_szServAddr), this->m_atmsFull);
		
		char tmp_dirve = 'd';

	//	if(strncmp(m_szServAddr, "0.0.0.0", 7) == 0)	// dirve C --> D로
	//		tmp_dirve = 'd';
	//	else	
	//		tmp_dirve = 'c';

	//	szKey[0] = tmp_dirve;
		szKey[0] = m_cCurrDir;

		// 송신 전문 셋팅
		memcpy(m_TxBuffer, this->m_RxBuffer, 58);	// lcode, scode 까지 copy해노코... 씨발 -_-;;;
		memcpy((char *)&m_TxBuffer[21], "04", 2);
		i2a(490, (char *)&m_TxBuffer[0], 10, 10);	// 기본 보낼데이터의 기본길이 셋팅
		
		if(dwCode == REQ_IMG)
		{
			nLen = str.find(" ");
			if(nLen > 0)	str = str.substr(0, nLen);
		}
		else	
		{	// 2009.12.27 수정,...
			nLen = str.find_last_not_of(" ");
			if(nLen > 0)	str = str.substr(0, nLen + 1);

			Logger("str len %d, str %s", str.length(), str.c_str());
		}

		// 2009.12.27 추가, BATCH 파일 실행
		if(dwCode == REQ_FIL)
		{
			// 배치 실행 명령어
			memset(szBatch, 0x00, sizeof(szBatch));
			memcpy(szBatch, str.c_str(), min(255, str.length()));

			if(szBatch[0] == 'z' || szBatch[0] == 'Z')
			{
				Logger("szBatch %s", szBatch);

				bRet = ProcessBatch(m_cCurrDir, szBatch);
				if(bRet == true)
				{
					memcpy(m_TxBuffer, this->m_RxBuffer, 50);	// lcode, scode 까지 copy해노코... 씨발 -_-;;;
					memcpy((char *)&m_TxBuffer[21], "00", 2);
					i2a(40, (char *)&m_TxBuffer[0], 10, 10);	// 기본 보낼데이터의 기본길이 셋팅
					return 1;
				}
				else	
				{
					memcpy(m_TxBuffer, this->m_RxBuffer, 50);	// lcode, scode 까지 copy해노코... 씨발 -_-;;;
					memcpy((char *)&m_TxBuffer[21], "02", 2);
					i2a(40, (char *)&m_TxBuffer[0], 10, 10);	// 기본 보낼데이터의 기본길이 셋팅
					return 1;
				}
			}
		}

		if(dwCode == REQ_FIL || dwCode == REQ_IMG)
		{
			if(dwCode == REQ_IMG)				//0         1         2
			{						//0123456789012345678901234
				szPath[0] = szKey[0];			//20091013121314123_I01.JP2
				sprintf((char *)&szPath[1], ":\\atms\\img\\%.6s\\%.2s\\%.21s.jpg", str.substr(0, 6).c_str(), str.substr(6, 2).c_str(), str.substr(0, str.length()).c_str());
				Logger("Tst file name : %s", (char *)szPath);
			}
			else	
			{
				sprintf((char *)szPath, "%s", str.c_str());
				Logger("Tst file name(Free format file request) %s", str.c_str());
			}
			
			hFind = FindFirstFile((char *)szPath, &wd);
			if(hFind == INVALID_HANDLE_VALUE)
			{
				if(dwCode == REQ_IMG)
				{
					Logger("file not found %s", szPath);

					sprintf((char *)&szPath[1], ":\\atms\\img\\%.6s\\%.2s\\%.21s.jp2", 
							str.substr(0, 6).c_str(), str.substr(6, 2).c_str(), str.substr(0, str.length()).c_str());
					
					Logger("file search retry %s", szPath);

					hFind = FindFirstFile((char *)szPath, &wd);
					if(hFind == INVALID_HANDLE_VALUE)
					{
						memcpy((char *)&m_TxBuffer[21], "04", 2);
						return 0;
					}
				}
				else		
				{
					memcpy((char *)&m_TxBuffer[21], "04", 2);
					return 0;
				}
			}

			
			if(dwCode == REQ_FIL)
			{
				nFileSize = ((wd.nFileSizeHigh == 0) ? wd.nFileSizeLow : wd.nFileSizeHigh);
				
			//	if(nFileSize > 10000000)
				if(nFileSize > 7000000)
				{
					FindClose(hFind);
					memcpy((char *)&m_TxBuffer[21], "05", 2);
					return 0;
				}
			}

			// 20091012121212123_C01.JPG
			FindClose(hFind);
			
			if(dwCode == REQ_IMG)
			{
				strcpy(szTgtFile, (char *)szPath);
				szTgtFile[min(strlen(szTgtFile) - 1, MAX_PATH - 1)] = '2';

				Logger("파일 압축, %s -> %s", szPath, szTgtFile);

				EnterCriticalSection(&GCS);
				dwRet = vis_jp2_encoder((char *)szPath, (char *)szTgtFile, "0.5");	// jpg -> jp2
				LeaveCriticalSection(&GCS);

				if(dwRet == 1)
				{
					Logger("원본파일 삭제 %s", szPath);
					DeleteFile((char *)szPath);	// jpg 삭제

					CopyFile((char *)szTgtFile, (char *)szPath, FALSE);
					Logger("타겟 파일 %s -> 복사 %s", szTgtFile, szPath);
					DeleteFile(szTgtFile);
				}
			}

			fp = fopen((char *)szPath, "r+b");
			if(fp == NULL)	
			{
				Logger("파일 오픈, %s 실패", szPath);
				return 0;
			}
			Logger("파일 오픈, %s 성공", szPath);
				
			if(dwCode == REQ_IMG)	nFileSize = _filelength(_fileno(fp));
			
			memcpy((char *)&m_TxBuffer[21], "00", 2);
			
			memset(&m_TxBuffer[58], 0x20, 256);

			if(dwCode == REQ_IMG)
			{
				memcpy((char *)&m_TxBuffer[58], str.c_str(), str.length());
				Logger("응답 전송 파일 이름 : %s", str.c_str());
			}
			else	
			{
				nLen = str.find_last_of('\\');
				if(nLen > 0)
				{
					int iTotal = str.length();
					
					iTotal = iTotal - nLen - 1;

					if(iTotal > 0)
					{
						memset((char *)&m_TxBuffer[58], 0x20, 256);
						memcpy((char *)&m_TxBuffer[58], str.substr(nLen + 1, iTotal).c_str(), iTotal);
						Logger("Uploading file name : %s", str.substr(nLen + 1, iTotal).c_str());
					}
					else	return 0;
				}
				else return 0;
			}

			// 파일 싸이즈 셋팅
			i2a(nFileSize, (char *)&m_TxBuffer[314], 10, 10);
			
			nLen = fread((char *)&m_TxBuffer[324], nFileSize, 1, fp);
			
			Logger("File Size %d, fread return value %d", nFileSize, nLen);

			fclose(fp);

			nFileSize = nFileSize + 324 - 10;	// 자시자신 제외
			i2a(nFileSize, (char *)&m_TxBuffer[0], 10, 10);

			return 1;
		}

		return 0;
	}

	BOOL IsFile(const char *dir)
	{
		if(strstr(dir, ".LOG") != NULL)	return TRUE;
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
		sprintf(&strPath[strlen(strPath)], "%02d\\RemoteSvc%02d.LOG", st.wMonth, st.wDay);

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

			memset(strPath, 0x00, MAX_PATH);
			strPath[0] = m_cCurrDir;
			strcat(strPath, ":\\atms\\log\\");
			sprintf(&strPath[strlen(strPath)], "%02d\\RemoteSvc%02d.LOG", t->tm_mon + 1, t->tm_mday);
			DeleteFile(strPath);

			m_bStart = false;
		}

		return true;
	}

	bool ProcessBatch(char drive, char *pBatch)
	{
		const char seps[] = ";";
		char *token = NULL;
		char szCmd[MAX_PATH];
		char szPath[MAX_PATH];
		string str;

		memset(szCmd, 0x00, sizeof(szCmd));

		token = strtok(pBatch, seps);

		while(token != NULL)
		{
			Sleep(2);
			
			if(strncmp(token, "z", 1) != 0)
			{
				strcat(szCmd, token);
				strcat(szCmd, "\r\n");
			}

			token = strtok(NULL, seps);
		}

		Logger("Batch string is %s", szCmd);
		
		// save batch file, c or d :\\atms\\agent\\remote_auto.bat file...

		FILE *fp = NULL;
		memset(szPath, 0x00, sizeof(szPath));
		sprintf(szPath, "%c:\\atms\\agent\\remote_auto.bat", drive);
		
		Logger("batch file path %s", szPath);

		fp = fopen(szPath, "w+b");
		if (fp == NULL)	{
			Logger("batch file creation error, return null");
			return false;
		}
		fwrite(szCmd, strlen(szCmd), 1, fp);
		fclose(fp);

		return ExecuteBatchJob(szPath);
	}

	bool ExecuteBatchJob(char *pJob)
	{
		STARTUPINFO si;
		PROCESS_INFORMATION pi;

		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));

		// Start the child process. 
		if(!CreateProcess(NULL,
				  pJob,
				  NULL,
				  NULL,
				  FALSE,
			          0,	
			          NULL,	
			          NULL,	
				  &si,	
				  &pi))
		{
			Logger("CreateProcess failed %d", GetLastError());
			return false;
		}

		WaitForSingleObject(pi.hProcess, 3 * 1000);	// 1분...

		// Close process and thread handles. 
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		
		DeleteFile(pJob);

		return true;
	}

	void i2a(int xv, char *dst, int radix, int size)
	{
		char	sz[20] = {0};
		int	len, idx = 0;

		memset(dst, 0x30, size);

		itoa(xv, sz, radix);
	
		len = strlen(sz);

		if(len > size)	return;

		idx = size - len;
	
		memcpy(&dst[idx], sz, len);
	}

	void SetSClock(int nTime)
	{
		m_nSStartTime = clock();
		m_nSTimeCount = nTime;                 /* time defined -----------------*/
	}

	int GetSClock()
	{
		m_nSLastTime = clock();
		return (m_nSTimeCount - ((m_nSLastTime - m_nSStartTime) / CLOCKS_PER_SEC));
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
	
	int VerifyData(unsigned char *pBuffer)
	{
		if(strncmp((char *)&pBuffer[10], "WZ04", 4) == 0)	return 0;	// 비정상 리턴

		memset(&this->m_SharedMem, 0x00, sizeof(TSharedMemory));

		memcpy(this->m_SharedMem.LCODE, &pBuffer[50], 4);
		memcpy(this->m_SharedMem.SCODE, &pBuffer[54], 4);
		memcpy(this->m_SharedMem.DATA, &pBuffer[58], 255);
		this->m_SharedMem.DATA[255] = 0X00;

		Logger("Remote cmd LCode %.4s", this->m_SharedMem.LCODE);
		Logger("Remote cmd SCode %.4s", this->m_SharedMem.SCODE);
		Logger("Remote cmd DATA %s", this->m_SharedMem.DATA);

		if(a2i(this->m_SharedMem.LCODE, 4) == 1000 && a2i(this->m_SharedMem.SCODE, 4) == 1100)	
			return REQ_IMG;	// 이미지 요청
		else if(a2i(this->m_SharedMem.LCODE, 4) == 1000 && a2i(this->m_SharedMem.SCODE, 4) == 1200)	
			return REQ_FIL;	// 비규격 데이터 요청
		else if(a2i(this->m_SharedMem.LCODE, 4) == 2000 && a2i(this->m_SharedMem.SCODE, 4) == 2100)	
			return REQ_CHG;	// 저널운영모드 변경
		else if(a2i(this->m_SharedMem.LCODE, 4) == 2000 && a2i(this->m_SharedMem.SCODE, 4) == 2200)	
			return REQ_RST;	// 원격리셋 요청
		else if(a2i(this->m_SharedMem.LCODE, 4) == 2000 && a2i(this->m_SharedMem.SCODE, 4) == 2300)	
			return REQ_RCM; // 이미지 전송 모드 변경
		else if(a2i(this->m_SharedMem.LCODE, 4) == 2000 && a2i(this->m_SharedMem.SCODE, 4) == 2400)	
			return REQ_JCM; // 이미지 & 저널 전송 모드 변경
		else if(a2i(this->m_SharedMem.LCODE, 4) == 9000 && a2i(this->m_SharedMem.SCODE, 4) == 9000)	
		{
			// 2012-07-04 crc check code 추가
			memcpy(this->m_SharedMem.CRC_CODE, "99999", 5);
			memcpy(this->m_SharedMem.CRC_CODE, &pBuffer[14], 5);

			return REQ_DWN;
		}
		else	return 0;

		return 0;
	}

	int WriteToSocket(SOCKET sSocket)
	{
		int dwRet = SOCKET_ERROR;
		int nLeft, nTotal, nReq;
		bool bFirst = true;

		nLeft = nTotal = 0;
		nReq = a2i(&m_TxBuffer[0], 10) + 10;

		if(this->m_bInCreateAsync == true)	SetClock(15);

		while(nTotal < nReq)
		{
			Sleep(250);

			dwRet = send(sSocket, (char *)&m_TxBuffer[nLeft], nReq - nLeft, 0);
			if(dwRet != SOCKET_ERROR  && dwRet > 0)
			{
				nLeft += dwRet;
				nTotal += dwRet;

				Logger("SUCCESS : Write success to server");
			}
			else
			{
				if(GetClock() > 0)	continue;
				else			return SOCKET_ERROR;
			}
		}

		return dwRet;
	}
	
	int ReadFromSocket(SOCKET sSocket, bool bSave)
	{
		int ret = SOCKET_ERROR;
		int left, total, req, fsize, verify;
		bool settings = false;
		WIN32_FIND_DATA wd;
		HANDLE hFind;
		string rootPath = "c:\\pkgapdn\\";
		string srcDir;
		string tmpDir;
		BOOL bGood = TRUE;
		char str[MAX_PATH] = {0, };
		string file_name;
		FILE *fp;
		UCHAR szBuffer[1024 * 8] = {0};
		left = total = req = 0;
		
		srcDir = rootPath;
		srcDir += "*.*";

		hFind = FindFirstFile("c:\\pkgapdn", &wd);
		if(hFind == INVALID_HANDLE_VALUE)
			CreateDirectory("c:\\pkgapdn", NULL);
		else	FindClose(hFind);

		hFind = FindFirstFile(srcDir.c_str(), &wd);
		if(hFind != INVALID_HANDLE_VALUE)
		{
			while(bGood)
			{
				Sleep(1);

				tmpDir = rootPath;
				tmpDir += wd.cFileName;

				DeleteFile(tmpDir.c_str());

				bGood = FindNextFile(hFind, &wd);
			}

			FindClose(hFind);
		}

		memcpy(str, &this->m_SharedMem.DATA[0], 30);
		for(int i = 0; i < 30; i++)
		{
			if(str[i] == 0x20)
			{
				str[i] = 0x00;
				break;
			}
		}

		file_name = str;		

		memset(str, 0x00, sizeof(str));
		memcpy(str, &this->m_SharedMem.DATA[30], 10);

		fsize = a2i((UCHAR *)str, 10);

		Logger("Info: 다운로드 파일 %s 싸이즈 %d, 다운로드 시작", file_name.c_str(), fsize);

		req = fsize;
		total = left = 0;

		sprintf(str, "%c:\\atms\\down_tmp", this->m_cCurrDir);
		hFind = FindFirstFile(str, &wd);
		if(hFind == INVALID_HANDLE_VALUE)	
			CreateDirectory(str, NULL);
		else	FindClose(hFind);

		tmpDir = str;
		tmpDir += "\\";
		tmpDir += file_name;

		fp = fopen(tmpDir.c_str(), "w+b");
		if(fp == NULL)	
		{
			Logger("Error: 다운로드 임시 %s 파일 생성 에러 발생, fopen", tmpDir.c_str());
			return SOCKET_ERROR;
		}

		SetClock(160);

		while(total < req)
		{
			Sleep(1);

			if(GetClock() <= 0)
			{
				Logger("Error: 다운로드 명령어, recv 오버타임 발생, 임시 파일 삭제, 에러 리턴");
				fclose(fp);

				DeleteFile(tmpDir.c_str());

				return SOCKET_ERROR;
			}

			memset(szBuffer, 0x00, sizeof(szBuffer));

			ret = recv(sSocket, (char *)szBuffer, sizeof(szBuffer), 0);
			if(ret > 0)
			{
				verify = fwrite(szBuffer, ret, 1, fp);
				if(verify != 1)
				{
					fclose(fp);

					Logger("Error: 다운로드 %s 임시파일 쓰기 에러 발생, fwrite, 에러리턴", tmpDir.c_str());

					DeleteFile(tmpDir.c_str());
					
					return SOCKET_ERROR;
				}
				
				total += ret;

				SetClock(10);
			}
		}

		fclose(fp);
		fp = NULL;

		string crc_code, host_crc_code;

		if(req == total)
		{
			if(strncmp((char *)m_SharedMem.CRC_CODE, "99999", 5) == 0)
			{
				srcDir = rootPath;
				srcDir += file_name;

				Logger("Info: Host crc code 99999, CRC Code 체크 생략, %s file --> %s file copy", tmpDir.c_str(), srcDir.c_str());

				bGood = CopyFile(tmpDir.c_str(), srcDir.c_str(), FALSE);
				if(bGood)
				{
					// 다운로드 에이젼트에 다운로드 알림
					file_name = "c:\\pkgapdn\\dwn.ok";
					
					fp = fopen(file_name.c_str(), "w+b");
					if(fp == NULL)
					{
						DeleteFile(srcDir.c_str());
						Logger("Error: %s file fopen error occured", file_name.c_str());

						return SOCKET_ERROR;
					}

					if(fp)	fclose(fp);

					Logger("Succ: %s Down load file 생성 완료, Host에 완료 보고", srcDir.c_str());

					return 1;
				}
				else
				{
					Logger("Error: %s file --> %s file copy error occured", tmpDir.c_str(), srcDir.c_str());
					
					return SOCKET_ERROR;
				}
			}
			else
			{
				memset(str, 0x00, sizeof(str));
				memcpy(str, m_SharedMem.CRC_CODE, 5);

				host_crc_code = str;
				
				Logger("Info: Host crc code %.5s, CRC Code 체크 시작, %s file", str, tmpDir.c_str());

				crc_code = MakeCrcCode(tmpDir);
				if(crc_code.compare(host_crc_code) == 0)
				{
					Logger("Succ: Host crc code %s, Local crc code %s same, success", host_crc_code.c_str(), crc_code.c_str());

					srcDir = rootPath;
					srcDir += file_name;

					bGood = CopyFile(tmpDir.c_str(), srcDir.c_str(), FALSE);
					if(bGood)
					{
						// 다운로드 에이젼트에 다운로드 알림
						file_name = "c:\\pkgapdn\\dwn.ok";
						
						fp = fopen(file_name.c_str(), "w+b");
						if(fp == NULL)
						{
							DeleteFile(srcDir.c_str());
							Logger("Error: %s file fopen error occured", file_name.c_str());

							return SOCKET_ERROR;
						}

						if(fp)	fclose(fp);

						Logger("Succ: %s Down load file 생성 완료, Host에 완료 보고", srcDir.c_str());

						return 1;
					}
					else
					{
						Logger("Error: %s file --> %s file copy error occured", tmpDir.c_str(), srcDir.c_str());
						
						return SOCKET_ERROR;
					}
				}
				else
				{
					Logger("Error: Host crc code %s, Local crc code %s not equal, error occured", host_crc_code.c_str(), crc_code.c_str());

					DeleteFile(tmpDir.c_str());

					return SOCKET_ERROR;
				}
			}
		}
		else
		{
			Logger("Fault: 다운로드 파일 크기 이상 발생, 삭제 후 에러 리턴");

			DeleteFile(tmpDir.c_str());

			return SOCKET_ERROR;
		}

		return SOCKET_ERROR;

	//	int nRet = SOCKET_ERROR;
	//	FILE *fp = NULL;
	//	string strFileName;
	//	string strTgtFile;
	//	int nFileSize;
	//	char szTemp[MAX_PATH] = {0, };
	//	int nLeft, nTotal, nReq;
	//	char szBuffer[4096] = {0, };
	//	bool bFst = true;
	//	int v = 0;

		/*
		memcpy(szTemp, &this->m_SharedMem.DATA[0], 30);
		
		// file 이름 추출
		for(nLeft = 0; nLeft < 30; nLeft++)
		{
			if(szTemp[nLeft] == 0x20)	
			{
				szTemp[nLeft] = 0x00;
				break;
			}
		}

		strFileName = szTemp;	// file 이름

		Logger("다운로드 매체 파일 이름 %s", strFileName.c_str());

		// 파일 크기 추출
		memset(szTemp, 0x00, sizeof(szTemp));
		memcpy(szTemp, &this->m_SharedMem.DATA[30], 10);

		nFileSize = a2i((unsigned char *)szTemp, 10);
		Logger("다운르도 매체 파일 크기 %d", nFileSize);


		nReq = nFileSize;
		nTotal = nLeft = 0;
		
		strTgtFile = "c:\\pkgapdn\\";
		CreateDirectory(strTgtFile.c_str(), NULL);
		strTgtFile += strFileName;

		Logger("다운로드 파일 저장 이름 %s", strTgtFile.c_str());

		fp = fopen(strTgtFile.c_str(), "w+b");
		if(fp == NULL)	
		{
			Logger("다운로드 파일 열기 실패!!! : %s", strTgtFile.c_str());
			return SOCKET_ERROR;
		}
		else	Logger("다운로드 파일 열기 성공!!! : %s", strTgtFile.c_str());

		this->SetClock(160);

		Logger("다운로드 파일 요청 크기 %d", nReq);

		while(nTotal < nReq)
		{
			Sleep(2);
			
			if(GetClock() <= 0)	
			{
				if(fp)	fclose(fp);
				fp = NULL;
				DeleteFile(strTgtFile.c_str());

				return SOCKET_ERROR;
			}
			
			memset(szBuffer, 0x00, sizeof(szBuffer));

			nRet = recv(sSocket, szBuffer, sizeof(szBuffer), 0);
			if(nRet > 0)
			{
				if(fp)
				{
					v = fwrite(szBuffer, nRet, 1, fp);
					if(v != 1)
					{
						fclose(fp);

						Logger("다운로드 파일 %s 쓰기 실패", strTgtFile.c_str());
						DeleteFile(strTgtFile.c_str());

						return SOCKET_ERROR;
					}
				}

				nTotal += nRet;

				SetClock(10);
			}
			else 
			{
				Logger("다운로드 recv function error");
				
				continue;
			}
		}
		
		if(fp)	fclose(fp);	// 다운로드 파일 save ok
		fp = NULL;

		Logger("읽기 완료 크기 %d, 다운로드 파일 크기 %d", nTotal, nFileSize);

		if(nTotal == nReq)	// ok... 다운로드...
		{
			Sleep(2000);

			// 다운로드 에이젼트에 다운로드 알림
			strFileName = "c:\\pkgapdn\\dwn.ok";
			fp = fopen(strFileName.c_str(), "w+b");
			if(fp == NULL)
			{
				DeleteFile(strTgtFile.c_str());
				return SOCKET_ERROR;
			}

			if(fp)	fclose(fp);

			Logger("매체 파일 다운로드 완료!!!");

			return 1;
		}
		else
		{
			Logger("매체 다운로드 매체 싸이즈, 이상, 삭제함 %s", strTgtFile.c_str());

			DeleteFile(strTgtFile.c_str());
			return SOCKET_ERROR;
		}

		return nRet;
		*/
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
			return "99999";
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

			WaitForSingleObject(pi.hProcess, 1000 * 60 * 5);	// 5분대기
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
		else	
		{
			Logger("Error: %s command call error occured", cmd.c_str());
			return "99999";
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
			return "99999";
		}

		fread(crc_code, 5, 1, fp);
		fclose(fp);

		tmp = crc_code;

		return tmp;
	}

	int ReadFromSocket(SOCKET sSocket)
	{
		int dwRet = SOCKET_ERROR;
		int nLeft, nTotal, nReq;
		bool bFirst = true;

		nLeft = nTotal = 0;
		nReq = 2048;
		
		memset(m_RxBuffer, 0x00, sizeof(m_RxBuffer));

		if(this->m_bInCreateAsync == true)	SetClock(15);

		while(nTotal < nReq)
		{
			Sleep(250);

			dwRet = recv(sSocket, (char *)&this->m_RxBuffer[nLeft], nReq - nLeft, 0);
			if(dwRet != SOCKET_ERROR && dwRet > 0)
			{
				Logger("Remote received data length %d", dwRet);

				if(bFirst)
				{
					nReq = a2i(&m_RxBuffer[0], 10) + 10;	// 자기 자신은 제외된 크기..
					bFirst = false;

					Logger("Sender sending data length %d", nReq);
				}

				nLeft += dwRet;
				nTotal += dwRet;

				Logger("nLeft data length %d", nLeft);
				Logger("nTotal data length %d", nTotal);
			}
			else	
			{
				if(this->m_bInCreateAsync == true)
				{
					if(GetClock() > 0)	continue;
					else			return SOCKET_ERROR;
				}
				else				return SOCKET_ERROR;
			}
		}

		return nTotal;
	}

	int a2i(unsigned char *buff, int nLength)
	{
		unsigned char szBuffer[11] = {0, };

		memcpy(szBuffer, buff, min(10, nLength));

		return atoi((char *)szBuffer);
	}

	bool Init(unsigned int nPort, bool bInCreateAsync)
	{
		bool bRet = true;
		u_long iMode = 0;

		if(bInCreateAsync == true)	iMode = 1;

		memset((char *)&mServerAddr, 0x00, sizeof(mServerAddr));
		
		this->m_nPort = nPort;

		mServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		mServerAddr.sin_family = AF_INET;
		mServerAddr.sin_port = htons((u_short)m_nPort);
			
		if((m_sCmdSock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
			return false;
			
		if(bind(m_sCmdSock, (struct sockaddr *)&mServerAddr, sizeof(mServerAddr)) < 0)		
		{
			closesocket(m_sCmdSock);
			
			return false;
		}
		
		if(listen(m_sCmdSock, 5) == SOCKET_ERROR)	// event queue 5 panding...- -+
		{
			closesocket(m_sCmdSock);
			
			return false;
		}

	//	if(bInCreateAsync)	if(ioctlsocket(m_sCmdSock, FIONBIO, &iMode) != 0)	return false;
			
		return bRet;
	}

	SOCKET Accept()
	{	
		int nLen;

		nLen = sizeof(mClientAddr);
		m_sClient = accept(m_sCmdSock, (struct sockaddr *)&mClientAddr, &nLen);

		return m_sClient;
	}

	void CloseSocket()
	{
		Logger("Completion client socket to be close");

		if(this->m_sClient != INVALID_SOCKET)	
		{
			shutdown(m_sClient, 2);
			closesocket(m_sClient);
			m_sClient = INVALID_SOCKET;

			Logger("m_sClient socket closed");
		}
	}
};

#endif // __SIMPLE_COMMANDER_H_INCLUDED__