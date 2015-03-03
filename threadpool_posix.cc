/*
 *
 *    Boss/worker Model with a C++ Thread pool based on POSIX thread
 *
 *    Written by EuiJong Hwang
 *
 */

#include <pthread.h>
#include <iostream>
#include <cstdlib>
#include <csignal>
#include <string>
#include <vector>
#include <list>

#define MAX_QUEUE_SIZE	20
#define MAX_POOL_SIZE	10

class TaskQueue
{
	public:
		TaskQueue(int queue_size);
		~TaskQueue();
		void PopTask();
		static void* StartThread(void* object); // use in thread
		void GetPacketData(); // use in StartThread function
		void ListeningTaskQueue();
		void Lock(std::string msg);
		void Unlock(std::string msg);
		void Wait(std::string msg);
		void Signal(std::string msg);
	private:
		std::list<int> task_queue;
		pthread_mutex_t mutex;
		pthread_cond_t cond;
};

class ThreadPool
{
	public:
		ThreadPool(int pool_size);
		~ThreadPool();
		void Create(TaskQueue* taskQueue);
		void Join();
	private:
		std::vector<pthread_t*> worker_threads;
};

bool sig_int = false; // SIGINT FLAG

/* SIGINT Handler */
void SignalHandler(int sig_value) {
	sig_int = true;
}

/*********************************************************************
 *                                                                   *
 *                       Task Queue Functions                        *
 *                                                                   *
 *********************************************************************/

/* Task Queue Constructor */
TaskQueue::TaskQueue(int queue_size) {

	task_queue.clear();
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond, NULL);

	for (int i = 0; i < queue_size; i++) {
		task_queue.push_back(i);
	}
}

/* Task Queue Destructor */
TaskQueue::~TaskQueue() {
	
	task_queue.clear();
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
}

/* Pop Task */
void TaskQueue::PopTask() {

	std::cout << "task_queue.front(): " << task_queue.front() << std::endl;
	task_queue.pop_front();
}

/* static Start Thread */
void* TaskQueue::StartThread(void* object) {

	TaskQueue* taskQueue = (TaskQueue*)object;
	taskQueue->GetPacketData();
}

/* Get Packet Data */
void TaskQueue::GetPacketData() {

	std::string msg = "Worker";

	while (true) {

		Lock(msg);
		Wait(msg);

		/* if the SIGINT event occurs, all worker threads exit the loop */
		if (sig_int) {
			Unlock(msg);
			break;
		}

		PopTask(); // Critical Section

		Unlock(msg);
	}
}

/* Listening Task Queue */
void TaskQueue::ListeningTaskQueue() {
	
	std::string msg = "Boss";

	while (true) {

		Lock(msg);

		/* if the SIGINT event occurs, the Boss wake up all threads */
		if (sig_int) {
			std::cout << msg << ": Listening Queue is stopped..." << std::endl;
			pthread_cond_broadcast(&cond);

			Unlock(msg);
			break;
		}

		if (task_queue.empty()) {
			std::cout << "Task Queue is Empty..." << std::endl;
			sleep(1);

			Unlock(msg);
			continue;
		}

		Signal(msg); // task queue is not empty

		Unlock(msg);
		sleep(1);
	}
}

/* Lock */
void TaskQueue::Lock(std::string msg) {
	
	int is_lock = pthread_mutex_lock(&mutex);
	
	if (is_lock != 0) {
		std::cout << msg << ": Lock Failed" << std::endl;
		return;
	}
	std::cout << msg << ": Lock Success" << std::endl;
}

/* Unlock */
void TaskQueue::Unlock(std::string msg) {

	int is_unlock = pthread_mutex_unlock(&mutex);
	
	if (is_unlock != 0) {
		std::cout << msg << ": Unlock Failed\n" << std::endl;
		return;
	}
	std::cout << msg << ": Unlock Success\n" << std::endl;
}

/* Wait */
void TaskQueue::Wait(std::string msg) {

	std::cout << msg << ": Wait For Signal...\n" << std::endl;
	int is_wait = pthread_cond_wait(&cond, &mutex);

	if (is_wait != 0) {
		std::cout << msg << ": Wait Failed" << std::endl;
		return;
	}
	std::cout << msg << ": Receive Signal & Lock Success" << std::endl;
}

/* Signal */
void TaskQueue::Signal(std::string msg) {
	
	int is_signal = pthread_cond_signal(&cond);

	if (is_signal != 0) {
		std::cout << msg << ": Signal Failed" << std::endl;
		return;
	}
	std::cout << msg << ": Send Signal" << std::endl;
}

/*********************************************************************
 *                                                                   *
 *                      Thread Pool Functions                        *
 *                                                                   *
 *********************************************************************/

/* Thread Pool Constructor */
ThreadPool::ThreadPool(int pool_size) {
	
	worker_threads.clear();
	
	for (int i = 0; i < pool_size; i++) {
		pthread_t* worker = (pthread_t*)malloc(sizeof(pthread_t));
		worker_threads.push_back(worker);
	}
}

/* Thread Pool Destructor */
ThreadPool::~ThreadPool() {

	for (int i = 0; i < worker_threads.size(); i++) {
		free(worker_threads[i]);
	}
	worker_threads.clear();
}

/* Create Thread Pool */
void ThreadPool::Create(TaskQueue* taskQueue) {

	for (int i = 0; i < worker_threads.size(); i++) {
		if (pthread_create(worker_threads[i], NULL, TaskQueue::StartThread, (void*)taskQueue) != 0) {
			std::cout << "pthread_create error..." << std::endl;
			exit(0);
		}
		sleep(1);
	}
}

/* Join Thread */
void ThreadPool::Join() {
	
	for (int i = 0; i < worker_threads.size(); i++) {
		if (pthread_join(*worker_threads[i], NULL) != 0) {
			std::cout << "pthread_join error..." << std::endl;
			exit(0);
		}
	}
}

/* Check Arguments */
int CheckArgs(int args, char** argv) {

	if (args == 3) {

		int queue_size = atoi(argv[1]);
		int pool_size = atoi(argv[2]);

		if ((queue_size > 0 && queue_size <= MAX_QUEUE_SIZE) && (pool_size > 0 && pool_size <= MAX_POOL_SIZE)) {
			return 0;
		}
	}
	std::cout << "\nusage: ./pool [Queue Size] [Thread Count] // MAX_QUEUE_SIZE == 20, MAX_POOL_SIZE == 10\n" << std::endl;
	return -1;
}

int main(int args, char** argv) {

	if (CheckArgs(args, argv) != 0) {
		return -1;
	}

	TaskQueue* taskQueue = new TaskQueue(atoi(argv[1]));
	ThreadPool* threadPool = new ThreadPool(atoi(argv[2]));

	threadPool->Create(taskQueue);

	signal(SIGINT, SignalHandler);
	taskQueue->ListeningTaskQueue();

	threadPool->Join();

	delete taskQueue;
	delete threadPool;

	return 0;
}
