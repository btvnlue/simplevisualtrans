#include "TaskEngine.h"
#include <Windows.h>

unsigned long __stdcall TaskEngine::ThProcTaskQueue(void* lpParam)
{
    TaskEngine* self = reinterpret_cast<TaskEngine*>(lpParam);

    self->ProcTaskQueue();
    return 0;
}

int TaskEngine::ProcTaskQueue()
{
    TaskItem* task;

    while (keeprun) {
        task = NULL;
        if (lockQueue.lock()) {
            if (queue.size() > 0) {
                task = *queue.begin();
                queue.pop_front();
            }
            lockQueue.unlock();
        }
        if (task) {
            task->Process(this);
            delete task;
        }
        else {
            Sleep(100);
        }
    }
    return 0;
}

TaskEngine::TaskEngine()
    :keeprun(false)
    , hThProc(NULL)
{
}

TaskEngine::~TaskEngine()
{
}

int TaskEngine::Start()
{
    DWORD thid;
    keeprun = true;
    hThProc = ::CreateThread(NULL, 0, ThProcTaskQueue, this, 0, &thid);
    return 0;
}

int TaskEngine::Stop()
{
    keeprun = false;
    if (hThProc) {
        ::WaitForSingleObject(hThProc, INFINITE);
        hThProc = NULL;
    }

    return 0;
}

int TaskEngine::PutTask(TaskItem* task)
{
    lockQueue.lock();
    queue.push_back(task);
    lockQueue.unlock();
    return 0;
}

Lock::Lock()
{
    hLock = ::CreateEvent(NULL, FALSE, TRUE, NULL);

}

Lock::~Lock()
{
    if (hLock) {
        ::CloseHandle(hLock);
        hLock = NULL;
    }
}

bool Lock::lock()
{
    bool dlk = false;
    if (::WaitForSingleObject(hLock, INFINITE) == WAIT_OBJECT_0) {
        dlk = true;
    }
    return dlk;
}

bool Lock::unlock()
{
    bool dul = SetEvent(hLock) == TRUE;
    return dul;
}
