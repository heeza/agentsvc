#ifndef __SOCKET_SERVER_H_INCLUDED__
#define __SOCKET_SERVER_H_INCLUDED__

#include "inc/memory_manager.h"
#include "inc/socket_client.h"
#include "inc/simple_define.h"

template <class Type>
class ServerSocket
{
private:
	SOCKET m_sServSock;
	QueuedBlocks<ClientSocket<Type> > m_SockPool;
	LINGER lingerStruct;

public:
	ServerSocket(unsigned int nPort, unsigned int nMaxClients, 
		bool bInCreateAync = false, bool bBindLocal = true): m_SockPool(nMaxClients)
	{
		unsigned long u_flag = 1;
		int nRet = 0;
		struct sockaddr_in psAddrIn;

		// Overlapped style�� ������ ���� ��, ��Ʈ����->���ε�->����
		this->m_sServSock = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if(this->m_sServSock == SOCKET_ERROR || this->m_sServSock <= 0)	throw "WSASocket creation failed.";

		if(bInCreateAync == true)
		{
			nRet = ::ioctlsocket(this->m_sServSock, FIONBIO, &u_flag);
			if(nRet < 0)
			{
				closesocket(this->m_sServSock);
				throw "ioctlsocket creation failed.";
			}
		}

		memset(&psAddrIn, 0x00, sizeof(struct sockaddr_in));
		psAddrIn.sin_family = AF_INET;
		psAddrIn.sin_port = htons(nPort);
		
		if(bBindLocal)	psAddrIn.sin_addr.s_addr = inet_addr("127.0.0.1");
		else		psAddrIn.sin_addr.s_addr = htons(INADDR_ANY);

		nRet = ::bind(this->m_sServSock, (struct sockaddr *)&psAddrIn, sizeof(psAddrIn));
		if(nRet < 0)
		{
			closesocket(this->m_sServSock);
			throw "server socket failed to bind.";
		}

		nRet = ::listen(this->m_sServSock, 5);
		if(nRet < 0)
		{
			closesocket(this->m_sServSock);
			throw "server socket failed to listen.";
		}

		int nZero = 0;
		nRet = setsockopt(this->m_sServSock, SOL_SOCKET, SO_SNDBUF, (char *)&nZero, sizeof(nZero));
		if (SOCKET_ERROR == nRet) 
		{
		//	Flog::LogMessage(BOUT, "etsockopt(SNDBUF): %d\n", WSAGetLastError());
			throw "server etsockopt failed to listen.";
		}

		// Disable receive buffering on the socket.  Setting SO_RCVBUF 
		// to 0 causes winsock to stop bufferring receive and perform
		// receives directly from our buffers, thereby reducing CPU usage.
		nZero = 0;
		nRet = setsockopt(this->m_sServSock, SOL_SOCKET, SO_RCVBUF, (char *)&nZero, sizeof(nZero));
		if (SOCKET_ERROR == nRet) 
		{
		//	Flog::LogMessage(BOUT, "etsockopt(SO_RCVBUF): %d\n", WSAGetLastError());
			throw "server etsockopt failed to listen.";
		}
        
		nRet = setsockopt(this->m_sServSock, SOL_SOCKET, SO_LINGER,
				(char *)&lingerStruct, sizeof(lingerStruct) );
		if(SOCKET_ERROR == nRet) 
		{
		//	Flog::LogMessage(BOUT, "setsockopt(SO_LINGER): %d\n", WSAGetLastError());
			throw "server etsockopt failed to listen.";
		}
	}

	// client�� �����û�� �ް�, ���� Ǯ�κ��� Ŭ���̾�Ʈ ��ü instance�� �ϳ� ������, 
	// ������ Ŭ���̾�Ʈ ���������� ���� �� Ŭ���̾�Ʈ inatance ��ü ������ �������ش�.
	ClientSocket<Type>* Accept()
	{
		SOCKET sSocket = SOCKET_ERROR;
		struct sockaddr_in psAddIn;
		int nLength = sizeof(struct sockaddr_in);
		ClientSocket<Type> *pClient = NULL;

		sSocket = WSAAccept(this->m_sServSock, (struct sockaddr *)&psAddIn, &nLength, NULL, NULL);
		if(sSocket != INVALID_SOCKET)

		{
			pClient = this->m_SockPool.GetFromQueue();
			if(pClient)
			{
				pClient->Lock();
				pClient->Associate(sSocket, &psAddIn);
				pClient->Unlock();
			}
			else
			{
				::shutdown(sSocket, 2);
				::closesocket(sSocket);
			}
		}

		return pClient;
	}

	// ���Ϸ��, Ŭ���̾�Ʈ instance�� �ٽ� ���밡����, ���� Ǯ(ť)�� ��ȯ�ܴ�
	void Release(ClientSocket<Type> *pClient)
	{
		if(pClient != NULL)
		{
			if(pClient->IsBusy())	pClient->CloseSocket();

			this->m_SockPool.Release(pClient);
		}
	}

	vector<ClientSocket<Type> *> *GetPool() 
	{
		return m_SockPool.GetBlocks();
	};

	~ServerSocket()
	{
		::shutdown(this->m_sServSock, 2);
		::closesocket(this->m_sServSock);
	}
};

#endif // __SOCKET_SERVER_H_INCLUDED__