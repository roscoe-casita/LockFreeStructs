/*
 * MPMCTester.hpp
 *
 *  Created on: Jan 1, 2019
 *      Author: rcasita
 */
//#include "pch.h"
#include "MPMCTester.hpp"
#include <iostream>
#include <vector>
#include "Metrics.hpp"
#include "MultiCache.hpp"
#include "concurrentqueue.hpp"




static int *NotAtomicMessages = NULL;

static int TotalMessages;
static std::atomic<int> EnqueuedMessages;
static std::atomic<int> DequeuedMessages;



LockingQueue<int> LQ__TestQueue;
LockFreeBoundedBufferSPSC<int> SPSC__TestQueue;
LockFreeBoundedBufferMPMC<int> LFBB__TestQueue;
HydraQueueMPMC<int> HQ__TestQueue;
RingBufferQueueMPMC<int> RB__TestQueue;
BlockingQueueMPMC<int> BQ__TestQueue;
BlockingHydraQueueMPMC<int> BH__TestQueue;
moodycamel::ConcurrentQueue<int*> MC__TestQueue;

ObjectCache<int> OC_Pool;

void LQ_ReaderProc()
{
	while (DequeuedMessages < TotalMessages)
	{
		int *ptr = LQ__TestQueue.TryDequeue();
		if (ptr != NULL)
		{
			(*ptr)++;
			DequeuedMessages++;
		}
	}
}

void SPSC_ReaderProc()
{
	while (DequeuedMessages < TotalMessages)
	{
		int *ptr = SPSC__TestQueue.TryDequeue();
		if (ptr != NULL)
		{
			(*ptr)++;
			DequeuedMessages++;
		}
	}
}

void LFBB_ReaderProc()
{
	while (DequeuedMessages < TotalMessages)
	{
		int *ptr = LFBB__TestQueue.TryDequeue();
		if (ptr != NULL)
		{
			(*ptr)++;
			DequeuedMessages++;
		}
	}
}
void HQ_ReaderProc()
{
	HQ__TestQueue.AddConsumer();
	while (DequeuedMessages < TotalMessages)
	{
		int *ptr = HQ__TestQueue.TryDequeue();
		if (ptr != NULL)
		{
			(*ptr)++;
			DequeuedMessages++;
		}
	}
}
void BH_ReaderProc()
{
	BH__TestQueue.AddConsumer();
	while (DequeuedMessages < TotalMessages)
	{
		int *ptr = BH__TestQueue.Dequeue();
		if (ptr != NULL)
		{
			(*ptr)++;
			DequeuedMessages++;
		}
	}
	BH__TestQueue.Shutdown();
}

void RB_ReaderProc()
{
	while (DequeuedMessages < TotalMessages)
	{
		int *ptr = RB__TestQueue.TryDequeue();
		if (ptr != NULL)
		{
			(*ptr)++;
			DequeuedMessages++;
		}
	}
}

void BQ_ReaderProc()
{
	while (DequeuedMessages < TotalMessages)
	{
		int *ptr = BQ__TestQueue.Dequeue();
		if (ptr != NULL)
		{
			(*ptr)++;
			DequeuedMessages++;
		}
	}
	BQ__TestQueue.Shutdown();
}

void SN_ReaderProc()
{
	RegisterTimer();
	HQ__TestQueue.AddConsumer();
	while (DequeuedMessages < TotalMessages)
	{
		int *ptr = HQ__TestQueue.TryDequeue();
		if (ptr != NULL)
		{
			int index = DequeuedMessages.fetch_add(1);
			if (index < TotalMessages)
			{
				NotAtomicMessages[index]++;
			}
			StartTimer("New/Delete");
			delete ptr;
			StopTimer("New/Delete");
		}
	}
}

void OC_ReaderProc()
{
	RegisterTimer();
	OC_Pool.RegisterThread();
	while (DequeuedMessages < TotalMessages)
	{
		int *ptr = HQ__TestQueue.TryDequeue();
		if (ptr != NULL)
		{
			int index = DequeuedMessages.fetch_add(1);
			if (index < TotalMessages)
			{
				NotAtomicMessages[index]++;
			}
			StartTimer("New/Delete");
			OC_Pool.Free(ptr);
			StopTimer("New/Delete");
		}
	}
	int **ary = new int *[TotalMessages];
	for (int i = 0; i < TotalMessages; i++)
	{
		ary[i] = OC_Pool.Malloc();
	}
	for (int i = 0; i < TotalMessages; i++)
	{
		OC_Pool.Free(ary[i]);
	}
	delete[] ary;
}

void MC_ReaderProc()
{
	while (DequeuedMessages < TotalMessages)
	{
		int *ptr = NULL;
		if (MC__TestQueue.try_dequeue(ptr))
		{
			(*ptr)++;
			DequeuedMessages++;
		}
	}
}

void LQ__WriterProc()
{
	while (EnqueuedMessages < TotalMessages)
	{
		int index = EnqueuedMessages.fetch_add(1, std::memory_order_acq_rel);
		if (index < TotalMessages)
		{
			int *ptr = &NotAtomicMessages[index];
			(*ptr)++;
			LQ__TestQueue.Enqueue(ptr);
		}
	}
}
void SPSC__WriterProc()
{
	while (EnqueuedMessages < TotalMessages)
	{
		int index = EnqueuedMessages.fetch_add(1, std::memory_order_acq_rel);
		if (index < TotalMessages)
		{
			int *ptr = &NotAtomicMessages[index];
			(*ptr)++;
			while (SPSC__TestQueue.TryEnqueue(ptr) == false)
			{
				continue;
			}
		}
	}
}
void LFBB__WriterProc()
{
	while (EnqueuedMessages < TotalMessages)
	{
		int index = EnqueuedMessages.fetch_add(1, std::memory_order_acq_rel);
		if (index < TotalMessages)
		{
			int *ptr = &NotAtomicMessages[index];
			(*ptr)++;
			while (LFBB__TestQueue.TryEnqueue(ptr) == false)
			{
				continue;
			}
		}
	}
}

void HQ__WriterProc()
{
	HQ__TestQueue.AddProducer();
	while (EnqueuedMessages < TotalMessages)
	{
		int index = EnqueuedMessages.fetch_add(1, std::memory_order_acq_rel);
		if (index < TotalMessages)
		{
			int *ptr = &NotAtomicMessages[index];
			(*ptr)++;
			if (HQ__TestQueue.TryEnqueue(ptr) == false)
			{
				HQ__TestQueue.Enqueue(ptr);
			}
		}
	}
}
void BH__WriterProc()
{
	BH__TestQueue.AddProducer();
	while (EnqueuedMessages < TotalMessages)
	{
		int index = EnqueuedMessages.fetch_add(1, std::memory_order_acq_rel);
		if (index < TotalMessages)
		{
			int *ptr = &NotAtomicMessages[index];
			(*ptr)++;
			if (BH__TestQueue.TryEnqueue(ptr) == false)
			{
				BH__TestQueue.Enqueue(ptr);
			}
		}
	}
}

void RB__WriterProc()
{
	while (EnqueuedMessages < TotalMessages)
	{
		int index = EnqueuedMessages.fetch_add(1, std::memory_order_acq_rel);
		if (index < TotalMessages)
		{
			int *ptr = &NotAtomicMessages[index];
			(*ptr)++;
			RB__TestQueue.Enqueue(ptr);
		}
	}
}
void SN__WriterProc()
{
	RegisterTimer();
	HQ__TestQueue.AddProducer();
	while (EnqueuedMessages < TotalMessages)
	{
		int index = EnqueuedMessages.fetch_add(1, std::memory_order_acq_rel);
		if (index < TotalMessages)
		{
			NotAtomicMessages[index]++;
			StartTimer("New/Delete");
			int *ptr = new int();
			StopTimer("New/Delete");
			HQ__TestQueue.Enqueue(ptr);
		}
	}
}

void OC__WriterProc()
{
	RegisterTimer();
	HQ__TestQueue.AddProducer();
	OC_Pool.RegisterThread();
	while (EnqueuedMessages < TotalMessages)
	{
		int index = EnqueuedMessages.fetch_add(1, std::memory_order_acq_rel);
		if (index < TotalMessages)
		{
			NotAtomicMessages[index]++;
			StartTimer("New/Delete");
			int *ptr = OC_Pool.Malloc();
			StopTimer("New/Delete");
			HQ__TestQueue.Enqueue(ptr);
		}
	}
}

void BQ__WriterProc()
{
	while (EnqueuedMessages < TotalMessages)
	{
		int index = EnqueuedMessages.fetch_add(1, std::memory_order_acq_rel);
		if (index < TotalMessages)
		{
			int *ptr = &NotAtomicMessages[index];
			(*ptr)++;
			BQ__TestQueue.Enqueue(ptr);
		}
	}
}
void MC__WriterProc()
{
	while (EnqueuedMessages < TotalMessages)
	{
		int index = EnqueuedMessages.fetch_add(1, std::memory_order_acq_rel);
		if (index < TotalMessages)
		{
			int *ptr = &NotAtomicMessages[index];
			(*ptr)++;
			MC__TestQueue.enqueue(ptr);
		}
	}
}


void RunTests(std::string queue_name,int writers, int readers, int messages)
{
	TotalMessages = messages;
	EnqueuedMessages = 0;
	DequeuedMessages = 0;
	NotAtomicMessages = new int[TotalMessages];
	for (int i = 0; i < TotalMessages; i++)
	{
		NotAtomicMessages[i] = 0;
	}

	std::vector<std::thread> threads;
	threads.resize(writers + readers);
	if (queue_name.compare("LFBB")==0)
	{
		for (int i = 0; i < writers; i++)
		{
			threads[i] = std::thread(LFBB__WriterProc);
		}
		for (int i = 0; i < readers; i++)
		{
			threads[i + writers] = std::thread(LFBB_ReaderProc);
		}
	}
	else if(queue_name.compare("SPSC") == 0)
	{
		for (int i = 0; i < writers; i++)
		{
			threads[i] = std::thread(SPSC__WriterProc);
		}
		for (int i = 0; i < readers; i++)
		{
			threads[i + writers] = std::thread(SPSC_ReaderProc);
		}
	}
	else if (queue_name.compare("LQ") == 0)
	{
		for (int i = 0; i < writers; i++)
		{
			threads[i] = std::thread(LQ__WriterProc);
		}
		for (int i = 0; i < readers; i++)
		{
			threads[i + writers] = std::thread(LQ_ReaderProc);
		}
	}
	else if (queue_name.compare("HQ")==0)
	{
		for (int i = 0; i < writers; i++)
		{
			threads[i] = std::thread(HQ__WriterProc);
		}
		for (int i = 0; i < readers; i++)
		{
			threads[i + writers] = std::thread(HQ_ReaderProc);
		}
	}
	else if (queue_name.compare("BH")==0)
	{
		for (int i = 0; i < writers; i++)
		{
			threads[i] = std::thread(BH__WriterProc);
		}
		for (int i = 0; i < readers; i++)
		{
			threads[i + writers] = std::thread(BH_ReaderProc);
		}
	}
	else if (queue_name.compare("RB")==0)
	{
		for (int i = 0; i < writers; i++)
		{
			threads[i] = std::thread(RB__WriterProc);
		}
		for (int i = 0; i < readers; i++)
		{
			threads[i + writers] = std::thread(RB_ReaderProc);
		}
	}
	else if (queue_name.compare("BQ")==0)
	{
		for (int i = 0; i < writers; i++)
		{
			threads[i] = std::thread(BQ__WriterProc);
		}
		for (int i = 0; i < readers; i++)
		{
			threads[i + writers] = std::thread(BQ_ReaderProc);
		}
	}
	/*else if (queue_name.compare("SN") == 0)
	{
		for (int i = 0; i < writers; i++)
		{
			threads[i] = std::thread(SN__WriterProc);
		}
		for (int i = 0; i < readers; i++)
		{
			threads[i + writers] = std::thread(SN_ReaderProc);
		}
	}
	else if (queue_name.compare("OC") == 0)
	{
		for (int i = 0; i < writers; i++)
		{
			threads[i] = std::thread(OC__WriterProc);
		}
		for (int i = 0; i < readers; i++)
		{
			threads[i + writers] = std::thread(OC_ReaderProc);
		}
	}*/
	else if (queue_name.compare("MC")==0)
	{
		for (int i = 0; i < writers; i++)
		{
			threads[i] = std::thread(MC__WriterProc);
		}
		for (int i = 0; i < readers; i++)
		{
			threads[i + writers] = std::thread(MC_ReaderProc);
		}
	}
	else
	{
		std::cout << "Invalid Queue : " << queue_name << std::endl;
		return;
	}
	for (int i = 0; i < readers + writers; i++)
	{
		threads[i].join();
	}

	for (int i = 0; i < TotalMessages; i++)
	{
		if (NotAtomicMessages[i] != 2)
		{
			std::cout << "Invalid Message either duplicate or missing: " << i << "\t " << NotAtomicMessages[i] << std::endl;
			throw "Invalid MPMC Test";
		}
	}
	delete[] NotAtomicMessages;
}












