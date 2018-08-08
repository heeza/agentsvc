#ifndef __SAFE_QUEUE_H_INCLUDED__
#define __SAFE_QUEUE_H_INCLUDED__

#pragma warning(disable:4786)

//#include <queue>
#include <windows.h>
#include "inc/memory_manager.h"

using namespace std;

template <class Type> 
class CSafeQueue 
{ 
 protected: 
	mpriority_queue<Type> m_PQueue;
   	size_t m_MaxItems; 
	QMutex m_qMutex; 

public: 
	CSafeQueue(size_t maxItems = 0) : m_PQueue(), m_qMutex()
	{
		if(maxItems <= 0) 
			throw "Invalid capacity supplied.";

		m_PQueue.reserve(maxItems);
		m_MaxItems = maxItems;
	}

   	bool Push(const Type& elem) 
   	{ 	
		m_qMutex.Lock();
     		if(m_PQueue.size() >= m_MaxItems) 
		{
			m_qMutex.Unlock();
       			return false; 
		}
  
		m_PQueue.push(elem);
		m_qMutex.Unlock();

     		return true; 
   	} 
  
   	void Pop() 
   	{ 
		m_qMutex.Lock();
		m_PQueue.pop();
		m_qMutex.Unlock();
   	} 
  
   	void Clear()  
   	{  
		m_qMutex.Lock();
		while(!m_PQueue.empty())
			m_PQueue.pop();

		m_qMutex.Unlock();
   	} 
  
   	bool SetMaxItems(size_t maxItems) 
   	{  
		m_qMutex.Lock();

     		if(maxItems < m_MaxItems) 
		{
			if(maxItems < m_PQueue.size())
			{
				m_qMutex.Unlock();
         			return false; 
			}
		}

     		m_MaxItems = maxItems; 
		m_qMutex.Unlock();

     		return true; 
   	} 
   	
	Type Top() 
	{ 
		Type t;

		m_qMutex.Lock();
		t = m_PQueue.top(); 
		m_qMutex.Unlock();

		return t;
	}

   	size_t GetMaxItems() 
	{ 
		m_qMutex.Lock();
		size_t t = m_MaxItems;
		m_qMutex.Unlock();

		return t;
	} 
   	
   	bool Empty() 
	{ 
		m_qMutex.Lock();
		bool bRet = m_PQueue.empty(); 
		m_qMutex.Unlock();

		return bRet;
	} 
   	
   	size_t Count() 
	{ 
		size_t t;

		m_qMutex.Lock();
		t = m_PQueue.size(); 
		m_qMutex.Unlock();

		return m_PQueue.size(); 
	} 
}; 

template <class Type>
class StaticSafeQueue
{
private:
	static CSafeQueue<Type> *m_safeQueue;

public:
	static void Init(int nSize)
	{
		if(m_safeQueue == NULL)	m_safeQueue = new CSafeQueue<Type>(nSize);
	}

	static Type Top()	{ return m_safeQueue->Top(); }
	static void Pop()	{ m_safeQueue->Pop(); }
	static bool Push(const Type& elem)	{ return m_safeQueue->Push(elem); }
	static void Clear()	{ m_safeQueue->Clear(); }
	static bool Empty() { return m_safeQueue->Empty(); } 
	static size_t GetMaxItems() { return m_safeQueue->GetMaxItems(); }
};

#endif // __SAFE_QUEUE_H_INCLUDED__