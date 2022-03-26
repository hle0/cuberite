#pragma once

#include <list>
#include "OSSupport/Queue.h"
#include "OSSupport/IsThread.h"

template <typename T, typename Task> class cThreadPool;

/** A worker thread that processes Tasks. Part of a cThreadPool. */
template <typename Task, typename Pool> class cThreadWorker :
	public cIsThread
{
public:
	using Super = cIsThread;

	cThreadWorker(AString && a_ThreadName, Pool * a_Pool) : Super(std::move(a_ThreadName)), m_Pool(a_Pool) {};
	~cThreadWorker() { Stop(); };

	void Execute() override
	{
		Task task;

		while (m_Pool->Retrieve(task, &m_ShouldTerminate))
		{
			Process(task);
		}
	};

	/** Process a Task received from a queue. */
	virtual void Process(Task & a_Task) = 0;
protected:
	/** The pool this worker is part of. */
	Pool * m_Pool;
};

/** A class that oversees a pool of worker threads (most likely subclasses of cThreadWorker) */
template <typename T, typename Task> class cThreadPool
{
public:
	~cThreadPool() { Stop(); };

	std::list<std::shared_ptr<T>> GetWorkers()
	{
		return m_Workers;
	}

	void Start()
	{
		if (!m_running)
		{
			std::for_each(
				m_Workers.begin(),
				m_Workers.end(),
				[](auto worker)
				{
					worker->Start();
				}
			);
			m_running = true;
		}
	};
	void Stop()
	{
		if (m_running)
		{
			// Stop each worker
			std::for_each(
				m_Workers.begin(),
				m_Workers.end(),
				[](auto worker)
				{
					worker->Stop();
				}
			);
			m_running = false;
			// Notify all waiting threads
			m_Event.SetAll();
		}
	};

	/** Submit a Task to the Queue. */
	void Submit(Task a_Task)
	{
		// Add the task to the queue
		m_Work.EnqueueItem(a_Task);
		// Wake up one worker
		m_Event.Set();
	};
	/** Tries to retrieve a Task from the Queue. Returns true if a Task was retrieved; otherwise, false (Stop() was called). */
	bool Retrieve(Task & a_Task, std::atomic<bool> * a_shouldStop)
	{
		// Try to dequeue an item.
		while (!m_Work.TryDequeueItem(a_Task))
		{
			// There is not an item to dequeue.
			// Wait for something to happen.
			while (!m_Event.Wait(5))
			{
				// Make sure we don't have to stop.
				if (*a_shouldStop)
				{
					return false;
				}
			}

			// Check that we're still running.
			if (!m_running || *a_shouldStop)
			{
				// Stop() was called, and we probably have no item.
				return false;
			}
		}

		// We got an item.
		return true;
	};
	/** Get the approximate length of the Queue. */
	size_t GetQueueLength()
	{
		return m_Work.Size();
	};
protected:
	using Queue = cQueue<Task>;
	/** The threads working in this pool. */
	std::list<std::shared_ptr<T>> m_Workers;

	/** A queue of tasks to be used by threads. */
	Queue m_Work;

	/** True if the threads have been started and have not yet been stopped. */
	bool m_running;

	/** A cEvent that fires when Stop() is called or a new Queue item is added. */
	cEvent m_Event;
};
