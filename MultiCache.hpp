#pragma once
/*
 * MultiCache.hpp
 *
 *  Created on: Dec 13, 2018
 *      Author: rcasita
		LockFreeBoundedBufferSPSC	Single Block BB. Maintains strict coherence.Used Internally for Hydra Queue.
		MultiBoundedBufferSPSC		Growable Buffer Queue. Maintains strict coherence. Used Internally for 
									Hydra Queue.
		LockFreeBoundedBufferMPMC	Single Block BB. Maintains strict coherence.
		BlockingQueueMPMC			Uses LFBB. Maintains strict coherence, use this if you have to use a LFBB.
		RingBufferQueueMPMC			Growable Ring of Buffers Queue, Uses LFBB. Maintains strict coherence. 
		ContentionLessHydraQueueMPMCCircular Queue of Queues, Uses RB. One for each producer thread, 
									must register thread, Less means "Less" contention , 
									not "contentionless". Maintains coherence. (not strict).
		HydraQueueMPMC				HashMap of MBB Queue, one for each thread, must register threads. 
									Maintains coherence. (not strict). 
		BlockingHydreaQueueMPMC		"Genenerally" the fastest queue under heavy load. Use this one if you can. 
									Maintains coherence. (not strict).

		ObjectCache					Bulk/Block Memory allocator that maintains all freed items in a free queue,
									the free queue is tried first otherwise a new block is allocated and added.
									Must Register Thread.

		
 */
 // Simplified BSD license:
 // Copyright (c) 2013-2015, Cameron Desrochers.
 // All rights reserved.
 //
 // Redistribution and use in source and binary forms, with or without modification,
 // are permitted provided that the following conditions are met:
 //
 // - Redistributions of source code must retain the above copyright notice, this list of
 // conditions and the following disclaimer.
 // - Redistributions in binary form must reproduce the above copyright notice, this list of
 // conditions and the following disclaimer in the documentation and/or other materials
 // provided with the distribution.
 //
 // THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 // EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 // MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 // THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 // SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 // OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 // HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 // TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 // EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef MULTICACHE_HPP_
#define MULTICACHE_HPP_

#include <thread>
#include <map>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <chrono>


#define MB (1024*1024) 
#define BYTE_PER_PTR 8 // 64 bit required, otherwise limited to 4B~ transactions.
#define BUFFER_SIZE 1*MB / BYTE_PER_PTR // MUST BE A POWER OF 2.


template<class T>
class LockFreeBoundedBufferSPSC
{
public:
	LockFreeBoundedBufferSPSC()
	{
		Buffer = new T*[BUFFER_SIZE];
		In = 0;
		Out = 0;
		IndexMask = BUFFER_SIZE - 1;
	}

	~LockFreeBoundedBufferSPSC()
	{
		delete Buffer;
	}
	bool TryEnqueue(T *item)
	{
		int in = In.load(std::memory_order_acquire);
		int inn = (in + 1) & IndexMask;
		int out = Out.load(std::memory_order_acquire);
		if (inn == out)
		{
			return false;
		}
		Buffer[in] = item;
		In.store(inn, std::memory_order_release);
		return true;
	}

	T *TryDequeue()
	{
		T *returnValue = NULL;
		int out = Out.load(std::memory_order_acquire);
		int outn = (out + 1) & IndexMask;
		int in = In.load(std::memory_order_acquire);
		if (out != in)
		{
			returnValue = Buffer[out];
			Out.store(outn, std::memory_order_release);
		}
		return returnValue;
	}

	bool IsFull()
	{
		int inn = (In.load(std::memory_order_acquire) + 1) & IndexMask;
		int out = Out.load(std::memory_order_acquire);
		return (inn == out);
	}
	bool IsEmpty()
	{
		int out = Out.load(std::memory_order_acquire);
		int in = In.load(std::memory_order_acquire);
		return (in == out);
	}

private:
	T **Buffer;
	std::atomic<int> In;
	std::atomic<int> Out;
	int IndexMask;
};

template<class T>
class Node
{
public:
	Node()
	{
		Data = NULL;
		Next = NULL;
	}
	T *Data;
	std::atomic<Node<T> *> Next;
};
template<class T>
class MultiBoundedBufferSPSC
{
public:
	MultiBoundedBufferSPSC()
	{
		Node<LockFreeBoundedBufferSPSC<T> > *ptr = new Node<LockFreeBoundedBufferSPSC<T> >();
		ptr->Data = new LockFreeBoundedBufferSPSC<T>();
		ptr->Next = ptr;
		CurrentRead = ptr;
		CurrentWrite = ptr;
	}
	~MultiBoundedBufferSPSC()
	{
		Node<LockFreeBoundedBufferSPSC<T> > *ptr = CurrentWrite;
		do
		{
			Node<LockFreeBoundedBufferSPSC<T> > *next = ptr->Next;
			delete ptr->Data;
			delete ptr;
			ptr = next;
		} while (ptr != CurrentWrite);
	}
public:
	void Enqueue(T *item)
	{
		if (CurrentWrite->Data->IsFull())
		{
			if (CurrentWrite->Next.load()->Data->IsEmpty() && CurrentRead != CurrentWrite->Next)
			{
				CurrentWrite = CurrentWrite->Next.load();
			}
			else
			{
				Node<LockFreeBoundedBufferSPSC<T> > *ptr = new Node<LockFreeBoundedBufferSPSC<T> >();
				ptr->Data = new LockFreeBoundedBufferSPSC<T>();
				ptr->Next = CurrentWrite->Next.load();
				CurrentWrite->Next = ptr;
				CurrentWrite = ptr;
			}
		}
		CurrentWrite->Data->TryEnqueue(item);
	}

	bool TryEnqueue(T *item)
	{
		if (CurrentWrite->Data->IsFull())
		{
			if (CurrentWrite->Next.load()->Data->IsEmpty() && CurrentRead != CurrentWrite->Next)
			{
				CurrentWrite = CurrentWrite->Next.load();
			}
		}
		return CurrentWrite->Data->TryEnqueue(item);
	}

	T *TryDequeue()
	{

		if (CurrentRead->Data->IsEmpty())
		{
			if (CurrentRead != CurrentWrite)
			{
				CurrentRead = CurrentRead->Next;
			}
		}
		T *rv = CurrentRead->Data->TryDequeue();
		if (rv != NULL)
		{

		}
		return rv;
	}

private:
	Node<LockFreeBoundedBufferSPSC<T> > *CurrentRead;
	Node<LockFreeBoundedBufferSPSC<T> > *CurrentWrite;
};


template<class T>
class LockFreeBoundedBufferMPMC
{
public:
	
	LockFreeBoundedBufferMPMC() // size must be a power of 2.
	{
		BufferSize = BUFFER_SIZE;
		Buffer = new T*[BufferSize];
		for (uintptr_t i = 0; i < BufferSize; i++)
		{
			Buffer[i] = NULL;
		}
		IndexMask = BufferSize - 1;
		InFront = 0;
		In = 0;
		Out = 0;
		OutRear = 0;
		EnqueueOptimisticCount = 0;
		EnqueueOvercommit = 0;
		DequeueOptimisticCount = 0;
		DequeueOvercommit = 0;
	}

	~LockFreeBoundedBufferMPMC()
	{
		delete Buffer;
	}

	bool TryEnqueue(T *item)
	{
		bool returnValue = false;
		uintptr_t overcommit = EnqueueOvercommit.load(std::memory_order_acquire);
		uintptr_t enqueopcount = EnqueueOptimisticCount.load(std::memory_order_acquire);
		uintptr_t spec_count = enqueopcount - overcommit;
		uintptr_t out_rear = OutRear.load(std::memory_order_acquire);
		if (spec_count < out_rear + BufferSize)
		{
			uintptr_t Optimistic_cout = EnqueueOptimisticCount.fetch_add(1, std::memory_order_acq_rel);
			uintptr_t count = Optimistic_cout - overcommit;
			if (count < out_rear + BufferSize)
			{
				uintptr_t number = InFront.fetch_add(1, std::memory_order_acq_rel);
				uintptr_t index = number & IndexMask;
				Buffer[index] = item;
				returnValue = true;
			}
			else
			{
				EnqueueOvercommit++;
			}
		}
		bool done = false;
		while (!done)
		{
			uintptr_t in_front = InFront.load(std::memory_order_acquire);
			uintptr_t in = In.load(std::memory_order_acquire);
			if (in < in_front)
			{
				uintptr_t index = in & IndexMask;
				if (Buffer[index] != NULL)
				{
					if (In.compare_exchange_strong(in, in + 1, std::memory_order_acq_rel))
					{
					}
				}
			}
			else
			{
				done = true;
			}
		}
		return returnValue;
	}

	T *Dequeue()
	{
		T *returnValue = NULL;
		bool done = false;
		while (!done)
		{
			returnValue = TryDequeue();
			if (returnValue != NULL)
				done = true;
		}
		return returnValue;
	}
	T *TryDequeue()
	{
		T *returnValue = NULL;
		uintptr_t overcommit = DequeueOvercommit.load(std::memory_order_acquire);
		uintptr_t opt_count = DequeueOptimisticCount.load(std::memory_order_acquire);
		uintptr_t in = In.load(std::memory_order_acquire);

		if (opt_count - overcommit < in)
		{
			uintptr_t real_count = DequeueOptimisticCount.fetch_add(1, std::memory_order_acq_rel);

			if (real_count - overcommit < in)
			{
				uintptr_t out = Out.fetch_add(1, std::memory_order_acq_rel);
				uintptr_t index = out & IndexMask;
				returnValue = Buffer[index];
				Buffer[index] = NULL;
			}
			else
			{
				DequeueOvercommit++;
			}
		}
		bool done = false;
		while (!done)
		{
			uintptr_t out_rear = OutRear.load(std::memory_order_acquire);
			uintptr_t out = Out.load(std::memory_order_acquire);
			if (out_rear < out)
			{
				uintptr_t out_rear_index = out_rear & IndexMask;
				if (Buffer[out_rear_index] == NULL)
				{
					if (OutRear.compare_exchange_strong(out_rear, 
						out_rear + 1, std::memory_order_acq_rel))
					{
						// only advance once.
					}
				}
			}
			else
			{
				done = true;
			}
		}
		return returnValue;
	}
	bool IsFull()
	{
		uintptr_t in = InFront.load(std::memory_order_acquire);
		uintptr_t out = OutRear.load(std::memory_order_acquire);
		if (out + BufferSize == in)
		{
			return true;
		}
		return false;
	}
	bool IsEmpty()
	{
		uintptr_t in = InFront.load(std::memory_order_acquire);
		uintptr_t out = OutRear.load(std::memory_order_acquire);
		if (in == out)
		{
			return true;
		}
		return false;
	}

private:
	T **Buffer;
	std::atomic<uintptr_t> InFront;
	std::atomic<uintptr_t> In;
	std::atomic<uintptr_t> Out;
	std::atomic<uintptr_t> OutRear;
	std::atomic<uintptr_t> EnqueueOptimisticCount;
	std::atomic<uintptr_t> EnqueueOvercommit;
	std::atomic<uintptr_t> DequeueOptimisticCount;
	std::atomic<uintptr_t> DequeueOvercommit;
	uintptr_t BufferSize;
	uintptr_t IndexMask;
};


static std::chrono::milliseconds BQ_Timeout(1);

template<class T>
class BlockingQueueMPMC
{
public:
	BlockingQueueMPMC()
	{
		Exit = false;
		EstimatedCount = 0;
	}
	void Enqueue(T *item)
	{
		bool done = false;
		while (!done)
		{
			if (EstimatedCount.load() >= BUFFER_SIZE)
			{
				std::unique_lock<std::mutex> Lock(NotFullMutex);
				NotFullSignal.wait_for(Lock, BQ_Timeout);
				Lock.unlock();
			}
			else
			{
				done = TryEnqueue(item);
			}
			if (Exit)
			{
				break;
			}
		}
	}
	bool TryEnqueue(T *item)
	{
		if (Buffer.TryEnqueue(item))
		{
			int count = EstimatedCount.fetch_add(1, std::memory_order_acq_rel);
			if (count <= 0)
			{
				NotEmptySignal.notify_one();
			}
			return true;
		}
		return false;
	}
	T *Dequeue()
	{
		T *rv = NULL;
		bool done = false;
		while (!done)
		{
			if (EstimatedCount.load() <= 0)
			{
				std::unique_lock<std::mutex> Lock(NotEmptyMutex);
				NotEmptySignal.wait_for(Lock, BQ_Timeout);
				Lock.unlock();
			}
			else
			{
				rv = TryDequeue();
				if (rv != NULL)
				{
					done = true;
				}
			}
			if (Exit)
			{
				break;
			}
		}
		return rv;
	}
	T *TryDequeue()
	{
		T* rv = Buffer.TryDequeue();
		if (rv != NULL)
		{
			int count = EstimatedCount.fetch_sub(1, std::memory_order_acq_rel);
			if (count >= BUFFER_SIZE)
			{
				NotFullSignal.notify_one();
			}
		}
		return rv;
	}
	void Shutdown()
	{
		Exit = true;
		NotEmptySignal.notify_all();
		NotFullSignal.notify_all();
	}
private:
	std::mutex NotFullMutex;
	std::condition_variable NotFullSignal;
	std::mutex NotEmptyMutex;
	std::condition_variable NotEmptySignal;
	bool Exit;
	std::atomic<int> EstimatedCount;
	LockFreeBoundedBufferMPMC<T> Buffer;
};



template<class T>
struct RingBufferNode
{
	RingBufferNode<T> * Next;
	LockFreeBoundedBufferMPMC<T> *Queue;
};


template<class T>
class RingBufferQueueMPMC
{
public:
	RingBufferQueueMPMC()
	{
		RingBufferNode<T> *ptr = new RingBufferNode<T>();
		ptr->Queue = new LockFreeBoundedBufferMPMC<T>();
		ptr->Next = ptr;
		WriteCircle = ptr;
		ReadCircle = ptr;
	}
	~RingBufferQueueMPMC()
	{
		RingBufferNode<T> *ptr = WriteCircle, *start = WriteCircle;
		do
		{
			RingBufferNode<T> *next = ptr->Next;
			delete ptr->Queue;
			delete ptr;
			ptr = next;
		} while (ptr != start);
	}
public:
	bool TryEnqueue(T *item)
	{
		RingBufferNode<T> *write = WriteCircle;
		return write->Queue->TryEnqueue(item);
	}

	void Enqueue(T *item)
	{
		bool done = false;
		while (!done)
		{
			RingBufferNode<T> *write = WriteCircle;
			if (write->Queue->IsFull())
			{
				RingBufferNode<T> *next = write->Next;
				if (next == ReadCircle)
				{
					RingBufferNode<T> *add = new RingBufferNode<T>();
					add->Queue = new LockFreeBoundedBufferMPMC<T>();
					BufferLock.lock();
					write = WriteCircle;
					if (write->Queue->IsFull() && write->Next == ReadCircle)
					{
						add->Next = write->Next;
						write->Next = add;
						add = NULL;
						WriteCircle = WriteCircle->Next;
					}
					BufferLock.unlock();
					if (add != NULL)
					{
						delete add->Queue;
						delete add;
					}
				}
				else
				{
					BufferLock.lock();
					write = WriteCircle;
					if (write->Queue->IsFull() && write->Next != ReadCircle)
					{
						WriteCircle = WriteCircle->Next;
					}
					BufferLock.unlock();
				}
			}
			else
			{
				if (write->Queue->TryEnqueue(item))
				{
					done = true;
				}
			}
		}
	}
	T *Dequeue()
	{
		T * rv = NULL;
		bool done = false;
		while (!done)
		{
			rv = TryDequeue();
			if (rv != NULL)
			{
				done = true;
			}
		}
		return rv;
	}
	T *TryDequeue()
	{
		T *rv = NULL;
		RingBufferNode<T> *read = ReadCircle;
		if (read->Queue->IsEmpty())
		{
			if (read != WriteCircle)
			{
				BufferLock.lock();
				read = ReadCircle;
				if (read->Queue->IsEmpty() && read != WriteCircle)
				{
					ReadCircle = read->Next;
				}
				BufferLock.unlock();
			}
		}
		else
		{
			rv = read->Queue->TryDequeue();
		}
		return rv;
	}
	RingBufferNode<T> *WriteCircle;
	RingBufferNode<T> *ReadCircle;
	std::mutex BufferLock;
};

template<class T>
struct ProducerCircleQueueNode
{
	ProducerCircleQueueNode<T> *Next;
	RingBufferQueueMPMC<T> Queue;
	std::thread::id TID;
};


template<class T>
struct ProducerQueueNode
{
	ProducerQueueNode<T> *Next;
	MultiBoundedBufferSPSC<T> Queue;
	std::mutex Lock;
};

template<class T>
class HydraQueueMPMC
{
public:
	HydraQueueMPMC()
	{
		CircleQueue = NULL;
	}
	~HydraQueueMPMC()
	{
		ProducerQueueNode<T> *ptr = CircleQueue;
		do
		{
			if (ptr != NULL)
			{
				ProducerQueueNode<T> *next = ptr->Next;
				delete ptr;
				ptr = next;
			}
		} while (ptr != CircleQueue);
	}
public:
	void Enqueue(T *item)
	{
		ProducerQueueNode<T> *pq = LookupProducer(std::this_thread::get_id());
		pq->Queue.Enqueue(item);
	}
	bool TryEnqueue(T *item)
	{
		ProducerQueueNode<T> *pq = LookupProducer(std::this_thread::get_id());

		return pq->Queue.TryEnqueue(item);
	}

	T * TryDequeue()
	{
		if (CircleQueue == NULL)
			return NULL;
		T *rv = NULL;
		ProducerQueueNode<T> *pq = LookupConsumer(std::this_thread::get_id());
		int EmptyQueueCount = 0;
		while (EmptyQueueCount < QueueCount)
		{
			if (pq->Lock.try_lock())
			{
				rv = pq->Queue.TryDequeue();
				pq->Lock.unlock();
				if (rv != NULL)
				{
					break;
				}
				else
				{
					EmptyQueueCount++;
				}
			}
			else
			{
				pq = pq->Next;
				SetMyConsumersProducer(std::this_thread::get_id(), pq);
				EmptyQueueCount = 0;
			}
		}
		return rv;
	}

private:

	void SetMyConsumersProducer(std::thread::id tid, ProducerQueueNode<T> *pq)
	{
		consumers[tid] = pq;
	}
public:
	void AddProducer()
	{
		std::thread::id tid = std::this_thread::get_id();
		ProducerQueueNode<T> *returnValue = new ProducerQueueNode<T>();
		lookup_lock.lock();
		producers[tid] = returnValue;
		if (CircleQueue == NULL)
		{
			returnValue->Next = returnValue;
			CircleQueue = returnValue;
		}
		else
		{
			returnValue->Next = CircleQueue->Next;
			CircleQueue->Next = returnValue;
		}
		QueueCount++;
		lookup_lock.unlock();
	}
	void AddConsumer()
	{
		bool done = false;
		while (!done)
		{
			if (CircleQueue != NULL)
			{
				std::thread::id tid = std::this_thread::get_id();
				lookup_lock.lock();
				consumers[tid] = CircleQueue;
				lookup_lock.unlock();
				done = true;
			}
		}
	}
private:
	ProducerQueueNode<T> *LookupProducer(std::thread::id tid) //,ProducerQueueNode<T> *ptr=NULL)
	{
		return producers[tid];
	}
	ProducerQueueNode<T> *LookupConsumer(std::thread::id tid) //,ProducerQueueNode<T> *ptr=NULL)
	{
		return consumers[tid];
	}
	ProducerQueueNode<T> *CircleQueue;
	std::atomic<int> QueueCount;
	std::mutex lookup_lock;
	map<std::thread::id, ProducerQueueNode<T> *> producers;
	map<std::thread::id, ProducerQueueNode<T> *> consumers;
};


template<class T>
class BlockingHydraQueueMPMC
{
public:
	BlockingHydraQueueMPMC()
	{
		Exit = false;
		EstimatedCount = 0;
	}
	void Enqueue(T *item)
	{
		Queue.Enqueue(item);
		int count = EstimatedCount.fetch_add(1, std::memory_order_acq_rel);
		if (count <= 0)
		{
			NotEmptySignal.notify_one();
		}
	}
	bool TryEnqueue(T *item)
	{
		if (Queue.TryEnqueue(item))
		{
			int count = EstimatedCount.fetch_add(1, std::memory_order_acq_rel);
			if (count <= 0)
			{
				NotEmptySignal.notify_one();
			}
			return true;
		}
		return false;
	}
	T *Dequeue()
	{
		T *rv = NULL;
		bool done = false;
		while (!done)
		{
			if (EstimatedCount.load() <= 0)
			{
				std::unique_lock<std::mutex> Lock(NotEmptyMutex);
				NotEmptySignal.wait_for(Lock, BQ_Timeout);
				Lock.unlock();
			}
			else
			{
				rv = TryDequeue();
				if (rv != NULL)
				{
					done = true;
				}
			}
			if (Exit)
			{
				break;
			}
		}
		return rv;
	}
	T *TryDequeue()
	{
		T* rv = Queue.TryDequeue();
		if (rv != NULL)
		{
			EstimatedCount.fetch_sub(1, std::memory_order_acq_rel);
		}
		return rv;
	}
	void AddProducer()
	{
		Queue.AddProducer();
	}
	void AddConsumer()
	{
		Queue.AddConsumer();
	}
	void Shutdown()
	{
		Exit = true;
		NotEmptySignal.notify_all();
	}
private:
	std::mutex NotEmptyMutex;
	std::condition_variable NotEmptySignal;
	bool Exit;
	std::atomic<int> EstimatedCount;
	HydraQueueMPMC<T> Queue;
};

#define OBJECT_GROW_SIZE 1024

template<class T>
class ObjectCache
{
public:
	ObjectCache()
	{
		BlockList = NULL;
	}
	~ObjectCache()
	{
		Node<T> *ptr = BlockList;
		while (ptr != NULL)
		{
			Node<T> *next = ptr->Next;
			delete ptr->Data;
			delete ptr;
			ptr = next;
		}
	}
	void RegisterThread()
	{
		Queue.AddProducer();
		Queue.AddConsumer();
	}
	T * Malloc()
	{
		T *rv = NULL;
		bool done = false;
		while (!done)
		{
			rv = Queue.TryDequeue();
			if (rv != NULL)
			{
				done = true;
			}
			else
			{
				AddSomeCache();
			}
		}
		return rv;
	}
	void Free(T *ptr)
	{
		Queue.Enqueue(ptr);
	}
private:
	void AddSomeCache()
	{
		Node<T> *ptr = new Node<T>();
		ptr->Data = new T[OBJECT_GROW_SIZE];
		bool done = false;
		while (!done)
		{
			Node<T> *curr = BlockList.load();
			ptr->Next.store(curr);
			if (BlockList.compare_exchange_strong(curr, ptr))
			{
				done = true;
			}
		}
		for (int i = 0; i < OBJECT_GROW_SIZE; i++)
		{
			ptr->Data = 0;
			Queue.Enqueue(&(ptr->Data[i]));
		}
	}
	std::atomic<Node<T> *>BlockList;
	HydraQueueMPMC<T> Queue;
};


template<class T>
class LockQueueNode
{

public:
	LockQueueNode()
	{
		Clear();
	}
	void Clear()
	{
		Next = NULL;
		Data = NULL;
	}
public:

	LockQueueNode<T> *Next;
	T *Data;
};

template<class T>
class LockingQueue
{

public:
	LockingQueue()
	{
		Tail = new LockQueueNode<T>();
		Head = Tail;
	}
	~LockingQueue()
	{
		delete Tail;
	}
	void Enqueue(T *item)
	{
		LockQueueNode<T> *add = new LockQueueNode<T>();
		TailLock.lock();
		Tail->Data = item;
		Tail->Next = add;
		Tail = add;
		TailLock.unlock();
	}

	T *TryDequeue()
	{
		T *returnValue = NULL;
		if (Head != Tail)
		{
			LockQueueNode<T> *node = NULL;
			HeadLock.lock();
			if (Head != Tail)
			{
				node = Head;
				Head = Head->Next;
			}
			HeadLock.unlock();
			if (node != NULL)
			{
				returnValue = node->Data;
				node->Clear();
				delete node;
			}
		}
		return returnValue;
	}
private:
	LockQueueNode<T> *Head;
	LockQueueNode<T> *Tail;
	std::mutex HeadLock;
	std::mutex TailLock;
};


#endif /* MULTICACHE_HPP_ */
