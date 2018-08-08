#ifndef __SIMPLE_CLIENT_H_INCLUDED__
#define __SIMPLE_CLIENT_H_INCLUDED__

#include "inc/threading.h"
#include "inc/log.h"
#include "inc/simple_define.h"
#include "inc/flog.h"
#include "inc/sys_info.h"
#include "inc/simple_commander.h"
#include "inc/J2KEncoder.h"
#include "inc/J2KDecoder.h"
#include "inc/simple_debug.h"
#include <time.h>
#include <io.h>
#include <fcntl.h>
#include <vector>
#include <queue>
#include <string>
#include <Windows.h>

using namespace std;

class ClientSocket 
{
private:
	SOCKET m_Socket;
	QMutex m_qMutex;
	bool m_bConnected;

	int m_nTimeCount;
	clock_t m_nLastTime, m_nStartTime;

public:
	ClientSocket(): m_qMutex()
	{ 
		m_Socket = SOCKET_ERROR; 
		m_bConnected = false;
	}

	bool IsConnected() { return m_bConnected; }

	virtual ~ClientSocket() 
	{
		if(m_Socket != SOCKET_ERROR)	
		{
			shutdown(m_Socket, 2);
			closesocket(m_Socket);

			m_Socket = SOCKET_ERROR;
		}
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

	int WriteToSocket(int sentbytes, char *pBuffer, UINT wait_time)
	{
		int nLeft, nReq, nTotal;
		int nRet;

		nReq = sentbytes;

		nTotal = nLeft = 0;

		SetClock(wait_time);

		while(nTotal < nReq)
		{
			if(GetClock() <= 0)	return SOCKET_ERROR;

			nRet = send(this->m_Socket, &pBuffer[nLeft], nReq - nLeft, 0);
			if(nRet > 0)
			{
				nTotal += nRet;
				nLeft += nRet;
			}
		}

		nRet = (nTotal > 0) ? nTotal : SOCKET_ERROR;

		return nRet;
	}

	int ReadFromSocket(char *pBuffer, int wait_time)
	{
		int total, left, ret, req;
		bool settings = false;

		total = left = ret = req = 0;

		req = 50;

		SetClock(wait_time);

		while(total < req)
		{
			Sleep(2);

			if(GetClock() <= 0) return SOCKET_ERROR;

			ret = recv(m_Socket, (char *)&pBuffer[left], req - left, 0);
			if(ret > 0)
			{
				total += ret;
				left += ret;

				if(!settings && total >= 10)
				{
					settings = true;

					req = a2i((char *)&pBuffer[0], 10);
				}

				SetClock(10);
			}
		}

		ret = (total > 0) ? total : SOCKET_ERROR;

		return ret;
	}

	int a2i(char *pBuffer, int length)
	{
		char szTemp[15] = {0, };

		memcpy(szTemp, pBuffer, min(length, sizeof(szTemp) - 1));

		return atoi(szTemp);
	}

	SOCKET Connect(string host_addr, int port) 
	{
		struct sockaddr_in serv_addr;
		unsigned long uFmode = 1;
		fd_set fdset;
		struct timeval timevalue;

		if(m_bConnected == true)	return m_Socket;

		if(m_Socket != SOCKET_ERROR)	
		{
			shutdown(m_Socket, 2);
			closesocket(m_Socket);

			m_Socket = SOCKET_ERROR;
			m_bConnected = false;
		}

		if((this->m_Socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
		{
			this->m_Socket = SOCKET_ERROR;
			this->m_bConnected = false;

			return m_Socket;
		}

		if(ioctlsocket(m_Socket, FIONBIO, &uFmode) == -1)	
		{
			shutdown(this->m_Socket, 2);
			closesocket(this->m_Socket);
			this->m_Socket = SOCKET_ERROR;
			this->m_bConnected = false;

			return m_Socket;
		}

		memset((char *)&serv_addr, 0x00, sizeof(struct sockaddr_in));

		serv_addr.sin_family      = AF_INET;
		serv_addr.sin_addr.s_addr = inet_addr(host_addr.c_str());
		serv_addr.sin_port        = htons((u_short)port);

		int i = connect(this->m_Socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
		if(i == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
		{	
			closesocket(this->m_Socket);
			this->m_Socket = SOCKET_ERROR;
			this->m_bConnected = false;

			Sleep(3 * 1000);

			return m_Socket;
		}
	
		FD_ZERO(&fdset);
	
		FD_SET(this->m_Socket, &fdset);
	
		timevalue.tv_sec = 5;
		timevalue.tv_usec = 0;
		
		if(select(0, NULL, &fdset, NULL, &timevalue) == SOCKET_ERROR)
		{
			shutdown(this->m_Socket, 2);
			closesocket(this->m_Socket);
			this->m_Socket = SOCKET_ERROR;
			this->m_bConnected = false;

			Sleep(3 * 1000);

			return m_Socket;
		}
	
		if(!FD_ISSET(this->m_Socket, &fdset))
		{
			shutdown(this->m_Socket, 2);
			closesocket(this->m_Socket);
			this->m_Socket = SOCKET_ERROR;
			this->m_bConnected = false;

			Sleep(3 * 1000);

			return m_Socket;
		}
		
		this->m_bConnected = true;
		
		return m_Socket;
	}

	void Close()
	{
		if(this->m_Socket == SOCKET_ERROR)	return;

		shutdown(m_Socket, 2);
		closesocket(m_Socket);

		m_Socket = SOCKET_ERROR;
		m_bConnected = false;
	}
};

#endif // __SIMPLE_CLIENT_H_INCLUDED__