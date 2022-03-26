#include "ThreadPool.h"

template <typename Task> cThreadWorker<Task>::cThreadWorker(AString && a_ThreadName, std::shared_ptr<Pool> a_Pool) :
	Super(this, a_ThreadName), m_Pool(a_Pool)
{}

template <typename Task> void cThreadWorker<Task>::Execute()
{
	Task *task;

	while (m_Pool->Retrieve(task))
	{
		Process(task);
	}
}

template <typename T, typename Task> cThreadPool<T, Task>::cThreadPool(int numThreads)
{
	for (int i = 0; i < numThreads; i++)
	{
		m_Workers.push_back(std::shared_ptr<T>());
	}
}

template <typename T, typename Task> cThreadPool<T, Task>::~cThreadPool<T, Task>()
{
	Stop();
}

template <typename T, typename Task> void cThreadPool<T, Task>::Start()
{
	if (!m_running)
	{
		std::for_each(m_Workers.begin(), m_Workers.end(), T::Start);
		m_running = true;
	}
}

template <typename T, typename Task> void cThreadPool<T, Task>::Stop()
{
	if (m_running)
	{
		// Stop each worker
		std::for_each(m_Workers.begin(), m_Workers.end(), T::Stop);
		m_running = false;
		// Notify all waiting threads
		m_Event.SetAll();
	}
}

template <typename T, typename Task> std::list<std::shared_ptr<T>> cThreadPool<T, Task>::GetWorkers()
{
	return m_Workers;
}

template <typename T, typename Task> void cThreadPool<T, Task>::Submit(Task a_Task)
{
	// Add the task to the queue
	m_Work.EnqueueItem(a_Task);
	// Wake up one worker
	m_Event.Set();
}

template <typename T, typename Task> bool cThreadPool<T, Task>::Retrieve(Task & a_Task)
{
	// Try to dequeue an item.
	while (!m_Work.TryDequeueItem(a_Task))
	{
		// There is not an item to dequeue.
		// Wait for something to happen.
		m_Event.Wait();

		// Check that we're still running.
		if (!m_running)
		{
			// Stop() was called, and we have no item.
			return false;
		}
	}

	// We got an item.
	return true;
}

template <typename T, typename Task> size_t cThreadPool<T, Task>::GetQueueLength()
{
	return m_Work.Size();
}