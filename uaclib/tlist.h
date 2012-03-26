/*!
#
# Win-Widget. Windows related software for Audio-Widget/SDR-Widget (http://code.google.com/p/sdr-widget/)
# Copyright (C) 2012 Nikolay Kovbasa
#
# Permission to copy, use, modify, sell and distribute this software 
# is granted provided this copyright notice appears in all copies. 
# This software is provided "as is" without express or implied
# warranty, and with no claim as to its suitability for any purpose.
#
#----------------------------------------------------------------------------
# Contact: nikkov@gmail.com
#----------------------------------------------------------------------------
*/

#ifndef __TLIST_T__
#define __TLIST_T__


template <class element, class list> class TElement
{
protected:
	element*	m_prev;
	element*	m_next;
	list*		m_list;

public:
	TElement() : m_prev(NULL), m_next(NULL), m_list(NULL)
	{
	}
	virtual ~TElement() {}
	virtual void Destroy() = 0;

	friend typename list;
};


class DummyLocker
{
public:
	DummyLocker() {}
	void Init() {}
	void Lock() {}
	void Unlock() {}
};


template <class element, class lockobj = DummyLocker> class TList
{
private:
	element *	m_first;
	element *	m_last;
	long		m_numElements;
	lockobj		m_lock;

public:
	TList() : m_first(NULL), m_last(NULL), m_numElements(0)
	{
		m_lock.Init();
	}

	~TList()
	{
		Clear();
	}

	long Count()
	{ return m_numElements; }

	element* First()
	{ return m_first; }
	
	element* Last()
	{ return m_last; }

	element* Next(element* elem)
	{ 
		if(!IsContains(elem))
			return NULL;
		return elem->m_next; 
	}

	element* Prev(element* elem)
	{ 
		if(!IsContains(elem))
			return NULL;
		return elem->m_prev(); 
	}

	void Clear()
	{
		element* curElem = m_first;
		while(curElem)
		{
			element* nextElem = curElem->m_next;
			curElem->Destroy();
			curElem = nextElem;
		}
		m_first = m_last = NULL;
		m_numElements = 0;
	}

	void LockList()
	{
		m_lock.Lock();
	}

	void UnlockList()
	{
		m_lock.Unlock();
	}

	bool IsContains(element* elem)
	{
		element* curElem = m_first;
		while(curElem)
		{
			if(elem == curElem)
				return TRUE;
			curElem = curElem->m_next;
		}
		return FALSE;
	}

	bool IsEmpty()
	{
		return m_numElements == 0;
	}

	bool Add(element* elem)
	{
		if(IsContains(elem))
			return FALSE;
		elem->m_next = elem->m_prev = NULL;
		elem->m_list = this;
		if(m_last)
		{
			m_last->m_next = elem;
			elem->m_prev = m_last;
			m_last = elem;
		}
		else
		{
			m_first = m_last = elem;
		}
		m_numElements++;
		return TRUE;
	}

	bool Del(element* elem)
	{
		if(!IsContains(elem))
			return FALSE;
        
		if (elem->m_prev) { elem->m_prev->m_next = elem->m_next; }
        if (elem->m_next) 
			elem->m_next->m_prev = elem->m_prev;

        if (elem == m_first) 
			m_first = elem->m_next;
        
		if (elem == m_last) 
			m_last=elem->m_prev;

        elem->m_next = NULL;
        elem->m_prev = NULL;
        elem->m_list = NULL;
		m_numElements--;
		return TRUE;
	}
};



#endif //__TLIST_T__