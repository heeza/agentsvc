#ifndef __CSIMPLECASTER_H_INCLUDED__
#define __CSIMPLECASTER_H_INCLUDED__

//#define DEBUGMODE

#include "inc/threading.h"
#include "inc/log.h"
#include "inc/simple_define.h"
#include "inc/flog.h"
#include "inc/sys_info.h"
#include "inc/simple_commander.h"
#include "inc/J2KEncoder.h"
#include "inc/J2KDecoder.h"
#include "inc/simple_debug.h"
#include "inc/simple_winpatcher.h"
#include <time.h>
#include <io.h>
#include <fcntl.h>
#include <vector>
#include <queue>
#include <deque>
#include <string>

#define BUFF_SIZE 1024 * 10
#define IMAGE_RETRY	1024 * 10

CRITICAL_SECTION GCS;

typedef struct _tagMYIMAGEINFO
{
	DWORD id;
	unsigned int nCount;
	unsigned char szPath[MAX_PATH];
	void Clear() { ::memset(this, 0x00, sizeof(MYIMAGEINFO)); }
} MYIMAGEINFO, *PMYIMAGEINFO;

deque<MYIMAGEINFO> imageQueue;

typedef struct _tagJnlMaster
{	// 0000000040 WZ01 0111 653 0310.2.105.214
	char m_szDataLen[10];		//  0
	char m_szMsgId[4];		// 10
	char m_szClientId1[4];		// 14
	char m_szClientId2[3];		// 18
	char m_szMsgResult[2];		// 21
	char m_szClientIp[15];		// 
	char m_szReserved[12];		// 20091224
	BYTE m_szData[BUFF_SIZE];
} JnlMaster;

typedef struct _tagJnlInfo
{
	char m_szLocalDate[20];
	char m_szLastJnlId[20];	// Last Send Journal
	char m_ImgSendMode;
	char m_JnlOperation;
	int  m_nImageKeep;
	int  m_nJnlKeep;
	char m_szAssetID[11];
} JnlInfo;


enum CD_STATUS	{ CD_INIT = 1, CD_OFF, CD_IDLE, CD_TRANS, CD_SUPER, CD_TROUBLE, CD_UNKNOWN, };

// 1 : 초기화중 (0)
// 2 : 오프라인
// 3 : 거래대기
// 4 : 거래중 (0)
// 5 : 계원모드
// 6 : 장애모드
// 7 : 기타

//typedef struct _tagCDStatus
class CDSTATUS
{
public:
	bool m_bTroubleSend;
	CD_STATUS m_CdOldStatus;
	CD_STATUS m_CdNewStatus;
	clock_t m_nLastTime, m_nStartTime;
	int m_nTimeCount;
	unsigned char m_szMessage[2048];
	char m_szSendTime[24];
	char m_szClearTime[24];
	char m_drive;

	CDSTATUS() 
	{ 
		m_CdNewStatus = m_CdOldStatus = CD_UNKNOWN; 
		m_bTroubleSend = false;
	}

	CDSTATUS(char drive)
	{
		m_CdNewStatus = m_CdOldStatus = CD_UNKNOWN;
		m_bTroubleSend = false;
		m_drive = drive;
	}

	void Clear() 
	{ 
		m_CdNewStatus = m_CdOldStatus = CD_UNKNOWN;
		m_bTroubleSend = false;
	}

	void SetState(CD_STATUS stat)
	{
		if(m_CdOldStatus != stat)
			m_CdOldStatus = m_CdNewStatus;

		m_CdNewStatus = stat;
	}
	
	CD_STATUS GetState()
	{
		return m_CdNewStatus;
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

	bool IsSend()
	{
		char szChar[10] ={0, };
		int i;
		char tmp[50] = { 0 };

		sprintf(tmp, "%c:\\atms\\ll_trb.ini", m_drive);
		GetPrivateProfileString("ATMS", "LLTROUBLE", "0", szChar, sizeof(szChar), tmp);
		
		i = szChar[0] - 0x30;
		if(i == 1)	this->m_bTroubleSend = true;
		else
		{
			if(this->m_bTroubleSend == true)	
				return this->m_bTroubleSend;
			else					
				this->m_bTroubleSend = false;
		}

		return this->m_bTroubleSend;
	}

	void SetSendStat(char *pCDWritedTime)
	{
		char tmp[50] = { 0 };
		this->m_bTroubleSend = true;

		sprintf(tmp, "%c:\\atms\\ll_trb.ini", m_drive);
		WritePrivateProfileString("ATMS", "LLTROUBLE", "1", tmp);
		WritePrivateProfileString("ATMS", "LLSENDTIME", this->m_szSendTime, tmp);
		WritePrivateProfileString("ATMS", "CDWRITEDTIME", pCDWritedTime, tmp);
	}

	void ReSetSendStat()
	{
		char tmp[50] = { 0 };
		sprintf(tmp, "%c:\\atms\\ll_trb.ini", m_drive);
		WritePrivateProfileString("ATMS", "LLTROUBLE", "0", tmp);
		
		this->m_bTroubleSend = false;
	}

	char *GetMessage()	
	{
		return (char *)&m_szMessage[0];
	}

	char *GetSendTime()
	{
		return (char *)&m_szSendTime[0];
	}

	void SetClearTime(char *pszDateTime)
	{
		memset(m_szClearTime, 0x00, sizeof(m_szClearTime));
		memcpy(m_szClearTime, pszDateTime, 17);
	}

	char *GetClearTime()
	{
		SYSTEMTIME st;
		char szClearTime[24] = { 0, };

		GetLocalTime(&st);

		sprintf(szClearTime, "%04d%02d%02d%02d%02d%02d%03d", st.wYear, st.wMonth, st.wDay, st.wHour,
			st.wMinute, st.wSecond, st.wMilliseconds);

		strcpy(m_szClearTime, szClearTime);
		return &m_szClearTime[0];
	}
	
	int MakeTroubleMessage(const char *pszJumCode, 
		               const char *pszAtmNum, 
			       const char *pszIpAddr, 
			       const char *pVendor, 
			       const char *pCDKind, 
			       int nMode)
	{
		SYSTEMTIME st;
		char szClearTime[24] = {0, };
		int i;
		bool bGood = true;

		GetLocalTime(&st);

		memset(this->m_szMessage, 0x00, sizeof(m_szMessage));
		
		memcpy(&this->m_szMessage[ 0], "0000000073", 10);
		memcpy(&this->m_szMessage[10], "WZ08", 4);
		memcpy(&this->m_szMessage[14], &pszJumCode[0], 4);
		memcpy(&this->m_szMessage[18], &pszAtmNum[0], 3);
		memcpy(&this->m_szMessage[21], "00", 2);
		memcpy(&this->m_szMessage[23], &pszIpAddr[0], min(strlen(pszIpAddr), 15));
		
		memcpy(&this->m_szMessage[50], &pszJumCode[0], 4);
		memcpy(&this->m_szMessage[54], &pszAtmNum[0], 3);

		if(nMode == 0 || nMode == 2)
		{
			sprintf(this->m_szSendTime, "%04d-%02d-%02d %02d:%02d:%02d.%03d", st.wYear, st.wMonth, st.wDay, st.wHour, 
							st.wMinute, st.wSecond, st.wMilliseconds);
		}
		else	
		{
			memcpy(szClearTime, GetClearTime(), min(17, strlen(GetClearTime())));

			for(i = 0; i < 17; i++)	
			{
				if(isdigit(szClearTime[i]))	
					continue;
				else	
				{
					bGood = false;
					break;
				}
			}

			if(bGood)
			{
				sprintf(this->m_szSendTime, "%.4s-%.2s-%.2s %.2s:%.2s:%.2s.%.3s", &szClearTime[0], &szClearTime[4], &szClearTime[6], &szClearTime[8], 
					&szClearTime[10], &szClearTime[12], &szClearTime[14]);
			}
			else
			{
				sprintf(this->m_szSendTime, "%04d-%02d-%02d %02d:%02d:%02d.%03d", st.wYear, st.wMonth, st.wDay, st.wHour, 
					st.wMinute, st.wSecond, st.wMilliseconds);
			}
		}

		strcpy((char *)&this->m_szMessage[57], m_szSendTime);

		this->m_szMessage[80] = pVendor[0];
		this->m_szMessage[81] = pCDKind[0];

		if(nMode == 0)	// 발생
			this->m_szMessage[82] = 'O';
		else		// 해제
			this->m_szMessage[82] = 'C';

		return 83;
	}

	int increase_date(char *finame)
	{
	int	year, month, date;

		year = a2i(&finame[0], 4);
		month = a2i(&finame[4], 2);
		date = a2i(&finame[6], 2);

		if(date == 31)	
		{
			date = 1;

			if(month == 12)	
			{
				month = 1;
				year++;
			}
			else	month++;
		}
		else	date++;

		n2a(year,  &finame[0], 4, 10);
		n2a(month, &finame[4], 2, 10);
		n2a(date,  &finame[6], 2, 10);

		return 0;
	}

	int a2i(char *pszSrc, int nLength)
	{
		char szBuffer[20];

		memset(szBuffer, 0x00, sizeof(szBuffer));

		memcpy(szBuffer, pszSrc, nLength);

		return atoi(szBuffer);
	}

	void n2a(int INTx, char *L_ADDR, int K, int DECI)
	{
		int     i1, i2, i3;
		int     i, j;

		if(INTx >= 0)   i2 = INTx;
		else            i2 = INTx * (-1);

		K = K - 1;

		for(i = K; i > 0; i--)
		{
			i1 = DECI;
			for(j = i; j > 1; j--)        i1 *= DECI;
			i3 = i2 / i1;
			i2 = i2 - i1 * i3;
			if(i3 >= 0x0a && DECI > 10)     i3 += 7;

			L_ADDR[K-i] = (char)(i3+0x30);
		}
		if(i2 >= 0x0a && DECI > 10)     i2 = i2 + 7;
		L_ADDR[K] = (char)(i2+0x30);

		if(INTx < 0)    L_ADDR[0] = '-';
	}
};

class CSimpleCaster : public IRunnable
{
private:
	CDSTATUS m_CdStatus;
	bool m_bConnected;
	int m_nCurState;
	bool m_bLogin;
	bool m_bDownState;
	SOCKET m_sServSocket;
	SOCKET m_sCmdSock;
	SOCKET m_sClient;
	
	char m_szServAddr[MAX_PATH];
	char m_szServPort[MAX_PATH];
	char m_atmsDir[MAX_PATH];
	
	char m_szJumCode[5];
	char m_szAtmNum[4];

	char m_szVendor[2];
	char m_szModel[3];
	char m_szKindCD[2];
	char m_szAssetID[11];
	char m_szMacAddress[13];	
	char m_szCPU[MAX_PATH];
	char m_szCPUClock[MAX_PATH];
	char m_szTMemory[6];
	char m_szFMemory[6];
	char m_szTHdd[6];
	char m_szFHdd[6];
	char m_szOSInfo[MAX_PATH];
	char iModeBuf[3];
	string m_strAgtVer;
	string m_strHostVer;
	bool m_ReqDown;
	int m_nTimeCount;
	int m_nPTimeCount;

	char m_szLocalIp[16];

	clock_t m_nLastTime, m_nStartTime;
	clock_t m_nPLastTime, m_nPStartTime;

	clock_t m_nWLastTime, m_nWStartTime;

	JnlMaster m_sJnlSend;
	JnlMaster m_sJnlRecv;
	JnlInfo m_sJnlInfo;
	
	SystemInformation m_SysInfo;
	TSharedMemory m_TxShared, m_RxShared;

	CSimpleThreadPool *m_ThreadPool;
	ServerProcessor *m_ServerProcess;
	SimpleWindowsPatcher *m_simpleWindowsPatcher;

	char m_cCurrDir;
	bool m_bStart;

	// 2011.10.20 미전송 파일 생성 skip 위해서 추가
	int m_nSendCreateionFailCount;

	// 2012.07.11 추가..
	bool m_bPowerUp;
	bool m_bIsLogIn;
	
	bool m_bShutdown;
	
protected:
	virtual void run()
	{
		int nLen = 0, i = 0;
		SYSTEMTIME st;
		int _mode = T_DATA;
		char _snd;
		int nRetry = 0;
		char szLocalPath[MAX_PATH] = {0, };
	//	char szFullPath[MAX_PATH] = {0, };
		char szTemp[30] = {0, };
		int len;
		bool bGood;
		string strVerify;

		while(!CThread::currentThread().isInterrupted())
		{
			::Sleep(100);

			if(this->m_nCurState == STATE_E)
				CDPollCheck();

			if(ServerConnect())
			{
				switch(this->m_nCurState)
				{
				case STATE_A:
					
					nLen = MkData(T_LOGIN, NULL);	// Log in AJMS
					if(nLen > 0)	this->m_nCurState++;
					
					break;

				case STATE_B:

					nLen = SendSock(nLen, (char *)&this->m_sJnlSend);
					if(nLen == OK)	this->m_nCurState++;
					else
					{
						SockReset();
						this->m_nCurState = STATE_A;
					}

					break;

				case STATE_C:
					
					nLen = RecvSock(T_LOGIN);
					if(nLen == OK)	
						this->m_nCurState++;
					else
					{
						SockReset();
						this->m_nCurState = STATE_A;
					}

					break;

				case STATE_D:

					if((strncmp(&this->m_sJnlSend.m_szMsgId[0], &this->m_sJnlRecv.m_szMsgId[0], 4) == 0) && 
						(strncmp(&this->m_sJnlRecv.m_szMsgResult[0], NM_MSG_OK, 2) == 0))
					{
						GetLocalTime(&st);

						n2a(st.wYear, &this->m_sJnlInfo.m_szLocalDate[0], 4, 10);
						n2a(st.wMonth, &this->m_sJnlInfo.m_szLocalDate[4], 2, 10);
						n2a(st.wDay, &this->m_sJnlInfo.m_szLocalDate[6], 2, 10);

						// 20090527121314123
						memcpy(&m_sJnlInfo.m_szLastJnlId[0], &this->m_sJnlRecv.m_szData[0], 17);
						if(strncmp(&m_sJnlInfo.m_szLastJnlId[0], "                 ", 17) == 0) // all space면 당년도의 1월 1월 부터 싹다 보냄...
							sprintf(&m_sJnlInfo.m_szLastJnlId[0], "%04d0101000000000", st.wYear);

						this->m_sJnlInfo.m_ImgSendMode = this->m_sJnlRecv.m_szData[17];
						if(this->m_sJnlInfo.m_ImgSendMode != 'R' && this->m_sJnlInfo.m_ImgSendMode != 'C')
							this->m_sJnlInfo.m_ImgSendMode = 'C';

						this->m_sJnlInfo.m_JnlOperation = this->m_sJnlRecv.m_szData[18];
						if(this->m_sJnlInfo.m_JnlOperation != 'E' && this->m_sJnlInfo.m_JnlOperation != 'A')
							this->m_sJnlInfo.m_ImgSendMode = 'A';

						this->m_sJnlInfo.m_nImageKeep = a2i((char *)&this->m_sJnlRecv.m_szData[19], 2);
						if(this->m_sJnlInfo.m_nImageKeep <= 0)	this->m_sJnlInfo.m_nImageKeep = 5;
						if(this->m_sJnlInfo.m_nImageKeep > 60)	this->m_sJnlInfo.m_nImageKeep = 60;

						this->m_sJnlInfo.m_nJnlKeep = a2i((char *)&this->m_sJnlRecv.m_szData[21], 2);
						if(this->m_sJnlInfo.m_nJnlKeep <= 0)	this->m_sJnlInfo.m_nJnlKeep = 5;
						if(this->m_sJnlInfo.m_nJnlKeep > 60)	this->m_sJnlInfo.m_nJnlKeep = 60;

						memset(this->m_sJnlInfo.m_szAssetID, 0x00, sizeof(this->m_sJnlInfo.m_szAssetID));
						memcpy(this->m_sJnlInfo.m_szAssetID, &this->m_sJnlRecv.m_szData[23], 10);

						bGood = true;
						strVerify = this->m_sJnlInfo.m_szAssetID;

						memset(szLocalPath, 0x00, sizeof(szLocalPath));

						GetCurrentDirectory(MAX_PATH, szLocalPath);
						
						Logger("Information : Asset ID is %s", strVerify.c_str());

						if(strVerify.compare("          ") != 0)
						{
							len = strVerify.find_last_not_of(" ");
							if(len > 0)	strVerify = strVerify.substr(0, len + 1);

							if((int)strVerify.length() > 0)
							{
								for(i = 0; i < (int)strVerify.length(); i++)
								{
									if(strVerify[i] == '.')	// fault condition
									{
										SockReset();
										Sleep(1000 * 10);
										bGood = false;
										break;
									}
								}
							}

							if (bGood)	WritePrivateProfileString("ATMINFO", "ASSET_ID", this->m_sJnlInfo.m_szAssetID, this->m_atmsDir);
						}

						if(bGood == false)	break;						

						memset(szTemp, 0x00, sizeof(szTemp));
						memcpy(szTemp, &this->m_sJnlRecv.m_szData[33], 15);	// will connected ip address 15
						
						bGood = true;
						strVerify = szTemp;
						if(strVerify.compare("               ") != 0)
						{
							len = strVerify.find_last_not_of(" ");
							if(len > 0)	strVerify = strVerify.substr(0, len + 1);	// delete right white space
							
							// 1.1.1.1	error condition~!!!
							if(strVerify.length() < 7)
							{
								Logger("Fault : Login recv data error[IP Address] - %s", strVerify.c_str());
								this->m_nCurState = STATE_A;

								Sleep(10 * 1000);

								break;
							}

							if((int)strVerify.length() > 0)
							{
								for(i = 0; i < (int)strVerify.length(); i++)
								{
									if(isdigit(strVerify[i]) || strVerify[i] == '.')
										continue;
									else	
									{
										bGood = false;
										break;
									}
								}
							
								//if(bGood)	WritePrivateProfileString("ATMS", "ATMSADDR", strVerify.c_str(), szFullPath);
								if (bGood)	WritePrivateProfileString("ATMS", "ATMSADDR", strVerify.c_str(), this->m_atmsDir);

								bGood = true;
							}
						}
						
						memset(szTemp, 0x00, sizeof(szTemp));
						memcpy(szTemp, &this->m_sJnlRecv.m_szData[48], 5);	// will connected port 5
						strVerify = szTemp;
						bGood = true;

						if(strVerify.compare("     ") != 0)
						{
							len = strVerify.find_last_not_of(" ");
							if(len > 0)
								strVerify = strVerify.substr(0, len + 1);

							if((int)strVerify.length() > 0)
							{
								for(i = 0; i < (int)strVerify.length(); i++)
								{
									if(isdigit(strVerify[i]))
										continue;
									else	
									{
										bGood = false;
										break;
									}
								}
							
								//if(bGood)	WritePrivateProfileString("ATMS", "ATMSPORT", strVerify.c_str(), szFullPath);
								if (bGood)	WritePrivateProfileString("ATMS", "ATMSPORT", strVerify.c_str(), this->m_atmsDir);

								bGood = true;
							}
						}

						// 신규 버전정보
						memset(szTemp, 0x00, sizeof(szTemp));
						memcpy(szTemp, &this->m_sJnlRecv.m_szData[53], 2);

						Logger("INFO: Server Agent version %.2s", szTemp);

						if(isdigit(szTemp[0]) && isdigit(szTemp[1]))
						{
							m_ReqDown = AgentVerCheck(atoi(szTemp));
							m_strHostVer = szTemp;
						}
						else	
							m_ReqDown = false;
					
						Logger("INFO : 로그인 성공 저널운영모드 값 수신 %c", this->m_sJnlInfo.m_JnlOperation);

						if(this->m_sJnlInfo.m_JnlOperation == 'E')
							//WritePrivateProfileString("AGENT", "MODE", "4", "c:\\atms\\atms.ini");
							WritePrivateProfileString("AGENT", "MODE", "4", this->m_atmsDir);
						else	
							//WritePrivateProfileString("AGENT", "MODE", "3", "c:\\atms\\atms.ini");
							WritePrivateProfileString("AGENT", "MODE", "3", this->m_atmsDir);

						if(this->m_sJnlInfo.m_ImgSendMode == 'R')
							//WritePrivateProfileString("AGENT", "IMGMODE", "R", "c:\\atms\\atms.ini");
							WritePrivateProfileString("AGENT", "IMGMODE", "R", this->m_atmsDir);
						else if(this->m_sJnlInfo.m_ImgSendMode == 'C')
							//WritePrivateProfileString("AGENT", "IMGMODE", "C", "c:\\atms\\atms.ini");
							WritePrivateProfileString("AGENT", "IMGMODE", "C", this->m_atmsDir);

						Logger("SUCCESS : Login Data search %.17s", &m_sJnlInfo.m_szLastJnlId[0]);


						// 데이터 삭제 , 로그인 이후 기준 일자를 넘김.. 2011.10.20 추가..
						CcmsDelete(true, this->m_cCurrDir, &m_sJnlInfo.m_szLastJnlId[0], this->m_sJnlInfo.m_nJnlKeep);
						CcmsDelete(false, this->m_cCurrDir, &m_sJnlInfo.m_szLastJnlId[0], this->m_sJnlInfo.m_nJnlKeep);

						this->m_nCurState = STATE_E;
						m_bLogin = true;

						Logger("Confirm: Next step ready... STATE_E");
					}
					else	
					{
						Logger("Error : Login recv data error - %.70s", (char *)&this->m_sJnlRecv);
						this->m_nCurState = STATE_A;

						Sleep(30 * 1000);
					}

					SetPollClock(120);

					if(m_ReqDown && m_bDownState == false) this->m_nCurState = STATE_L;
					
					Logger("Confirm: ReqDown is %d, DownState is %d, Next State is %d", m_ReqDown, m_bDownState, this->m_nCurState);

					break;
				
				case STATE_E:

					::Sleep(50);

					nLen = MkData(_mode, &_snd);
					if(nLen > 0)	this->m_nCurState++;
					else
					{
						nLen = 0;

						_mode = T_DATA;

						if(GetPollClock() < 0)	this->m_nCurState = STATE_I;
					//	else			WizardSndCheck();	// // 빈파일정보 생성...
												// 2009.12.07 체크...
					}

					break;

				case STATE_F:
					
 					nLen = SendSock(nLen, (char *)&this->m_sJnlSend);
					if(nLen == OK)	
					{
						this->m_nCurState++;
						SetPollClock(120);
					}
					else
					{
						SockReset();
						this->m_nCurState = STATE_A;
					}

					break;

				case STATE_G:

				//	Logger("DEBUG : Send complete, next recv state mode is %c", _snd);
					if(_snd == 't')	nLen = RecvSock(T_DATA);
					if(_snd == 'i')	nLen = RecvSock(T_IMGFACE);
					if(_snd == 'c')	nLen = RecvSock(T_IMGFACE);

					Logger("DEBUG : Recv complete, next recv state return value is %d", nLen);

					if(nLen == OK)	this->m_nCurState++;
					else
					{
						SockReset();
						this->m_nCurState = STATE_A;
					}

					break;

				case STATE_H:
					
					_mode = T_DATA;

					if(_snd == 't' || _snd == 'i' || _snd == 'c')
					{
						if((strncmp(&this->m_sJnlSend.m_szClientId1[0], &this->m_sJnlRecv.m_szClientId1[0], 4) == OK) &&
						   (strncmp(&this->m_sJnlSend.m_szClientId2[0], &this->m_sJnlRecv.m_szClientId2[0], 3) == OK) &&
						   (strncmp(&this->m_sJnlSend.m_szMsgId[0], &this->m_sJnlRecv.m_szMsgId[0], 4) == OK) &&
						   (strncmp(&this->m_sJnlRecv.m_szMsgResult[0], "00", 2) == OK))
						{
							if(_snd == 't')	
								_mode = T_IMGFACE;
							if(_snd == 'i')	
								_mode = T_IMGCARD;
							if(_snd == 'c') 
								_mode = T_DATA;

							memset(iModeBuf, 0x00, sizeof(iModeBuf));
							GetPrivateProfileString("AGENT", "IMGMODE", "R", iModeBuf, sizeof(iModeBuf), this->m_atmsDir);

							this->m_sJnlInfo.m_ImgSendMode = iModeBuf[0];
							
							if(this->m_sJnlInfo.m_ImgSendMode == 'C')	_mode = T_DATA;

							// submit this image data
							this->m_nCurState = STATE_E;

							nRetry = 0;
						}
						else
						{
							Logger("Error: Jnl send, response data error - %.2s", (char *)&this->m_sJnlRecv.m_szMsgResult[0]);
							
							this->m_nCurState = STATE_A;

							SockReset();

							Sleep(15 * 1000);
						}
					}
					else	// Oops!!!, 
					{
						::Sleep(1000 * 10);

						SockReset();
						this->m_nCurState = STATE_A;
						nRetry = 0;
					}

					break;

				case STATE_I:	// poll 처리...
					
					nLen = MkData(T_POLL, NULL);
					if(nLen > 0)	this->m_nCurState++;
					else		
					{
						SockReset();
						this->m_nCurState = STATE_A;
						nRetry = 0;
					}

					break;

				case STATE_J:

					nLen = SendSock(nLen, (char *)&this->m_sJnlSend);
					if(nLen == OK)	this->m_nCurState++;
					else
					{
						SockReset();
						this->m_nCurState = STATE_A;
					}

				case STATE_K:

					nLen = RecvSock(T_POLL);
					if(nLen != OK)
					{
						SockReset();
						this->m_nCurState = STATE_A;
					}
					else
					{
						SetPollClock(120);
						this->m_nCurState = STATE_E;
					}

					break;

				case STATE_L:		// 에이젼트 다운로드 처리... 

					m_bDownState = true;

					Logger("Confirm: 다운로드 스테이트, 요청 전문 전송 준비");
					
					nLen = MkData(T_DREQ, NULL);	// 다운로드 요청 전문 생성...
					if(nLen > 0)
					{
						nLen = SendSock(nLen, (char *)&this->m_sJnlSend);
						if(nLen == OK)
						{
							nLen = RecvSock(T_DREQ);
							if(nLen != OK)
							{
								SockReset();
								this->m_nCurState = STATE_A;

								break;
							}
							else
							{
								if((strncmp(&this->m_sJnlSend.m_szMsgId[0], &this->m_sJnlRecv.m_szMsgId[0], 4) == 0) && 
										(strncmp(&this->m_sJnlRecv.m_szMsgResult[0], NM_MSG_OK, 2) == 0))		
								{
									bGood = MediaDownLoad();
									if(bGood)	
									{
										WritePrivateProfileString("ccms", "ccmsver", m_strHostVer.c_str(), this->m_atmsDir);
										m_strAgtVer = m_strHostVer;
									}
									else	
										Logger("Failed: Write version information %s --> %s", this->m_atmsDir, m_strHostVer.c_str());
								}
								else	
								{
									this->m_nCurState = STATE_E;
									SetPollClock(120);
									break;
								}
							}
						}
						else
						{
							SockReset();
							this->m_nCurState = STATE_A;	// error condition...! 나중에 코드 추가...

							break;
						}
					}

					this->m_nCurState = STATE_E;
					SetPollClock(120);

					break;

				default:	// Oops!!!

					SockReset();
					this->m_nCurState = STATE_A;
					nRetry = 0;
				
					break;
				}
			}
		}
	}

public:
	CSimpleCaster(char drive) : m_SysInfo(), m_CdStatus(drive)
	{ 	
		InitializeCriticalSection(&GCS);

		m_nSendCreateionFailCount = 0;
		
		m_bShutdown = false;
		m_bDownState = false;
		m_ReqDown = false;
		m_bStart = true;
		m_CdStatus.Clear();
		m_bLogin = false;
		m_cCurrDir = drive;

		m_bConnected = false;
		m_nCurState = STATE_A;
		m_sServSocket = INVALID_SOCKET;
		m_nTimeCount = 0;
		m_nLastTime = 0; 
		m_nStartTime = 0;

		memset(&this->m_sJnlSend, 0x0, sizeof(JnlMaster));
		memset(&this->m_sJnlRecv, 0x0, sizeof(JnlMaster));
		memset(&this->m_sJnlInfo, 0x00, sizeof(JnlInfo));

		memset(this->m_szServAddr, 0x00, sizeof(m_szServAddr));
		memset(this->m_szServPort, 0x00, sizeof(m_szServPort));

		memset(this->m_szJumCode, 0x00, sizeof(m_szJumCode));
		memset(this->m_szAtmNum, 0x00, sizeof(m_szAtmNum));
		memset(this->m_szLocalIp, 0x00, sizeof(m_szLocalIp));
		memset(this->m_szVendor, 0x00, sizeof(m_szVendor));
		memset(this->m_szModel, 0x00, sizeof(m_szModel));
		memset(this->m_szKindCD, 0x00, sizeof(m_szKindCD));
		memset(this->m_szAssetID, 0x00, sizeof(m_szAssetID));
		memset(this->m_szMacAddress, 0x00, sizeof(m_szMacAddress));	
		memset(this->m_szCPU, 0x00, sizeof(m_szCPU));
		memset(this->m_szCPUClock, 0x00, sizeof(m_szCPUClock));
		memset(this->m_szTMemory, 0x00, sizeof(m_szTMemory));
		memset(this->m_szFMemory, 0x00, sizeof(m_szFMemory));
		memset(this->m_szTHdd, 0x00, sizeof(m_szTHdd));
		memset(this->m_szFHdd, 0x00, sizeof(m_szFHdd));
		memset(this->m_szOSInfo, 0x00, sizeof(m_szOSInfo));

		GetConfiguration();

		Logger("\r\n\r\nInfo: Start Agent Svc version 1.6.1, 2018-06-17 ----------------------------------");

		sprintf(this->m_szFHdd, "%-5.1f", m_SysInfo.GetHddFreeSpace() / 1024);
		sprintf(this->m_szTHdd, "%-5.1f", m_SysInfo.GetHddTotalSpace() / 1024);

		ltoa((long)m_SysInfo.GetMemStatus(), this->m_szFMemory, 10);
		this->m_SysInfo.GetProcessorInformation(this->m_szCPU);
		this->m_SysInfo.GetProcessorClock(this->m_szCPUClock);
		this->m_SysInfo.GetOsPatchInformation(this->m_szOSInfo);
		this->m_SysInfo.GetMacAddress(this->m_szMacAddress);
		this->m_SysInfo.GetLocalIPAddress(this->m_szLocalIp);

		sprintf(this->m_atmsDir, "%c:\\atms\\atms.ini", this->m_cCurrDir);

		this->m_ThreadPool = new CSimpleThreadPool(3);
		this->m_ThreadPool->startAll();

		this->m_ServerProcess = new ServerProcessor(20015, this->m_cCurrDir);

		Logger("Info: 쓰레드 풀 스타트 passing drive %c", this->m_cCurrDir);

		this->m_simpleWindowsPatcher = new SimpleWindowsPatcher(this->m_cCurrDir);

		this->m_ThreadPool->submit(m_ServerProcess);
		this->m_ThreadPool->submit(m_simpleWindowsPatcher);
	}

	bool InitCommander()
	{
		u_long iMode = 1;
		struct sockaddr_in mServerAddr;
		
		memset((char *)&mServerAddr, 0x00, sizeof(mServerAddr));
		
		mServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		mServerAddr.sin_family = AF_INET;
		mServerAddr.sin_port = htons((u_short)20019);

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

		if(ioctlsocket(m_sCmdSock, FIONBIO, &iMode) != 0)	
			return false;

		return true;
	}

	bool Accept()
	{
		bool ret = false;
		
		int nLen = 0;
		struct sockaddr_in mClientAddr;

		SetClock(60);

		memset(&mClientAddr, 0x00, sizeof(struct sockaddr_in));
		nLen = sizeof(mClientAddr);

		while(1)
		{
			Sleep(100);

			if(GetClock() <= 0)	return false;	

			m_sClient = accept(m_sCmdSock, (struct sockaddr *)&mClientAddr, &nLen);
			if(m_sClient > 0 && m_sClient != INVALID_SOCKET)
			{
				ret = true;
				break;
			}
		}

		return ret;
	}

	void CloseCommander()
	{
		closesocket(this->m_sClient);
		closesocket(this->m_sCmdSock);

		m_sCmdSock = INVALID_SOCKET;
		m_sClient = INVALID_SOCKET;
	}

	int ReadFromServer(int mode, SOCKET sock, char *buff) 
	{
		int total, req, left, len;

		len = total = req = left = 0;
		
		if(mode == T_DINFO)	req = 90;
		
		SetClock(60);

		while(total < req)
		{
			Sleep(100);

			if(GetClock() <= 0)	return SOCKET_ERROR;
			
			len = recv(sock, (char *)&buff[left], req - left, 0);
			if(len > 0)
			{
				left += len;
				total += len;
			}
			else	continue;
		}

		return total;
	}

	int ReadFromServer(SOCKET sock, int buf_len, char *buff, string file_name, int filesize)
	{
		FILE *fp = NULL;
		string real_name;
		string comp_name;

		int idx = file_name.find_last_of("\\");
		if(idx > 0)	
		{
			real_name = file_name.substr(0, idx);
			real_name += "\\";
			real_name += "setup.exe";
		}

		int len, total, req, ret;

		len = total = req = ret = 0;
		
		req = filesize;
		if(req <= 0)	return SOCKET_ERROR;

		memset(buff, 0x00, buf_len);


		if((fp = fopen(real_name.c_str(), "w+b")) == NULL)
			return SOCKET_ERROR;

		SetClock(60);

		while(total < req)
		{
			Sleep(10);

			if(GetClock() <= 0)	
			{
				if(fp)	fclose(fp);
				return false;
			}

			ret = recv(sock, (char *)buff, 4096, 0);
			if(ret == SOCKET_ERROR || ret <= 0)	continue;
			
			if(fp)
			{
				fwrite(buff, ret, 1, fp);
				memset(buff, buf_len, sizeof(buff));
			}

			total += ret;

			SetClock(5);
		}

		if(fp)	fclose(fp);	// 다운로드 파일 save ok

		Logger("읽기 완료 크기 %d, 다운로드 파일 크기 %d", total, filesize);

		if(total == req)	// ok... 다운로드...
		{
			Sleep(1000);

			// 다운로드 에이젼트에 다운로드 알림
			comp_name = "c:\\pkgapdn\\dwn.ok";
			if((fp = fopen(comp_name.c_str(), "w+b")) == NULL)
			{
				DeleteFile(real_name.c_str());
				return SOCKET_ERROR;
			}

			if(fp)	fclose(fp);

			Logger("매체 파일 다운로드 완료!!!");

			len = MkData(T_DCOMP, NULL);
			len = SendToServer(len, sock, (char *)&this->m_sJnlSend);
			if(len > 0)	Logger("Success : 다운로드 완료 통보");
			else		Logger("Failded : 다운로드 완료 통보");

			return total;
		}
		else
		{
			DeleteFile(real_name.c_str());
			return SOCKET_ERROR;
		}

		return total;
	}

	int SendToServer(int len, SOCKET sock, char *buff)
	{
		int total, req, left;

		total = req = left = 0;
		
		SetClock(10);

		while(total < len)
		{
			Sleep(100);
			
			if(GetClock() <= 0)	return SOCKET_ERROR;

			req = send(sock, &buff[left], len - left, 0);
			if(req > 0)
			{
				left += req;
				total += req;
			}
			else	continue;
		}

		return total;
	}

	bool MediaDownLoad()
	{
		bool ret = false;
		int len;
		unsigned char buff[4096] = {0, };

		if(InitCommander() != true)
		{
			CloseCommander();

			return false;
		}

		if(Accept() != true)
		{
			CloseCommander();

			return false;
		}

		len = ReadFromServer(T_DINFO, m_sClient, (char *)buff);
		if(len < 0)
		{
			CloseCommander();

			return false;
		}
		else 
		{
			if(strncmp((char *)&buff[21], "00", 2) != 0)
			{
				CloseCommander();

				return false;
			}
			else 
			{
				memcpy(&buff[0], "0000000040", 10);

				len = SendToServer(50, m_sClient, (char *)buff);
				if(len < 0)	
				{
					CloseCommander();

					return false;
				}
			}
		}

		string name_file;
		string name_path;
		int filesize = 0;
		char tmp0[50] = {0, };

		memcpy(tmp0, &buff[50], 30);
		name_file = tmp0;
		len = name_file.find_last_not_of(" ");
		if(len > 0)	name_file = name_file.substr(0, len + 1);

		memset(tmp0, 0x00, sizeof(tmp0));
		memcpy(tmp0, &buff[80], 10);
		filesize = atoi(tmp0);
		
		name_path = "c:\\pkgapdn";
		CreateDirectory(name_path.c_str(), NULL);

		name_path += "\\";
		name_path += name_file;

		len = ReadFromServer(m_sClient, sizeof(buff), (char *)buff, name_path, filesize);

		memset(buff, 0x00, sizeof(buff));
		memset(buff, 0x30, 50);
		memcpy(buff, "0000000040WZ93", 14);

		CloseCommander();

		ret = (len > 0) ? true : false;

		return ret;
	}
	
	bool AgentVerCheck(int host_ver)
	{
		bool ret = false;

		int localVer = atoi(this->m_strAgtVer.c_str());

	//	2011.10.20
	//	if(localVer > host_ver)	ret = true;
		if(host_ver > localVer)	ret = true;

		Logger("Confirm: Current Local Version %d, Host Version %d, Download phase is %d", localVer, host_ver, ret);

		return ret;
	}

	~CSimpleCaster()
	{
		closesocket(m_sServSocket);
	}
	
	void SetImageTransterMode(UCHAR Mode)
	{
		this->m_sJnlInfo.m_ImgSendMode = (char)Mode;		
	}
	
	int MakeSharedMem(int sMode)
	{	
		// 3	2000	2100	저널운영 모드 변경
		memset(&this->m_TxShared, 0x00, sizeof(TSharedMemory));

		// 3	0200 0001 저널운영 모드 변경
		memcpy(this->m_TxShared.LCODE, "2000", 4);

		if(sMode == 0)
		{	
			memcpy(this->m_TxShared.SCODE, "2100", 4);
			memcpy(this->m_TxShared.LEN, "00000001", 1);
			memcpy(this->m_TxShared.DATA, &this->m_sJnlInfo.m_JnlOperation, 1);
		}

		Logger("INFO : 저널운영 모드 데이터 셋팅, Data %d", this->m_sJnlInfo.m_JnlOperation);

		return 0;
	}
	
	void n2a(int INTx, char *L_ADDR, int K, int DECI)
	{
		int     i1, i2, i3;
		int     i, j;

		if(INTx >= 0)   i2 = INTx;
		else            i2 = INTx * (-1);

		K = K - 1;

		for(i = K; i > 0; i--)
		{
			i1 = DECI;
			for(j = i; j > 1; j--)        i1 *= DECI;
			i3 = i2 / i1;
			i2 = i2 - i1 * i3;
			if(i3 >= 0x0a && DECI > 10)     i3 += 7;

			L_ADDR[K-i] = (char)(i3+0x30);
		}
		if(i2 >= 0x0a && DECI > 10)     i2 = i2 + 7;
		L_ADDR[K] = (char)(i2+0x30);

		if(INTx < 0)    L_ADDR[0] = '-';
	}

	void GetConfiguration()
	{
		char szPath[MAX_PATH] = {0, };
		//char szTemp[MAX_PATH] = {0, };
		char szTmp0[3] = {0, };
		char szFileName[MAX_PATH] = {0, };

#ifndef __SIMPLE_DEBUG__ /////////////////////////////////////////////////////////////////////////////////////////////////
		GetCurrentDirectory(MAX_PATH, szPath);
		
	//	memcpy(&szTemp[0], &szPath[0], 3);
	//	strcpy(&szTemp[3], "atms\\atms.ini");
		
	//	GetModuleFileName(NULL, szFileName, sizeof(szFileName));

	//	m_cCurrDir = szFileName[0];

	//	szTemp[0] = m_cCurrDir;
		
	//	WritePrivateProfileStringA("ATMS", "ATMSVER", "15", szTemp);
		WritePrivateProfileStringA("ATMS", "ATMSVER", "15", this->m_atmsDir);

		/*GetPrivateProfileStringA("ATMS", "ATMSADDR", "0.0.0.0", &m_szServAddr[0], sizeof(m_szServAddr), szTemp);
		GetPrivateProfileStringA("ATMS", "ATMSPORT", "00000", &m_szServPort[0], sizeof(m_szServPort), szTemp);
		GetPrivateProfileStringA("ATMS", "ATMSVER", "13", szTmp0, sizeof(szTmp0), szTemp);*/

		GetPrivateProfileStringA("ATMS", "ATMSADDR", "0.0.0.0", &m_szServAddr[0], sizeof(m_szServAddr), this->m_atmsDir);
		GetPrivateProfileStringA("ATMS", "ATMSPORT", "00000", &m_szServPort[0], sizeof(m_szServPort), this->m_atmsDir);
		GetPrivateProfileStringA("ATMS", "ATMSVER", "13", szTmp0, sizeof(szTmp0), this->m_atmsDir);

		// current agent version,...
		this->m_strAgtVer = szTmp0;
		/*
		GetPrivateProfileStringA("ATMINFO", "JUMCODE", "    ", &m_szJumCode[0], sizeof(m_szJumCode), szTemp);
		GetPrivateProfileStringA("ATMINFO", "ATMNUM", "   ", &m_szAtmNum[0], sizeof(m_szAtmNum), szTemp);
		GetPrivateProfileStringA("ATMINFO", "VENDOR", " ", &m_szVendor[0], sizeof(m_szVendor), szTemp);
		GetPrivateProfileStringA("ATMINFO", "MODEL", "  ", &m_szModel[0], sizeof(m_szModel), szTemp);
		GetPrivateProfileStringA("ATMINFO", "KIND_CD", " ", &m_szKindCD[0], sizeof(m_szKindCD), szTemp);
		GetPrivateProfileStringA("ATMINFO", "ASSET_ID", "01234567890", &m_szAssetID[0], sizeof(m_szAssetID), szTemp);*/

		GetPrivateProfileStringA("ATMINFO", "JUMCODE", "    ", &m_szJumCode[0], sizeof(m_szJumCode), this->m_atmsDir);
		GetPrivateProfileStringA("ATMINFO", "ATMNUM", "   ", &m_szAtmNum[0], sizeof(m_szAtmNum), this->m_atmsDir);
		GetPrivateProfileStringA("ATMINFO", "VENDOR", " ", &m_szVendor[0], sizeof(m_szVendor), this->m_atmsDir);
		GetPrivateProfileStringA("ATMINFO", "MODEL", "  ", &m_szModel[0], sizeof(m_szModel), this->m_atmsDir);
		GetPrivateProfileStringA("ATMINFO", "KIND_CD", " ", &m_szKindCD[0], sizeof(m_szKindCD), this->m_atmsDir);
		GetPrivateProfileStringA("ATMINFO", "ASSET_ID", "01234567890", &m_szAssetID[0], sizeof(m_szAssetID), this->m_atmsDir);
		
		// 테스트 코드
	//	memcpy(m_szJumCode, "0999", 4);
	//	memcpy(m_szAtmNum, "501", 3);

		if(strncmp(m_szJumCode, "    ", 4) == 0 || strncmp(m_szAtmNum, "   ", 3) == 0 ||
		   strncmp(m_szVendor, " ", 1) == 0 || strncmp(m_szModel, "  ", 2) == 0 ||
		   strncmp(m_szKindCD, " ", 1) == 0)
			memset(m_szServAddr, 0x00, sizeof(m_szServAddr));

		/*if(strncmp(&m_szJumCode[0], "0860", 4) != 0)
		{
			if(strncmp(&m_szServAddr[0], "23.20.203.154", 13) != 0 ||
			   strncmp(&m_szServPort[0], "20012", 5) != 0)
			{
				memset(&m_szServAddr[0], 0x00, sizeof(m_szServAddr));
				memset(&m_szServPort[0], 0x00, sizeof(m_szServPort));

				GetModuleFileName(NULL, szFileName, sizeof(szFileName));
				strcpy(&szFileName[1], ":\\atms\\atms.ini");

				WritePrivateProfileString("ccms", "ccmsaddr", "23.20.203.154", szFileName);
				WritePrivateProfileString("ccms", "ccmsport", "20012", szFileName);

				strcpy(m_szServAddr, "23.20.203.154");
				strcpy(m_szServPort, "20012");
			}
		}*/
#else	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		GetCurrentDirectory(MAX_PATH, szPath);
		
		memcpy(&szTemp[0], &szPath[0], 3);
		strcpy(&szTemp[3], "atms\\atms.ini");

		GetModuleFileName(NULL, szFileName, sizeof(szFileName));
		
		m_cCurrDir = szFileName[0];

		szTemp[0] = m_cCurrDir;
//		1
		GetPrivateProfileStringA("ATMS", "ATMSADDR", "23.25.15.189", &m_szServAddr[0], sizeof(m_szServAddr), szTemp);
		GetPrivateProfileStringA("ATMS", "ATMSPORT", "20012", &m_szServPort[0], sizeof(m_szServPort), szTemp);
		GetPrivateProfileStringA("ATMS", "ATMSVER", "12", szTmp0, sizeof(szTmp0), szTemp);

		// current agent version,...
		this->m_strAgtVer = szTmp0;

		GetPrivateProfileStringA("ATMINFO", "JUMCODE", "0860", &m_szJumCode[0], sizeof(m_szJumCode), szTemp);
		GetPrivateProfileStringA("ATMINFO", "ATMNUM", "055", &m_szAtmNum[0], sizeof(m_szAtmNum), szTemp);
		GetPrivateProfileStringA("ATMINFO", "VENDOR", "1", &m_szVendor[0], sizeof(m_szVendor), szTemp);
		GetPrivateProfileStringA("ATMINFO", "MODEL", "17", &m_szModel[0], sizeof(m_szModel), szTemp);
		GetPrivateProfileStringA("ATMINFO", "KIND_CD", "A", &m_szKindCD[0], sizeof(m_szKindCD), szTemp);
		GetPrivateProfileStringA("ATMINFO", "ASSET_ID", "          ", &m_szAssetID[0], sizeof(m_szAssetID), szTemp);

		if(strncmp(m_szJumCode, "    ", 4) == 0 || strncmp(m_szAtmNum, "   ", 3) == 0 ||
		   strncmp(m_szVendor, " ", 1) == 0 || strncmp(m_szModel, "  ", 2) == 0 ||
		   strncmp(m_szKindCD, " ", 1) == 0)
			memset(m_szServAddr, 0x00, sizeof(m_szServAddr));

		if(strncmp(&m_szJumCode[0], "0860", 4) != 0)
		{
			if(strncmp(&m_szServAddr[0], "23.20.203.154", 13) != 0 ||
			   strncmp(&m_szServPort[0], "20012", 5) != 0)
			{
				memset(&m_szServAddr[0], 0x00, sizeof(m_szServAddr));
				memset(&m_szServPort[0], 0x00, sizeof(m_szServPort));

				GetModuleFileName(NULL, szFileName, sizeof(szFileName));
				strcpy(&szFileName[1], ":\\atms\\atms.ini");

				WritePrivateProfileString("ccms", "ccmsaddr", "23.20.203.154", szFileName);
				WritePrivateProfileString("ccms", "ccmsport", "20012", szFileName);

				strcpy(m_szServAddr, "23.20.203.154");
				strcpy(m_szServPort, "20012");
			}
		}
#endif	////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	}
	
	int ZDifftime(SYSTEMTIME start, SYSTEMTIME finish)
	{
		int d = 0 , temp, up = 0;

		temp = finish.wSecond  - start.wSecond;
		if (temp < 0)
		{
			temp += 60;
			up = -1;
		}
		else	up = 0;

		d += temp * 100;

		temp = finish.wMinute  - start.wMinute + up;
		if(temp < 0)
		{
			temp += 60;
			up = -1;
		}
		else	up = 0;

		d += temp * 6000;

		temp = finish.wHour - start.wHour + up;
		if(temp < 0)
		{
			temp += 60;
			up = -1;
		}
		else	up = 0;

		return (d * 10);
	}

	void WizardSndMark(char *pPath, char *pFileName)
	{
		char szMaxPath[MAX_PATH] = {0, };
		FILE *fp;
		
		MakeDir(pPath);
		
		strcpy(szMaxPath, pPath);
		strcat(szMaxPath, "\\");
		strcat(szMaxPath, pFileName);

		fp = fopen(szMaxPath, "w+b");
		if(fp)	
		{
			Logger("Success : %s 파일 전송 기록 성공...next step", szMaxPath);
			fclose(fp);
		}
		else	
		{
			Logger("Error : %s 파일 전송 기록 실패...next step", szMaxPath);

			Sleep(2);
			
			fp = fopen(szMaxPath, "w+b");
			if(fp)	
			{
				Logger("Success : %s 파일 전송 기록 성공...next step", szMaxPath);

				fclose(fp);
			}
			else
			{
				m_nSendCreateionFailCount++;

				Logger("Error : %s 파일 전송 기록 실패...next step", szMaxPath);
			}
		}
	}

	int CDPollCheck()
	{
		static int iMod = 0;
		char strData[3], strCurTime[30], strGetTime[30], strOldWritedTime[30];
		SYSTEMTIME stCurTime, stAtmTime;
		int nCurMode, nDiffTime;
		int nAnchor, nRet;
		int iRetry = 0;

		if(iMod < 3)
		{
			iMod++;
			return 0;
		}
		else	iMod = 0;
		
		GetLocalTime(&stCurTime);
		sprintf(strCurTime, "%04d-%02d-%02d %02d:%02d:%02d", 
				stCurTime.wYear, stCurTime.wMonth, stCurTime.wDay, stCurTime.wHour, stCurTime.wMinute, stCurTime.wSecond);

		GetPrivateProfileString("ATM", "CURSTAT", "1", strData, sizeof(strData), this->m_atmsDir);
		GetPrivateProfileString("ATM", "POLLTIME", strCurTime, strGetTime, sizeof(strGetTime), this->m_atmsDir);

		nCurMode = strData[0] - 0x30;

		switch(nCurMode)
		{
		case WZ_IDLE:
			nAnchor = 60 * 3;	// 3
			break;

		case WZ_SERVICE:
			nAnchor = 60 * 1;	// 1분 체크
			break;

		case WZ_POWERUP:
			nAnchor = 60 * 20;	// 20 분체크
			break;

		case WZ_OFFLINE:
			nAnchor = 60 * 3;	// 3 분
			break;

		case WZ_MFAULT:
			nAnchor = 60 * 60;	// 1 시간
			break;

		case WZ_MANUAL:
			nAnchor = 60 * 60;	// 1 시간
			break;

		case WZ_ETCMODE:
		default:
			nAnchor = 60 * 30;	// 30분
			break;
		}

		// 시간Gap 때문에 반드시 다시 시간을 얻어옴(99999장애를 유발할 수 있음)
		GetLocalTime(&stCurTime);
		sprintf(strCurTime, "%04d-%02d-%02d %02d:%02d:%02d", 
				stCurTime.wYear, stCurTime.wMonth, stCurTime.wDay, stCurTime.wHour, stCurTime.wMinute, stCurTime.wSecond);
	
		//	0	  1
	//	0123456789012345678
	//	2009-11-11 21:22:22
		stAtmTime.wHour = a2i(&strGetTime[11], 2);
		stAtmTime.wMinute = a2i(&strGetTime[14], 2);
		stAtmTime.wSecond = a2i(&strGetTime[17], 2);
		
		nDiffTime = ZDifftime(stAtmTime, stCurTime);
		nDiffTime = nDiffTime / 1000;

		iRetry = 0;
		
		// 1000밀리 세컨드 단위로 리턴됨
		if(nDiffTime > nAnchor)
		{
			while(iRetry < 3)
			{
				if(!m_CdStatus.IsSend())	// LL 99999장애를 보내지 않았음!!!
				{
					Logger("HAPPEN : ATM --> AGENT overtime occured, Make trouble 99999 data. CD/ATM cur mode %d", nCurMode);

					nRet = m_CdStatus.MakeTroubleMessage(this->m_szJumCode, 
										this->m_szAtmNum, 
										this->m_szLocalIp, 
										this->m_szVendor, 
										this->m_szKindCD, 
										0);

					nRet = this->SendSock(nRet, m_CdStatus.GetMessage());
					if(nRet == OK)
					{
						nRet = this->RecvSock(T_TROUBLE);
						if(nRet == OK)
						{
							m_CdStatus.SetSendStat(strGetTime);
							Logger("SUCCESS : Agent --> Server LL 99999장애 전송 성공");
							
							break;
						}
						else	
						{
							Logger("ERROR : Agent --> Server LL 99999장애 전송 실패, Retry Send again");
							Sleep(3000);

							SockReset();

							ServerConnect();
						}
					}
					else	
						return 0;
				}
				else 
					break;

				Sleep(10);
				
				iRetry++;
			}
		}

		if(m_CdStatus.IsSend())
		{
			memset(strData, 0x00, sizeof(strData));
			GetPrivateProfileString("ATM", "CURSTAT", "1", strData, sizeof(strData), this->m_atmsDir);
			nCurMode = strData[0] - 0x30;
			
			m_CdStatus.SetClearTime(strCurTime);

			if(nCurMode == WZ_IDLE || nCurMode == WZ_SERVICE)
			{
				GetLocalTime(&stCurTime);
				sprintf(strCurTime, "%04d-%02d-%02d %02d:%02d:%02d", 
							stCurTime.wYear, stCurTime.wMonth, 
							stCurTime.wDay, stCurTime.wHour, 
							stCurTime.wMinute, stCurTime.wSecond);

				GetPrivateProfileString("ATM", "POLLTIME", strCurTime, strGetTime, sizeof(strGetTime), this->m_atmsDir);
				
				memset(strData, 0x00, sizeof(strData));
				memset(strOldWritedTime, 0x00, sizeof(strData));

				char ttmp[50] = { 0 };

				sprintf(ttmp, "%c:\\atms\\ll_trb.ini", this->m_atmsDir[0]);
				GetPrivateProfileString("ATMS", "CDWRITEDTIME", strCurTime, strOldWritedTime, sizeof(strOldWritedTime), ttmp);

				if(strncmp(&strGetTime[0], &strOldWritedTime[0], strlen(strGetTime)) != 0)
				{
					Logger("RECOVER : AGENT --> ALIVE occured");
					Logger("INFO : MAKE TROUBLE 99999 Release Send TRANSFER DATA");
						
					nRet = m_CdStatus.MakeTroubleMessage(this->m_szJumCode, this->m_szAtmNum, this->m_szLocalIp, this->m_szVendor, this->m_szKindCD, 1);
					nRet = this->SendSock(nRet, m_CdStatus.GetMessage());
					if(nRet == OK)
					{
						nRet = this->RecvSock(T_TROUBLE);
						if(nRet == OK && strncmp(&this->m_sJnlRecv.m_szMsgResult[0], NM_MSG_OK, 2) == 0)
						{
							m_CdStatus.ReSetSendStat();
							Logger("SUCCESS : Agent --> Server LL 해제 99999장애 전송 성공");
						}
						else	
						{
							Logger("ERROR : Agent --> Server LL 해제 99999장애 전송 실패");
							SockReset();
						}
					}
					else	return 0;
				}	
				else	return 0;
			}
		}

		return 0;
	}

	int MkData(int nMode, char *snd)
	{
		int nLen = 0;
		int i = 0, stat = 0;
		char ipath[MAX_PATH] = {0};
		unsigned int u_ret = 0;
		char szDataLength[20] = {0, };
		char szDataLen[20] = {0, };
		static int nMod = 0;
		SYSTEMTIME st;
		char szTempDateTime[30] = {0, };
		static int nIterator = 0;
		int nRet = 0;
		MYIMAGEINFO info;
		
		if(nIterator == 0)
		{
			GetConfiguration();

			nIterator++;

			if(nIterator == 50)	nIterator = 0;
		}

		if(nMode == T_POLL)
		{
			GetLocalTime(&st);
		
			if(st.wHour >= 7 && st.wHour < 8 && m_bLogin == false)
			{	
				SockReset();
				SetPollClock(120);
				
				return 0;
			}

			// 전원 미 OFF 대비 
			if(nMod >= 15)
			{
				GetLocalTime(&st);
				
				n2a(st.wYear,  &szTempDateTime[0], 4, 10);
				n2a(st.wMonth, &szTempDateTime[4], 2, 10);
				n2a(st.wDay,   &szTempDateTime[6], 2, 10);
				
				if(strncmp(szTempDateTime, m_sJnlInfo.m_szLocalDate, 8) != 0)	
				{
					memcpy(m_sJnlInfo.m_szLocalDate, szTempDateTime, 8);
					Logger("Info: No power shutdown, occured, anchor date re settings");
				}

				nMod = 0;
			}

			nMod++;
			
			memset(&this->m_sJnlSend, 0x20, sizeof(JnlMaster));
			
			memcpy(&this->m_sJnlSend.m_szMsgId[0], "WZ04", 4);
			memcpy(&this->m_sJnlSend.m_szMsgResult[0], "00", 2);
			memcpy(&this->m_sJnlSend.m_szClientIp[0], &this->m_szLocalIp[0], strlen(this->m_szLocalIp));
			memcpy(&this->m_sJnlSend.m_szClientId1[0], this->m_szJumCode, 4);
			memcpy(&this->m_sJnlSend.m_szClientId2[0], this->m_szAtmNum, 3);
			memset(&this->m_sJnlSend.m_szReserved[0], 0x20, sizeof(this->m_sJnlSend.m_szReserved));
			
			sprintf(szDataLength, "%010d", 40);

			memcpy(&this->m_sJnlSend.m_szDataLen[0], szDataLength, 10);

			return 50;
		}
		
		if(nMode == T_DREQ || nMode == T_DCOMP) 
		{
			memset(&this->m_sJnlSend, 0x20, sizeof(JnlMaster));
			
			if(nMode == T_DREQ)
				memcpy(&this->m_sJnlSend.m_szMsgId[0], "WZ91", 4);
			else	
				memcpy(&this->m_sJnlSend.m_szMsgId[0], "WZ93", 4);

			memcpy(&this->m_sJnlSend.m_szMsgResult[0], "00", 2);
			memcpy(&this->m_sJnlSend.m_szClientIp[0], &this->m_szLocalIp[0], strlen(this->m_szLocalIp));
			memcpy(&this->m_sJnlSend.m_szClientId1[0], this->m_szJumCode, 4);
			memcpy(&this->m_sJnlSend.m_szClientId2[0], this->m_szAtmNum, 3);
			memset(&this->m_sJnlSend.m_szReserved[0], 0x20, sizeof(this->m_sJnlSend.m_szReserved));
			memcpy(&this->m_sJnlSend.m_szData[0], this->m_strAgtVer.c_str(), 2);

			nLen = 50;

			if(nMode == T_DREQ)
			{
				sprintf(szDataLength, "%010d", 42);
				nLen = 52;
			}
			else	sprintf(szDataLength, "%010d", 40);
		
			memcpy(&this->m_sJnlSend.m_szDataLen[0], szDataLength, 10);
			
			return nLen;
		}
		

		if(nMode == T_LOGIN)
		{
			memset(&this->m_sJnlSend, 0x20, sizeof(JnlMaster));
			
			memcpy(&this->m_sJnlSend.m_szMsgId[0], "WZ01", 4);
			memcpy(&this->m_sJnlSend.m_szMsgResult[0], "00", 2);
			memcpy(&this->m_sJnlSend.m_szClientIp[0], &this->m_szLocalIp[0], strlen(this->m_szLocalIp));
			memcpy(&this->m_sJnlSend.m_szClientId1[0], this->m_szJumCode, 4);
			memcpy(&this->m_sJnlSend.m_szClientId2[0], this->m_szAtmNum, 3);
			memset(&this->m_sJnlSend.m_szReserved[0], 0x20, sizeof(this->m_sJnlSend.m_szReserved));

			memcpy(&this->m_sJnlSend.m_szData[  0], this->m_szCPU, min(strlen(this->m_szCPU), 50));
			memcpy(&this->m_sJnlSend.m_szData[ 50], this->m_szCPUClock, min(strlen(this->m_szCPUClock), 60));
			memcpy(&this->m_sJnlSend.m_szData[110], this->m_szFMemory, min(strlen(this->m_szFMemory), 5));
			memcpy(&this->m_sJnlSend.m_szData[115], this->m_szTHdd, min(strlen(this->m_szTHdd), 5));
			memcpy(&this->m_sJnlSend.m_szData[120], this->m_szFHdd, min(strlen(this->m_szFHdd), 5));
			memcpy(&this->m_sJnlSend.m_szData[125], this->m_szOSInfo, min(strlen(this->m_szOSInfo), 100));
			memcpy(&this->m_sJnlSend.m_szData[225], this->m_szVendor, 1);
			memcpy(&this->m_sJnlSend.m_szData[226], this->m_szModel, 2);
			memcpy(&this->m_sJnlSend.m_szData[228], this->m_szKindCD, 1);
			memcpy(&this->m_sJnlSend.m_szData[229], this->m_szMacAddress, 12);
			memcpy(&this->m_sJnlSend.m_szData[241], this->m_szAssetID, 10);

			// 2009.11.28 agent version 정보 추가
			memcpy(&this->m_sJnlSend.m_szData[251], m_strAgtVer.c_str(), 2);
			memset(&this->m_sJnlSend.m_szData[253], 0x20, 199);
			
			sprintf(szDataLength, "%010d", 490);

			memcpy(&this->m_sJnlSend.m_szDataLen[0], szDataLength, 10);

			return 500;
		}

		if(nMode == T_DATA)
		{
			memset(&this->m_sJnlSend, 0x20, sizeof(JnlMaster));
			
			// 전원 미 OFF 대비 
			if(nMod >= 15)
			{
				GetLocalTime(&st);
				
				n2a(st.wYear,  &szTempDateTime[0], 4, 10);
				n2a(st.wMonth, &szTempDateTime[4], 2, 10);
				n2a(st.wDay,   &szTempDateTime[6], 2, 10);
				
				if(strncmp(szTempDateTime, m_sJnlInfo.m_szLocalDate, 8) != 0)	memcpy(m_sJnlInfo.m_szLocalDate, szTempDateTime, 8);

				nMod = 0;
			}

			nMod++;

			i = FindStr(m_sJnlInfo.m_szLocalDate, m_sJnlInfo.m_szLastJnlId, m_sJnlSend.m_szData);
			if (i < 0)	{
				return -1;
			}

			memcpy(&this->m_sJnlSend.m_szClientId1[0], &this->m_szJumCode[0], 4);
			memcpy(&this->m_sJnlSend.m_szClientId2[0], &this->m_szAtmNum[0], 3);
			memcpy(&this->m_sJnlSend.m_szClientIp[0], &this->m_szLocalIp[0], strlen(this->m_szLocalIp));
			memcpy(&this->m_sJnlSend.m_szMsgId[0], "WZ02", 4);
			memcpy(&this->m_sJnlSend.m_szMsgResult[0], "00", 2);
			memset(&this->m_sJnlSend.m_szReserved[0], 0x20, sizeof(&this->m_sJnlSend.m_szReserved[0]));
			memcpy(&this->m_sJnlSend.m_szReserved[0], &m_sJnlInfo.m_szLastJnlId[6], 11);

			sprintf(szDataLen, "%010d", i + 40);
			memcpy(&this->m_sJnlSend.m_szDataLen[0], szDataLen, 10);

			nLen = i + 50;	// 실데이터 길이...
			*snd = 't';
		}

		if(nMode == T_IMGFACE || nMode == T_IMGCARD)
		{
			memset(&this->m_sJnlSend, 0x20, sizeof(JnlMaster));

			nLen = FindImage(nMode, &this->m_sJnlInfo.m_szLastJnlId[0], &this->m_sJnlSend);
			/*
			if(nLen < 0)
			{
				if(imageQueue.size() > 0)	
				{
					info = imageQueue.front();
					
					nLen = FindImageFromQueue((char *)info.szPath, &this->m_sJnlSend, &nMode);
					if(nLen < 0)	
					{
						if(info.nCount > IMAGE_RETRY)	
							imageQueue.pop();
						else
						{
							info.nCount++;
							imageQueue.pop();
							imageQueue.push(info);
						}
					}
					else	
					{
						if(!imageQueue.empty())	imageQueue.pop();	// 2011.10.20 추가..
					}
				}
			}
			*/
			if(nMode == T_IMGFACE)
				*snd = 'i';
			else	
				*snd = 'c';
		}

		return nLen;
	}

	/*
	int MkData(int nMode, char *snd)
	{
		int nLen = 0;
		int i = 0, stat = 0;
		char ipath[MAX_PATH] = { 0 };
		unsigned int u_ret = 0;
		char szDataLength[20] = { 0, };
		char szDataLen[20] = { 0, };
		static int nMod = 0;
		SYSTEMTIME st;
		char szTempDateTime[30] = { 0, };
		static int nIterator = 0;
		int nRet = 0;
		MYIMAGEINFO info;


		if (nIterator == 0)
		{
			GetConfiguration();

			nIterator++;

			if (nIterator == 50)	nIterator = 0;
		}

		if (nMode == T_POLL)
		{
			GetLocalTime(&st);

			if (st.wHour >= 7 && st.wHour < 8 && m_bLogin == false)
			{
				SockReset();
				SetPollClock(120);

				return 0;
			}

			// 전원 미 OFF 대비 
			if (nMod >= 15)
			{
				GetLocalTime(&st);

				n2a(st.wYear, &szTempDateTime[0], 4, 10);
				n2a(st.wMonth, &szTempDateTime[4], 2, 10);
				n2a(st.wDay, &szTempDateTime[6], 2, 10);

				if (strncmp(szTempDateTime, m_sJnlInfo.m_szLocalDate, 8) != 0)
				{
					memcpy(m_sJnlInfo.m_szLocalDate, szTempDateTime, 8);
					Logger("Info: No power shutdown, occured, anchor date re settings");
				}

				nMod = 0;
			}

			nMod++;

			memset(&this->m_sJnlSend, 0x20, sizeof(JnlMaster));

			memcpy(&this->m_sJnlSend.m_szMsgId[0], "WZ04", 4);
			memcpy(&this->m_sJnlSend.m_szMsgResult[0], "00", 2);
			memcpy(&this->m_sJnlSend.m_szClientIp[0], &this->m_szLocalIp[0], strlen(this->m_szLocalIp));
			memcpy(&this->m_sJnlSend.m_szClientId1[0], this->m_szJumCode, 4);
			memcpy(&this->m_sJnlSend.m_szClientId2[0], this->m_szAtmNum, 3);
			memset(&this->m_sJnlSend.m_szReserved[0], 0x20, sizeof(this->m_sJnlSend.m_szReserved));

			sprintf(szDataLength, "%010d", 40);

			memcpy(&this->m_sJnlSend.m_szDataLen[0], szDataLength, 10);

			return 50;
		}

		if (nMode == T_DREQ || nMode == T_DCOMP)
		{
			memset(&this->m_sJnlSend, 0x20, sizeof(JnlMaster));

			if (nMode == T_DREQ)
				memcpy(&this->m_sJnlSend.m_szMsgId[0], "WZ91", 4);
			else	memcpy(&this->m_sJnlSend.m_szMsgId[0], "WZ93", 4);

			memcpy(&this->m_sJnlSend.m_szMsgResult[0], "00", 2);
			memcpy(&this->m_sJnlSend.m_szClientIp[0], &this->m_szLocalIp[0], strlen(this->m_szLocalIp));
			memcpy(&this->m_sJnlSend.m_szClientId1[0], this->m_szJumCode, 4);
			memcpy(&this->m_sJnlSend.m_szClientId2[0], this->m_szAtmNum, 3);
			memset(&this->m_sJnlSend.m_szReserved[0], 0x20, sizeof(this->m_sJnlSend.m_szReserved));
			memcpy(&this->m_sJnlSend.m_szData[0], this->m_strAgtVer.c_str(), 2);

			nLen = 50;

			if (nMode == T_DREQ)
			{
				sprintf(szDataLength, "%010d", 42);
				nLen = 52;
			}
			else	sprintf(szDataLength, "%010d", 40);

			memcpy(&this->m_sJnlSend.m_szDataLen[0], szDataLength, 10);

			return nLen;
		}


		if (nMode == T_LOGIN)
		{
			memset(&this->m_sJnlSend, 0x20, sizeof(JnlMaster));

			memcpy(&this->m_sJnlSend.m_szMsgId[0], "WZ01", 4);
			memcpy(&this->m_sJnlSend.m_szMsgResult[0], "00", 2);
			memcpy(&this->m_sJnlSend.m_szClientIp[0], &this->m_szLocalIp[0], strlen(this->m_szLocalIp));
			memcpy(&this->m_sJnlSend.m_szClientId1[0], this->m_szJumCode, 4);
			memcpy(&this->m_sJnlSend.m_szClientId2[0], this->m_szAtmNum, 3);
			memset(&this->m_sJnlSend.m_szReserved[0], 0x20, sizeof(this->m_sJnlSend.m_szReserved));

			memcpy(&this->m_sJnlSend.m_szData[0], this->m_szCPU, min(strlen(this->m_szCPU), 50));
			memcpy(&this->m_sJnlSend.m_szData[50], this->m_szCPUClock, min(strlen(this->m_szCPUClock), 60));
			memcpy(&this->m_sJnlSend.m_szData[110], this->m_szFMemory, min(strlen(this->m_szFMemory), 5));
			memcpy(&this->m_sJnlSend.m_szData[115], this->m_szTHdd, min(strlen(this->m_szTHdd), 5));
			memcpy(&this->m_sJnlSend.m_szData[120], this->m_szFHdd, min(strlen(this->m_szFHdd), 5));
			memcpy(&this->m_sJnlSend.m_szData[125], this->m_szOSInfo, min(strlen(this->m_szOSInfo), 100));
			memcpy(&this->m_sJnlSend.m_szData[225], this->m_szVendor, 1);
			memcpy(&this->m_sJnlSend.m_szData[226], this->m_szModel, 2);
			memcpy(&this->m_sJnlSend.m_szData[228], this->m_szKindCD, 1);
			memcpy(&this->m_sJnlSend.m_szData[229], this->m_szMacAddress, 12);
			memcpy(&this->m_sJnlSend.m_szData[241], this->m_szAssetID, 10);

			// 2009.11.28 agent version 정보 추가
			memcpy(&this->m_sJnlSend.m_szData[251], m_strAgtVer.c_str(), 2);
			memset(&this->m_sJnlSend.m_szData[253], 0x20, 199);

			sprintf(szDataLength, "%010d", 490);

			memcpy(&this->m_sJnlSend.m_szDataLen[0], szDataLength, 10);

			return 500;
		}

		if (nMode == T_DATA)
		{
			memset(&this->m_sJnlSend, 0x20, sizeof(JnlMaster));

			// 전원 미 OFF 대비 
			if (nMod >= 15)
			{
				GetLocalTime(&st);

				n2a(st.wYear, &szTempDateTime[0], 4, 10);
				n2a(st.wMonth, &szTempDateTime[4], 2, 10);
				n2a(st.wDay, &szTempDateTime[6], 2, 10);

				if (strncmp(szTempDateTime, m_sJnlInfo.m_szLocalDate, 8) != 0)	memcpy(m_sJnlInfo.m_szLocalDate, szTempDateTime, 8);

				nMod = 0;
			}

			nMod++;

			i = FindStr(m_sJnlInfo.m_szLocalDate, m_sJnlInfo.m_szLastJnlId, m_sJnlSend.m_szData);
			if (i < 0)	{
				return -1;
			}

			memcpy(&this->m_sJnlSend.m_szClientId1[0], &this->m_szJumCode[0], 4);
			memcpy(&this->m_sJnlSend.m_szClientId2[0], &this->m_szAtmNum[0], 3);
			memcpy(&this->m_sJnlSend.m_szClientIp[0], &this->m_szLocalIp[0], strlen(this->m_szLocalIp));
			memcpy(&this->m_sJnlSend.m_szMsgId[0], "WZ02", 4);
			memcpy(&this->m_sJnlSend.m_szMsgResult[0], "00", 2);
			memset(&this->m_sJnlSend.m_szReserved[0], 0x20, sizeof(&this->m_sJnlSend.m_szReserved[0]));
			memcpy(&this->m_sJnlSend.m_szReserved[0], &m_sJnlInfo.m_szLastJnlId[6], 11);

			sprintf(szDataLen, "%010d", i + 40);
			memcpy(&this->m_sJnlSend.m_szDataLen[0], szDataLen, 10);

			nLen = i + 50;	// 실데이터 길이...
			*snd = 't';
		}

		if (nMode == T_IMGFACE || nMode == T_IMGCARD)
		{
			memset(&this->m_sJnlSend, 0x20, sizeof(JnlMaster));

			nLen = FindImage(nMode, &this->m_sJnlInfo.m_szLastJnlId[0], &this->m_sJnlSend);
			if (nLen < 0)
			{
				if (imageQueue.size() > 0)
				{
					info = imageQueue.front();

					nLen = FindImageFromQueue((char *)info.szPath, &this->m_sJnlSend, &nMode);
					if (nLen < 0)
					{
						if (info.nCount > 1024)
							imageQueue.pop();
						else
						{
							info.nCount++;
							imageQueue.pop();
							imageQueue.push(info);
						}
					}
					else
					{
						if (!imageQueue.empty())	imageQueue.pop();	// 2011.10.20 추가..
					}
				}
			}

			if (nMode == T_IMGFACE)
				*snd = 'i';
			else
				*snd = 'c';
		}

		return nLen;
	}
	*/
	int FindImageFromQueue(char *szPath, JnlMaster *sMaster, int *nMode)
	{
		WIN32_FIND_DATAA wd;
		HANDLE hFind;
		char szPathFile[MAX_PATH] = {0, };
		char szTgtFile[MAX_PATH] = {0, };
		unsigned int uFileSize;
		FILE *fOpen = NULL;
		int nLen = 0;
		char szDataLen[20] = {0, };
		char szTemp[MAX_PATH] = {0, };
		DWORD dwRet = 0;

		strcpy(szTgtFile, szPath);
		szTgtFile[min(strlen(szPath) - 1, MAX_PATH - 1)] = '2';

		sprintf(&szPathFile[0], szPath);
		
		if(szPathFile[40] == 'c' || szPathFile[40] == 'C')	*nMode = T_IMGCARD;
		else							*nMode = T_IMGFACE;

		hFind = FindFirstFileA(szPathFile, &wd);
		if(hFind == INVALID_HANDLE_VALUE)
			return -1;
		
		FindClose(hFind);
		
		EnterCriticalSection(&GCS);		
		dwRet = vis_jp2_encoder(szPathFile, szTgtFile, "0.5");	// jpg -> jp2
		LeaveCriticalSection(&GCS);

		if(dwRet == 1)
		{
			DeleteFile(szPathFile);	// jpg 삭제
			CopyFile(szTgtFile, szPathFile, FALSE);
		}
		
		DeleteFile(szTgtFile);
		
		if((fOpen = fopen(szPathFile, "r+b")) == NULL)	return -1;
		
		uFileSize = _filelength(_fileno(fOpen));
		if(uFileSize > (1024 * 8))	
		{
			fclose(fOpen);
			return -1;
		}
		
		// Set Image Name...
		memset(&sMaster->m_szData[0], 0x00, sizeof(sMaster->m_szData));
		memset(&sMaster->m_szData[0], 0x20, 25);
		memcpy(&sMaster->m_szData[0], &wd.cFileName[0], min(strlen(wd.cFileName), 25));
		n2a(uFileSize, (char *)&sMaster->m_szData[25], 10, 10);
		
		nLen = fread(&sMaster->m_szData[35], uFileSize, sizeof(char), fOpen);
		fclose(fOpen);
		
		// 전체데이터 길이 = 50 + 25 + File Length - 10;
		if(nLen < 0)	return -1;
		
		memcpy(&sMaster->m_szClientId1[0], &this->m_szJumCode[0], 4);
		memcpy(&sMaster->m_szClientId2[0], &this->m_szAtmNum[0], 3);
		memcpy(&sMaster->m_szClientIp[0], &this->m_szLocalIp[0], strlen(this->m_szLocalIp));
		memcpy(&sMaster->m_szMsgId[0], "WZ03", 4);
		memcpy(&sMaster->m_szMsgResult[0], "00", 2);
		memset(&sMaster->m_szReserved[0], 0x20, sizeof(&sMaster->m_szReserved[0]));
		
		sprintf(szDataLen, "%010d", (int)(75 + uFileSize));
		
		memcpy(&sMaster->m_szDataLen[0], szDataLen, 10);
		
		nLen = (75 + uFileSize  + 10);
		
		return nLen;
	}

	int FindImage(int nMode, char *pszFile, JnlMaster *sMaster)
	{
		WIN32_FIND_DATAA wd;
		HANDLE hFind;
		char szPathFile[MAX_PATH] = {0, };
		char szTgtFile[MAX_PATH] = {0, };
		unsigned int uFileSize;
		FILE *fOpen = NULL;
		int nLen = 0;
		char szDataLen[20] = {0, };
		char szTemp[MAX_PATH] = {0, };
		MYIMAGEINFO ImageInfo;
		DWORD dwRet = 0;

		szTgtFile[0] = szPathFile[0] = m_cCurrDir;	//szTemp[0];

		if(nMode == T_IMGFACE)	
		{
			sprintf(&szPathFile[1], ":\\atms\\img\\%.6s\\%.2s\\%.17s_I01.jpg", &pszFile[0], &pszFile[6], &pszFile[0]);
			sprintf(&szTgtFile[1], ":\\atms\\img\\%.6s\\%.2s\\%.17s_I01.jp2", &pszFile[0], &pszFile[6], &pszFile[0]);
		}
		else			
		{
			sprintf(&szPathFile[1], ":\\atms\\img\\%.6s\\%.2s\\%.17s_C01.jpg", &pszFile[0], &pszFile[6], &pszFile[0]);
			sprintf(&szTgtFile[1], ":\\atms\\img\\%.6s\\%.2s\\%.17s_C01.jp2", &pszFile[0], &pszFile[6], &pszFile[0]);
		}

		hFind = FindFirstFileA(szPathFile, &wd);
		if(hFind == INVALID_HANDLE_VALUE)	
		{
			if(imageQueue.size() > 1024)
			{
				Logger("Info: Queue Size over not push queue");
				return -1;
			}
			else
			{
			//	ImageInfo.Clear();
			//	strcpy((char *)ImageInfo.szPath, (char *)szPathFile);
			//	imageQueue.push(ImageInfo);

			//	Logger("INFO : Image not found push queue file name %s", szPathFile);

			//	std::find(visited.begin(), visited.end(), x) != visited.end()
			//	DWORD
			}

			return -1;
		}

		FindClose(hFind);

		EnterCriticalSection(&GCS);
		dwRet = vis_jp2_encoder(szPathFile, szTgtFile, "0.5");
		LeaveCriticalSection(&GCS);

		if(dwRet == 1)
		{
			DeleteFile(szPathFile);	// jpg 삭제
			CopyFile(szTgtFile, szPathFile, FALSE);
		}

		DeleteFile(szTgtFile);
		
		if((fOpen = fopen(szPathFile, "r+b")) == NULL)	return -1;

		uFileSize = _filelength(_fileno(fOpen));

		if(uFileSize > (1024 * 8))	
		{
			fclose(fOpen);
			return -1;
		}

		// Set Image Name...
		memset(&sMaster->m_szData[0], 0x00, sizeof(sMaster->m_szData));
		memset(&sMaster->m_szData[0], 0x20, 25);
		memcpy(&sMaster->m_szData[0], &wd.cFileName[0], min(strlen(wd.cFileName), 25));
		n2a(uFileSize, (char *)&sMaster->m_szData[25], 10, 10);

		nLen = fread(&sMaster->m_szData[35], uFileSize, sizeof(char), fOpen);
		fclose(fOpen);

		// 전체데이터 길이 = 50 + 25 + File Length - 10;
		if(nLen < 0)	return -1;

		memcpy(&sMaster->m_szClientId1[0], &this->m_szJumCode[0], 4);
		memcpy(&sMaster->m_szClientId2[0], &this->m_szAtmNum[0], 3);
		memcpy(&sMaster->m_szClientIp[0], &this->m_szLocalIp[0], strlen(this->m_szLocalIp));
		memcpy(&sMaster->m_szMsgId[0], "WZ03", 4);
		memcpy(&sMaster->m_szMsgResult[0], "00", 2);
		memset(&sMaster->m_szReserved[0], 0x20, sizeof(&sMaster->m_szReserved[0]));
		
		sprintf(szDataLen, "%010d", (int)(75 + uFileSize));

		memcpy(&sMaster->m_szDataLen[0], szDataLen, 10);

		nLen = (75 + uFileSize  + 10);

		return nLen;
	}

	int FindStr(char *pszLoDate, char *pszAnchor, BYTE *pszBuffer)
	{
		char szPath[MAX_PATH] = {0, };
		char szTemp[MAX_PATH] = {0, };
		char szCopy[MAX_PATH] = {0, };
		char szCopyTemp[MAX_PATH] = {0, };

		char szSndPath[MAX_PATH] = {0, };

		char szDataLen[11] = {0, };
		
		WIN32_FIND_DATAA wd;
		WIN32_FIND_DATAA sndmark;
		
		HANDLE hFind;
		HANDLE hFindSnd;

		BOOL bFind = false;
		BOOL bGood = true;
		BOOL bRet = FALSE;
		int iOpen;
		int fSize;
		int i, nLen, count;
		static int snRetryCnt = 0;
		static int snReadCnt = 0;
		static int snOpenCnt = 0;
		string str;
		FILE *fp = NULL;

		szCopy[0] = szTemp[0] = szPath[0] = m_cCurrDir;

		// C:\CCMS\JNL\200905\21\20090521121718123.JNL
		sprintf(&szPath[1], ":\\atms\\JNL\\%.6s\\%.2s\\*.jnl", &pszAnchor[0], &pszAnchor[6]);
		sprintf(&szCopy[1], ":\\atms\\JNL\\%.6s\\%.2s\\", &pszAnchor[0], &pszAnchor[6]);

		hFind = FindFirstFileA(szPath, &wd);
		if(hFind == INVALID_HANDLE_VALUE)
		{
			if(strncmp(&pszLoDate[0], &pszAnchor[0], 8) != OK)
			{
				memset(&pszAnchor[8], 0x30, 9);
				IncreaseDate(&pszAnchor[0]);
			}

			::Sleep(2);

			return -1;
		}
		else
		{
 			while(bGood)
			{
				if(!(wd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					if(strncmp(&wd.cFileName[0], &pszAnchor[0], 17) > 0)	
					{
						bFind = true;

						break;
					}
					else
					{
						
					//	0	  1         2
					//	012345678901234567890
					//	20101018121314123.jnl
					//	전송 완료된 파일인지 아닌지 검사함...
						sprintf(&szSndPath[0], "%c:\\atms\\JNL\\%.6s\\%.2s\\snd\\%s", szCopy[0], &wd.cFileName[0], &wd.cFileName[6], &wd.cFileName[0]);
						hFindSnd = FindFirstFile(szSndPath, &sndmark);

						// 2011.10.20 추가... 미전송 데이터 생성 snd 횟숫 검사해서, 10번 이하일때만 재전송
						if(m_nSendCreateionFailCount < 10 && hFindSnd == INVALID_HANDLE_VALUE)
						{
							// Find 미전송, 파일 발견, 무조건 전송,... 점/기번 체크 필요없음... 충돌나도 상관없지?...
							sprintf(&szSndPath[0], "%c:\\atms\\JNL\\%.6s\\%.2s\\%s", szCopy[0], &wd.cFileName[0], &wd.cFileName[6], &wd.cFileName[0]);

							Logger("Confirm: 미전송 데이터 파일 path %s", szSndPath);
							
							iOpen = _open(szSndPath, _O_BINARY);
							if(iOpen < 0)	
							{
								// 빈파일정보 생성...
								sprintf(&szCopy[1], ":\\atms\\JNL\\%.6s\\%.2s\\snd", &wd.cFileName[0], &wd.cFileName[6]);
								WizardSndMark(szCopy, wd.cFileName);
								
								FindClose(hFind);

								Logger("Fault: 미전송 파일 오픈 에러 발생, %s, Send Mark 생성 Skip", szCopy);

								return -1;
							}
							
							fSize = _filelength(iOpen);
							if(fSize <= 0)
							{
								_close(iOpen);
								Sleep(10);

								// 빈파일정보 생성...
								sprintf(&szCopy[1], ":\\atms\\JNL\\%.6s\\%.2s\\snd", &wd.cFileName[0], &wd.cFileName[6]);
								WizardSndMark(szCopy, wd.cFileName);
								
								FindClose(hFind);

								Logger("Fault: 미전송 파일 싸이즈 에러 발생, %s, Send Mark 생성 Skip", szCopy);

								return -1;
							}
							
							fSize = _read(iOpen, &m_sJnlSend.m_szData[0], fSize);
							if(fSize <= 0)
							{
								_close(iOpen);
								::Sleep(10);
								
								
								// 빈파일정보 생성...
								sprintf(&szCopy[1], ":\\atms\\JNL\\%.6s\\%.2s\\snd", &wd.cFileName[0], &wd.cFileName[6]);
								WizardSndMark(szCopy, wd.cFileName);
								
								FindClose(hFind);

								Logger("Fault: 미전송 파일 읽기 에러 발생, %s, Send Mark 생성 Skip", szCopy);

								return -1;
							}

							_close(iOpen);

							memcpy(&this->m_sJnlSend.m_szClientId1[0], &this->m_szJumCode[0], 4);
							memcpy(&this->m_sJnlSend.m_szClientId2[0], &this->m_szAtmNum[0], 3);
							memcpy(&this->m_sJnlSend.m_szClientIp[0], &this->m_szLocalIp[0], strlen(this->m_szLocalIp));
							memcpy(&this->m_sJnlSend.m_szMsgId[0], "WZ02", 4);
							memcpy(&this->m_sJnlSend.m_szMsgResult[0], "00", 2);
							memset(&this->m_sJnlSend.m_szReserved[0], 0x20, sizeof(&this->m_sJnlSend.m_szReserved[0]));
							memcpy(&this->m_sJnlSend.m_szReserved[0], &m_sJnlInfo.m_szLastJnlId[6], 11);

							sprintf(szDataLen, "%010d", fSize + 40);
							memcpy(&this->m_sJnlSend.m_szDataLen[0], szDataLen, 10);
							
							for(int cnt = 0; cnt < 2; cnt++)
							{
								i = SendSock(fSize + 40 + 10, (char *)&this->m_sJnlSend);
								if(i == OK)
								{
									Logger("Success: 미전송 데이터 전송 성공, %dBytes", fSize + 40 + 10);

									break;
								}

								Sleep(10);
							}

							i = RecvSock(T_DATA);
							if(i == OK)
							{
								Logger("Success: 미전송 데이터 전송, Read 성공 Server Result Code %.2s", &this->m_sJnlRecv.m_szMsgResult[0]);
							}
							else
								Logger("Failed: 미전송 데이터 전송, Read 실패");

							// 빈파일정보 생성...
							sprintf(&szCopy[1], ":\\atms\\JNL\\%.6s\\%.2s\\snd", &wd.cFileName[0], &wd.cFileName[6]);
							WizardSndMark(szCopy, wd.cFileName);
							
							FindClose(hFind);
							
							Logger("Confirm: 미전송 데이터 Send Mark %s", szCopy);
							
							return -1;
						}
						else 
						{
							// 2011.10.20 추가,... 위에서 INVALID_HANDLE_VALUE가 아닐때만 삭제
							if(hFind != INVALID_HANDLE_VALUE)
							{
								FindClose(hFindSnd);

								strcpy(szCopyTemp, szCopy);
								strcat(szCopyTemp, wd.cFileName);
								bRet = DeleteFile(szCopyTemp);

								Logger("DEBUG : %s File delete, return value %d", szCopyTemp, bRet);
							}
						}
					}
				}

				bGood = FindNextFileA(hFind, &wd);

				::Sleep(1);
			}
		}

		FindClose(hFind);

		if(bFind)	// Find Data File
		{
			szPath[0] = szTemp[0];
			szCopy[0] = szTemp[0];

			sprintf(&szPath[1], ":\\atms\\JNL\\%.6s\\%.2s\\%s", &pszAnchor[0], &pszAnchor[6], wd.cFileName);
			iOpen = _open(szPath, _O_BINARY);
			if(iOpen < 0)	
			{
				// 기준일을 변경하지 않고 다음번에 다시 시도함...
				::Sleep(10);
				if(snOpenCnt < 2)
				{
					snOpenCnt++;
					return -1;
				}
				else	memcpy(&pszAnchor[8], &wd.cFileName[8], 9);	// 다음파일을 찾는다.

				return -1;
			}
			else	snOpenCnt = 0;

			fSize = _filelength(iOpen);
			if(fSize <= 0)	
			{	
				_close(iOpen);
				::Sleep(10);

				if(snRetryCnt < 5)	
				{		
					snRetryCnt++;

					return -1;
				}
				else	// 30회 시도를 했지만 역시 파일 싸이즈가 0임 포기 다음 파일 찾자
					memcpy(&pszAnchor[8], &wd.cFileName[8], 9);	// 다음파일을 찾는다.
				
				snRetryCnt = 0;

				return -1;
			}
			else	snRetryCnt = 0;

 			fSize = _read(iOpen, pszBuffer, fSize);
			if(fSize <= 0)
			{
				_close(iOpen);
				::Sleep(10);
				
				if(snReadCnt < 2)
				{
					snReadCnt++;
					return -1;
				}
				else	//2009.12.27일 추가,... 30회 시도를 했지만 역시 파일 싸이즈가 0임 포기 다음 파일 찾자
					memcpy(&pszAnchor[8], &wd.cFileName[8], 9);	// 다음파일을 찾는다.

				return -1;
			}
			else	snReadCnt = 0;

			bGood = TRUE;

			string siteno;
			string atmno;

			char szSiteNo[4] = {0, };
			char szAtmNo[4] = {0, };

			memcpy(szSiteNo, &pszBuffer[30], 3);
			memcpy(szAtmNo, &pszBuffer[38], 2);

			// 2009.12.31 긴급 패치, 영업점번, 기기번호를 atms.ini에 설정된 점/기번과 비교하여, 
			// 같지 않으면 다른 폴더로 이동시킨다.
			if(strncmp(&m_szJumCode[1], (char *)&pszBuffer[30], 3) != 0 ||	// 둘중 하나만 같지 않아도 이상 전문
			   strncmp(&m_szAtmNum[1], (char *)&pszBuffer[38], 2) != 0)	// 이름을 바꾸거나, 또는 결번처리
			{
				// 단, 깨진 전문은 아니여야한다. 깨진 전문은 옴기지 않고, 걍 놔비둔다
				for(i = 0; i < 3; i++)
				{
					if(isdigit(pszBuffer[30 + i]))	continue;
					else				
					{
						bGood = FALSE;
						break;
					}
				}

				if(bGood)
				{
					for(i = 0; i < 2; i++)
					{
						if(isdigit(pszBuffer[38 + i]))	continue;
						else				
						{
							bGood = FALSE;
							break;
						}
					}
				}

				if(bGood)	// 이름을 바꾼다, skip함
				{
					_close(iOpen);

					str = szPath;
					i = str.find(".jnl");
					if(i > 0)	str = str.replace(i, 4, ".con");

					MoveFile(szPath, str.c_str());
					DeleteFile(szPath);
					
					memcpy(&pszAnchor[8], &wd.cFileName[8], 9);		// update key value....효성 때문(고척동 출장소)

					return -1;
				}
				else // 2010.01.10 결번 처리 추가
				{
					str = wd.cFileName;
					_close(iOpen);

					memcpy(&this->m_sJnlSend.m_szClientId1[0], &this->m_szJumCode[0], 4);				
																					// 4byte
					memcpy(&this->m_sJnlSend.m_szClientId2[0], &this->m_szAtmNum[0], 3);					
																					// 7byte
					memcpy(&this->m_sJnlSend.m_szClientIp[0], &this->m_szLocalIp[0], min(15, strlen(this->m_szLocalIp)));
					memcpy(&this->m_sJnlSend.m_szMsgId[0], "WZ07", 4);
					memcpy(&this->m_sJnlSend.m_szMsgResult[0], "00", 2);
					memset(&this->m_sJnlSend.m_szReserved[0], 0x20, sizeof(&this->m_sJnlSend.m_szReserved[0]));		
																					// 40byte
					memcpy(&this->m_sJnlSend.m_szData[0], str.c_str(), 17);			// 57byte
					if(isdigit(pszBuffer[40]))	
						this->m_sJnlSend.m_szData[17] = pszBuffer[40];				// 58byte
					else				
						this->m_sJnlSend.m_szData[17] = ' ';

					memcpy(&this->m_sJnlSend.m_szData[18], this->m_szVendor, 1);	// 59byte
					memcpy(&this->m_sJnlSend.m_szData[19], this->m_szKindCD, 1);	// 60byte
					memcpy(&this->m_sJnlSend.m_szData[20], this->m_szModel, 2);		// 62byte

					sprintf(szDataLen, "%010d", 62);
					memcpy(&this->m_sJnlSend.m_szDataLen[0], szDataLen, 10);		// 62byte 셋

					nLen = 72;
					
					for(count = 0; count < 2; count++)
					{
						i = SendSock(nLen, (char *)&m_sJnlSend);
						if(i == OK)	break;
					}
					
					i = RecvSock(0);

					str = szPath;
					i = str.find(".jnl");
					if(i > 0)	str = str.replace(i, 4, ".lst");	// 결번파일 lst 로 변경

					memcpy(&pszAnchor[8], &wd.cFileName[8], 9);		// update key value....효성 때문(고척동 출장소)
					bGood = MoveFile(szPath, str.c_str());
					if(!bGood)	DeleteFile(szPath);

					return -1;
				}
			}
			
			// 빈파일정보 생성...
			// 2009.12.07 체크...
			sprintf(&szCopy[1], ":\\atms\\JNL\\%.6s\\%.2s\\snd", &pszAnchor[0], &pszAnchor[6]);
			WizardSndMark(szCopy, wd.cFileName);

			memcpy(&pszAnchor[8], &wd.cFileName[8], 9);
			_close(iOpen);

			return fSize;
		}
		else
		{
			if(strncmp(&pszLoDate[0], &pszAnchor[0], 8) != OK)
			{
				memset(&pszAnchor[8], 0x30, 9);
				IncreaseDate(&pszAnchor[0]);

				::Sleep(10);

				return -1;
			}
		}

		::Sleep(50);

		return -1;
	}
	
	int a2i(char *pszSrc, int nLength)
	{
		char szBuffer[20];

		memset(szBuffer, 0x00, sizeof(szBuffer));

		memcpy(szBuffer, pszSrc, nLength);

		return atoi(szBuffer);
	}

	int IncreaseDate(char *pszDate)
	{
		int nYear, nMonth, nDate;

		nYear = a2i(&pszDate[0], 4);
		nMonth = a2i(&pszDate[4], 2);
		nDate = a2i(&pszDate[6], 2);

		if(nDate == 31)	
		{
			nDate = 1;

			if(nMonth == 12)	
			{
				nMonth = 1;
				nYear++;
			}
			else	nMonth++;
		}
		else	nDate++;
	
		n2a(nYear,  &pszDate[0], 4, 10);
		n2a(nMonth, &pszDate[4], 2, 10);
		n2a(nDate,  &pszDate[6], 2, 10);
	
		return 0;
	}

	int RecvSock(int nMode)
	{
		int req, left, ret, total;
		char *pStr = NULL;
		bool bfirst = true;
	
		total = 1024;
		DWORD dwCode;

#ifdef DEBUGMODE
		return OK;
#endif // DEBUGMODE

		left = total;
		req = ret = 0;

		memset(&this->m_sJnlRecv, 0x00, sizeof(JnlMaster));
		pStr = (char *)&this->m_sJnlRecv;

		SetClock(60);

		while(req != total && req < total)
		{
			if((ret = ::recv(this->m_sServSocket, pStr, left, 0)) <= 0)
			{
				if(GetClock() >= 0)	
				{
					::Sleep(10);
					continue;
				}
				else	
				{
					dwCode = WSAGetLastError();

					Logger("RECV : Recv Data Overtime Result Code : 1 WinSock ErrCode %d", dwCode);

					return NG;
				}
			}

			if(bfirst && ret > 10)	
			{
				total = a2i(pStr, 10) + 10;
				bfirst = false;
			}

			left -= ret;
			req += ret;
			pStr += ret;
		
			::Sleep(10);
		}

		return OK;
	}

	int SendSock(int nLen, char *pszBuffer)
	{
		int nRet = NG;
		int nTotal, nLeft;
		bool bSnd = false;

		nTotal = nLeft = 0;
		// 10초 Setting...
		this->SetClock(10);
		
#ifdef DEBUGMODE
		return OK;
#endif 
		while(nTotal != nLen && nTotal < nLen)
		{
			if(this->GetClock() <= 0)	
			{
				return NG;
				break;
			}

			nRet = ::send(this->m_sServSocket, &pszBuffer[nLeft], nLen - nLeft, 0);
			if(nRet == SOCKET_ERROR || nRet == INVALID_SOCKET || nRet <= 0)
			{
				if(WSAGetLastError() == WSAEWOULDBLOCK)
				{
					::Sleep(30);
					continue;
				}
				else 
				{
					if(bSnd == true)	
					{
						::Sleep(10);
						SetClock(2);
						continue;
					}
					else	
					{
						nRet = NG;
						return nRet;
						break;
					}
				}
			}

			bSnd = true;
			nTotal += nRet;
			nLeft += nRet;

			::Sleep(10);
		} 

		nRet = (nRet != NG) ? OK : NG;
		
		Logger("SEND : Send Data %dbyte Result Code : %d", nTotal, nRet);

		return nRet;
	}

	void SockReset()
	{
		this->m_bConnected = false;
		this->m_bLogin = false;
		this->m_nCurState = STATE_A;

		shutdown(this->m_sServSocket, 2);
		closesocket(this->m_sServSocket);
		
		this->m_sServSocket = INVALID_SOCKET;

		Logger("ERROR : Socket Error occured Reset Socket");
	}

	bool ServerConnect()
	{
		bool bRet = false;
		int i = NG;
		char szServIpAddr[20] = {0, };
		char szServPort[20] = {0, };
		struct sockaddr_in serv_addr;
		unsigned long uFmode = 1;
		fd_set fdset;
		struct timeval timevalue;
		static bool bLogging = false;
		
		GetConfiguration();

		if(this->m_bConnected == true)	return this->m_bConnected;

		this->m_nCurState = STATE_A;	// 연결 끊어졌다가 붙은 경우, 무조건 첨부터 다시 시작(LOGON --> DATA TRANSFER)

#ifdef DEBUGMODE
		// debug mode...
		bRet = this->m_bConnected = true;
		return bRet;
#endif // DEBUGMODE

		memset(szServIpAddr, 0, 20);
		
		if(this->m_sServSocket != INVALID_SOCKET)	
		{
			shutdown(this->m_sServSocket, 2);
			closesocket(this->m_sServSocket);
			this->m_sServSocket = INVALID_SOCKET;
			this->m_bConnected = false;
		}

		if((this->m_sServSocket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
		{
			this->m_sServSocket = INVALID_SOCKET;
			this->m_bConnected = false;

			return bRet;
		}

		if(ioctlsocket(m_sServSocket, FIONBIO, &uFmode) == -1)	
		{
			shutdown(this->m_sServSocket, 2);
			closesocket(this->m_sServSocket);
			this->m_sServSocket = INVALID_SOCKET;
			this->m_bConnected = false;

			return bRet;
		}
		
		memcpy(&szServIpAddr[0], &this->m_szServAddr[0], min(19, strlen(&this->m_szServAddr[0])));
		memcpy(&szServPort[0], &this->m_szServPort[0], min(19, strlen(&this->m_szServPort[0])));

		memset((char *)&serv_addr, 0x00, sizeof(struct sockaddr_in));

		serv_addr.sin_family      = AF_INET;
		serv_addr.sin_addr.s_addr = inet_addr(szServIpAddr);
		serv_addr.sin_port        = htons((u_short)atoi(szServPort));
	
		i = connect(this->m_sServSocket, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
		if(i == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
		{	
			if(!bLogging)
			{
				Logger("Socket Connect Error...connect api %d", WSAGetLastError());
				bLogging = true;
			}
			
			closesocket(this->m_sServSocket);
			this->m_sServSocket = INVALID_SOCKET;
			this->m_bConnected = false;

			Sleep(3 * 1000);

			return bRet;
		}
	
		FD_ZERO(&fdset);
	
		FD_SET(this->m_sServSocket, &fdset);
	
		timevalue.tv_sec = 5;
		timevalue.tv_usec = 0;
		
	//	CDPollCheck();

		if(select(0, NULL, &fdset, NULL, &timevalue) == SOCKET_ERROR)
		{
			if(!bLogging)
			{
				Logger("Socket Connect Error... select api %d", WSAGetLastError());
				bLogging = true;
			}

		//	CDPollCheck();

			shutdown(this->m_sServSocket, 2);
			closesocket(this->m_sServSocket);
			this->m_sServSocket = INVALID_SOCKET;
			this->m_bConnected = false;

			Sleep(3 * 1000);

			return bRet;
		}
	
		if(!FD_ISSET(this->m_sServSocket, &fdset))
		{
			if(!bLogging)
			{
				Logger("Socket Connect Error... FD_ISSET macro %d", WSAGetLastError());
				bLogging = true;
			}
			
		//	CDPollCheck();

			shutdown(this->m_sServSocket, 2);
			closesocket(this->m_sServSocket);
			this->m_sServSocket = INVALID_SOCKET;
			this->m_bConnected = false;

			Sleep(3 * 1000);

			return bRet;
		}
		
		bRet = this->m_bConnected = true;
		bLogging = false;

		return bRet;
	}

	void SetWClock(int nTime)
	{
		m_nWStartTime = clock();
		m_nTimeCount = nTime;                 /* time defined -----------------*/
	}

	int GetWClock()
	{
		m_nWLastTime = clock();
		return (m_nTimeCount - ((m_nWLastTime - m_nWStartTime) / CLOCKS_PER_SEC));
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

	void SetPollClock(int nTime)
	{
		m_nPStartTime = clock();
		m_nPTimeCount = nTime;                 /* time defined -----------------*/
	}

	int GetPollClock()
	{
		m_nPLastTime = clock();
		return (m_nPTimeCount - ((m_nPLastTime - m_nPStartTime) / CLOCKS_PER_SEC));
	}

	int DelFolder(char *pFolder)
	{
		SHFILEOPSTRUCT FileOp = {0};
		char szTemp[MAX_PATH] = {0};

		strcpy(szTemp, pFolder);
		FileOp.hwnd = NULL;
		FileOp.wFunc = FO_DELETE;
		FileOp.pFrom = NULL;
		FileOp.pTo = NULL;
		FileOp.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI; // 확인메시지가 안뜨도록 설정
		FileOp.fAnyOperationsAborted = 0;
		FileOp.hNameMappings = NULL;
		FileOp.lpszProgressTitle = NULL;
		FileOp.pFrom = szTemp;

		SHFileOperation(&FileOp);

		return 1;
	}

	int increase_date(char *finame)
	{
	int	year, month, date;

		year = a2i(&finame[0], 4);
		month = a2i(&finame[4], 2);
		date = a2i(&finame[6], 2);

		if(date == 31)	
		{
			date = 1;

			if(month == 12)	
			{
				month = 1;
				year++;
			}
			else	month++;
		}
		else	date++;

		n2a(year,  &finame[0], 4, 10);
		n2a(month, &finame[4], 2, 10);
		n2a(date,  &finame[6], 2, 10);

		return 0;
	}

	int decrease_date(char *finame)
	{
		int year, month, date;

		year = a2i(&finame[0], 4);
		month = a2i(&finame[4], 2);
		date = a2i(&finame[6], 2);

		if(date == 1)
		{
			date = 31;

			if(month == 1)
			{
				month = 12;
				year--;
			}
			else month--;
		}
		else date--;

		n2a(year,  &finame[0], 4, 10);
		n2a(month, &finame[4], 2, 10);
		n2a(date,  &finame[6], 2, 10);

		return 0;
	}

	int CcmsDelete(bool bJnl, char cDrive, char *szLocalDate, int nKeepDate)
	{
		static int first = 2;
		int nWeight = 0, i;
		char szDelIncDate[9] = {0, };
		char szDelAnchor[9] = {0, };
		char szPath[MAX_PATH] = {0, };
		char szTgtFile[MAX_PATH] = {0, };
		char szSndFile[MAX_PATH] = {0, };
		BOOL bGood = TRUE;
		HANDLE hFind;
		WIN32_FIND_DATA wd;

		if(first <= 0)	return 0;
		return 0;

		first--;

		Logger("bJnl %d cDrive %c szLocalDate %s nDelDate %d", bJnl, cDrive, szLocalDate, nKeepDate);

		memcpy(szDelIncDate, szLocalDate, 8);
		memcpy(szDelAnchor, szLocalDate, 8);

		if(nKeepDate > 30 && nKeepDate < 40)		nWeight = 60;
		else if(nKeepDate > 40 && nKeepDate < 50)	nWeight = 70;
		else if(nKeepDate > 50 && nKeepDate < 60)	nWeight = 80;
		else if(nKeepDate > 60 && nKeepDate < 70)	nWeight = 90;
		else if(nKeepDate > 70 && nKeepDate < 80)	nWeight = 100;
		else if(nKeepDate > 90 && nKeepDate < 100)	nWeight = 120;
		else						nWeight = nKeepDate * 3;

		for(i = 0; i < nWeight; i++)	decrease_date(szDelIncDate);
		for(i = 0; i < nKeepDate; i++)	decrease_date(szDelAnchor);

		while(strncmp(szDelAnchor, szDelIncDate, 8) >= 0)
		{
			Sleep(1);

			memset(szPath, 0x00, sizeof(szPath));
			if(bJnl)	sprintf(szPath, "%c:\\atms\\jnl\\%.6s\\%.2s\\*.*", cDrive, &szDelIncDate[0], &szDelIncDate[6]);
			else		sprintf(szPath, "%c:\\atms\\img\\%.6s\\%.2s\\*.*", cDrive, &szDelIncDate[0], &szDelIncDate[6]);
			
			Logger("Deleting Tgt file path %s", szPath);

			hFind = FindFirstFile(szPath, &wd);
			if(hFind != INVALID_HANDLE_VALUE )
			{
				// delete all file...
				while(bGood)
				{
					Sleep(1);
					
					if(!(wd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					{
						if(bJnl)
						{
							sprintf(szTgtFile, "%c:\\atms\\jnl\\%.6s\\%.2s\\%s", cDrive, &szDelIncDate[0], &szDelIncDate[6], wd.cFileName);
							sprintf(szSndFile, "%c:\\atms\\jnl\\%.6s\\%.2s\\Snd\\%s", cDrive, &szDelIncDate[0], &szDelIncDate[6], wd.cFileName);
						}
						else	
							sprintf(szTgtFile, "%c:\\atms\\img\\%.6s\\%.2s\\%s", cDrive, &szDelIncDate[0], &szDelIncDate[6], wd.cFileName);

						DeleteFile(szTgtFile);
					
						Logger("Deleted Tgt file file name %s", szTgtFile);

						if(bJnl)	DeleteFile(szSndFile);
					}
					
					bGood = FindNextFile(hFind, &wd);
				}

				FindClose(hFind);

				if(bJnl)	
				{
					sprintf(szPath, "%c:\\atms\\jnl\\%.6s\\%.2s", cDrive, &szDelIncDate[0], &szDelIncDate[6]);
					sprintf(szSndFile, "%c:\\atms\\jnl\\%.6s\\%.2s\\Snd", cDrive, &szDelIncDate[0], &szDelIncDate[6]);
				}
				else	sprintf(szPath, "%c:\\atms\\img\\%.6s\\%.2s", cDrive, &szDelIncDate[0], &szDelIncDate[6]);
				
				if(bJnl)	DelFolder(szSndFile);

				Logger("Deleted Tgt folder name %s", szPath);

				DelFolder(szPath);			// delete directory

				if(bJnl)	sprintf(szPath, "%c:\\atms\\jnl\\%.6s", cDrive, &szDelIncDate[0]);
				else		sprintf(szPath, "%c:\\atms\\img\\%.6s", cDrive, &szDelIncDate[0]);

				DelFolder(szPath);

			}	
			
			bGood = TRUE;

			increase_date(szDelIncDate);
		}

		return 0;
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
		sprintf(&strPath[strlen(strPath)], "%02d\\AgentSvc%02d.LOG", st.wMonth, st.wDay);

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

			for(int i = 0; i < 14; i++)
			{
				memset(strPath, 0x00, MAX_PATH);
				strPath[0] = m_cCurrDir;
				strcat(strPath, ":\\atms\\log\\");
				sprintf(&strPath[strlen(strPath)], "%02d\\AgentSvc%02d.LOG", t->tm_mon + 1, t->tm_mday + i);
				DeleteFile(strPath);
			}

			m_bStart = false;
		}

		return true;
	}

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

	void setUp(char curr_dir) {
		this->m_cCurrDir = curr_dir;
	}
};

#endif // __CSIMPLECASTER_H_INCLUDED__