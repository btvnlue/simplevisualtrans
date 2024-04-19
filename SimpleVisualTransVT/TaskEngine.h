#pragma once

#include <Windows.h>
#include <list>

class TaskEngine;

class Lock
{
	HANDLE hLock;
public:
	Lock();
	virtual ~Lock();
	bool lock();
	bool unlock();
};

class TaskItem
{
public:
	virtual int Process(TaskEngine* eng) = 0;
};

class TaskEngine
{
	static unsigned long __stdcall ThProcTaskQueue(void* lpParam);
	int ProcTaskQueue();
	Lock lockQueue;
	std::list<TaskItem*> queue;
	bool keeprun;
	HANDLE hThProc;
public:
	TaskEngine();
	virtual ~TaskEngine();
	int Start();
	int Stop();
	int PutTask(TaskItem* task);
};

