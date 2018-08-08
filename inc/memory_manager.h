#ifndef __MEMORY_MANAGER_H_INCLUDED__
#define __MEMORY_MANAGER_H_INCLUDED__

#pragma warning(disable:4786)

#include <vector>
#include <queue>
#include <set>
#include "inc/sync_simple.h"

using namespace std;

template<class T>
class mpriority_queue : public priority_queue<T, vector<T>, less<typename vector<T>::value_type> > 
{
public:
	void reserve(size_type _N) { c.reserve(_N); };
	size_type capacity() const { return c.capacity(); };
};

template <class Type>
class QueuedBlocks
{
private:
	QMutex m_qMutex;
	vector<Type *> m_allBlocks;
	set<Type *> m_quBlocks;

public:
	QueuedBlocks(int nInitSize = 1): m_qMutex(), m_allBlocks(), m_quBlocks()
	{
		int nSize;

		nSize = (nInitSize <= 0) ? 1 : nInitSize;

		for(int i = 0; i < nSize; i++)	// pre-allocate �޸� �Ҵ�
		{
			Type *t = new Type();		// alloc memory block
			if(t)
			{
				t->Clear();
				m_quBlocks.insert(t);
				m_allBlocks.push_back(t);
			}
		}
	}
	
	// Queue���� ������� �ʴ� �޸� �ҷ��� �ϳ� �����Ѵ�. ���� �޸𸮺� ������ NULL�� �����Ѵ�.
	// �޸� �� ���� �� queue���� �޸� ���� �����Ѵ�.
	Type *GetFromQueue() 
	{
		Type *t = NULL;
		
		m_qMutex.Lock();
		if(!m_quBlocks.empty())
		{
			set<Type *>::iterator itPos = m_quBlocks.begin();
			if(*itPos)	
			{
				t = *itPos;
				m_quBlocks.erase(t);
			}
		}
		m_qMutex.Unlock();

		return t;
	}

	// Queue���� ������� �ʴ� �޸� ���� �ϳ� �����Ѵ�. ���� �޸� �ҷ��� ������ �ϳ� �Ҵ� �� �����Ѵ�.
	Type *Get()
	{
		Type *t = NULL;

		t = GetFromQueue();
		if(t == NULL)
		{
			t = new Type();
			if(t)
			{
				t->Clear();
				m_qMutex.Lock();
				m_allBlocks.push_back(t);
				m_qMutex.Unlock();
			}
		}

		return t;
	}

	// ���Ϸ�� �޸� ���� ��ȯ�Ѵ�.
	void Release(Type *t)
	{
		if(t)
		{
			t->Clear();
			m_qMutex.Lock();
			m_quBlocks.insert(t);
			m_qMutex.Unlock();
		}
	}

	// �Ҵ�� �޸𸮺� ��θ� �����Ѵ�.
	vector<Type *> *GetBlocks() { return &m_allBlocks; }

	~QueuedBlocks() 
	{
		m_qMutex.Lock();
		
		m_quBlocks.clear();

		vector<Type *>::iterator itPos = m_allBlocks.begin();
		for(; itPos < m_allBlocks.end(); itPos++)
		{
			Type *t = *itPos;
			t->Clear();
			delete t;
		}
		
		m_allBlocks.clear();
		m_qMutex.Unlock();
	}
};

template <class Type>
class StaticBlocks
{
private:
	static QueuedBlocks<Type> *blocks;

public:
	static void Init(int nInitSize = 1)
	{
		if(blocks == NULL)	blocks = new QueuedBlocks<Type>(nInitSize);
	}

	static Type *Get() { return blocks->Get(); }

	static void Release(Type *t) 
	{ 
		if(blocks != NULL)	blocks->Release(t); 
	}
};

#endif // __MEMORY_MANAGER_H_INCLUDED__