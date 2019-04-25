/*
 * Metrics.cpp
 *
 *  Created on: Dec 11, 2018
 *      Author: rcasita
 */
//#include "pch.h"

#include "Metrics.hpp"
#include <mutex>
#include <ctime>
#include <thread>

class Timer
{
public:
	void StartTimer(string name);
	void StopTimer(string name);
	map<string, std::clock_t> start_time;
	map<string, std::clock_t> time_used;
	vector<string> GetNames();
};

void Timer::StartTimer(string name)
{

	start_time[name] = std::clock();
}


void Timer::StopTimer(string name)
{
	clock_t temp = std::clock() - start_time[name];
	time_used[name] += temp;
}

vector<string> Timer::GetNames()
{
	vector<string> returnValue;
	for (map<string, std::clock_t>::iterator itr = start_time.begin(); itr != start_time.end(); itr++)
	{
		returnValue.push_back(itr->first);
	}
	return returnValue;
}

map<std::thread::id, Timer> Timers;


void StartTimer(string name)
{
	std::thread::id tid = std::this_thread::get_id();
	Timers[tid].StartTimer(name);
}

void StopTimer(string name)
{
	std::thread::id tid = std::this_thread::get_id();
	Timers[tid].StopTimer(name);
}


std::mutex RegisterLock;
void RegisterTimer()
{
	RegisterLock.lock();
	std::thread::id tid = std::this_thread::get_id();
	Timers[tid] = Timer();
	RegisterLock.unlock();
}


void DisplayTimes()
{
	map<string, std::clock_t> times;
	for (map<std::thread::id, Timer>::iterator titr = Timers.begin(); titr != Timers.end(); titr++)
	{
		vector<string> names = titr->second.GetNames();
		for (vector<string>::iterator itr = names.begin(); itr != names.end(); itr++)
		{
			times[*itr] += titr->second.time_used[*itr];
		}
	}
	double total_time = 0;
	for (map<string, std::clock_t>::iterator itr = times.begin(); itr != times.end(); itr++)
	{
		double duration = (((double)itr->second) / ((double)CLOCKS_PER_SEC));
		total_time += duration;
	}
	//std::cout << "Timer Metrics: " << std::endl;
	for (map<string, std::clock_t>::iterator itr = times.begin(); itr != times.end(); itr++)
	{
		double duration = (((double)itr->second) / ((double)CLOCKS_PER_SEC));
		//std::cout << itr->first << " took : " << duration << std::endl;
		std::cout << itr->first << " percent: " << ((duration / total_time) * 100) << "%" << std::endl;
	}
	std::cout << "Total Time: " << total_time << std::endl;
}



