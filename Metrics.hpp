#pragma once
#ifndef METRICS_HPP_TIMER
#define METRICS_HPP_TIMER
#include <time.h>
#include <map>
#include <string>
#include <vector>
#include <iostream>
using std::map;
using std::string;
using std::vector;

void RegisterTimer();
void StartTimer(string name);
void StopTimer(string name);
void DisplayTimes();

#endif
