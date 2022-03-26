#pragma once

#include <list>
#include "OSSupport/Queue.h"
#include "OSSupport/IsThread.h"

template <typename T, typename Task> class cThreadPool;

/** A worker thread that processes Tasks. Part of a cThreadPool. */
template <typename Task> class cThreadWorker :
	public cIsThread
{
  public:
	using Pool = cThreadPool<cThreadWorker<Task>, Task>;
	using Super = cIsThread;

	cThreadWorker(AString && a_ThreadName, std::shared_ptr<Pool> a_Pool);

	void Execute() override;

	/** Process a Task received from a queue. */
	virtual void Process(Task & a_Task) = 0;
  protected:
	/** The pool this worker is part of. */
	std::shared_ptr<Pool> m_Pool;
};

/** A class that oversees a pool of worker threads (most likely subclasses of cThreadWorker) */
template <typename T, typename Task> class cThreadPool
{
  public:
	cThreadPool(int numThreads);
	~cThreadPool();

	std::list<std::shared_ptr<T>> GetWorkers();

	void Start();
	void Stop();

	/** Submit a Task to the Queue. */
	void Submit(Task a_Task);
	/** Tries to retrieve a Task from the Queue. Returns true if a Task was retrieved; otherwise, false (Stop() was called). */
	bool Retrieve(Task & a_Task);
	/** Get the approximate length of the Queue. */
	size_t GetQueueLength();
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