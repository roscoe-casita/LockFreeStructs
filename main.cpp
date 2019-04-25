/*
 * main.cpp
 *
 *  Created on: Jan 1, 2019
 *      Author: rcasita
 */
//#include "pch.h"
#include <stdlib.h>
#include <iostream>
#include "Metrics.hpp"
#include "MultiCache.hpp"
#include "MPMCTester.hpp"

using std::cout;
using std::endl;
using std::dec;
using std::hex;



int main(int argc, char *argv[])
{
	RegisterTimer();
	for (int i = 0; i < argc; i++)
	{
		cout << argv[i] << "\t";
	}
	cout << endl;

	if (argc != 5)
	{
		cout << "Usages: MPMC <QueueName> <Writers> <Readers> <Messages>" << endl;
		cout << "QueueNames: LQ, LFBB, RB, BQ, HQ, BH, MC, SN, OC " << endl;
		cout << "LQ: Locking Queue. " << endl;
		cout << "SPSC: Lock Free Bounded Buffer - Single Producer (1) Single Consumer (2). " << endl;
		cout << "LFBB: Lock Free Bounded Buffer Queue. " << endl;
		cout << "RB: Ring Buffer of LFBB Queue. " << endl;
		cout << "BQ: Blocking LFBB Queue. " << endl;
		cout << "HQ: Hydra Queue (Buffer for each producer)" << endl;
		cout << "BH: Blocking Hydra Queue." << endl;
		cout << "MC: Moody Camel Lock Free Queue" << endl;
//		cout << "SN: Standard new/delete (cross thread )" << endl;
//		cout << "OC: ObjectCache Malloc/Free (cross thread)" << endl;
		return 0;
	}
	std::string queue_name = argv[1];
	int writers = atoi(argv[2]);
	int readers = atoi(argv[3]);
	int messages = atoi(argv[4]);

	StartTimer("Total Time");
	RunTests(queue_name,writers, readers, messages);
	StopTimer("Total Time");
	DisplayTimes();
	//system("pause");
}
