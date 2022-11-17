#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_
#include <condition_variable>
#include <mutex>
#include <future>
#include <memory>
#include <type_traits>
#include <functional>
#include <queue>
#include <vector>
#include <thread>


#define holdMutex(mutex_var,name) std::unique_lock<std::mutex> name(mutex_var)
#define unholdMutex(name) name.unlock()
#define holdMutexWhile(mutex_var,name,condition_expr,condition_var) std::unique_lock<std::mutex> name(mutex_var); \
									while (!(condition_expr)) condition_var.wait(name)

template<int poolSize>
class ThreadPool
{
private:
	
	using Signal = std::condition_variable;
	using automutex = std::unique_lock<std::mutex>;
	using Task = std::function<void()>;
	using TaskQueue = std::queue<Task>;
	using threadArr = std::vector<std::thread>;

	int taskRemain;
	TaskQueue tasks;
	Signal signal;
	std::mutex lock;
	threadArr hThreads;
	bool destroying;

	inline void threadWrapper(int id)
	{
		while(1)
		{
			holdMutexWhile(lock,lck,!tasks.empty() || destroying ,signal);
			if (destroying == true)
			{
				unholdMutex(lck);
				return;
			}
			Task task = tasks.front();
			tasks.pop();
			unholdMutex(lck);
			task();
			taskRemain--;
		
		}
	}


public:
	ThreadPool()
	{
		taskRemain = 0;
		destroying = false;
		for (int i = 0; i < poolSize; i++)
		{
			hThreads.emplace_back(std::thread(&ThreadPool::threadWrapper, this,i));
		}
	}

	~ThreadPool()
	{
		destroying = true;
		signal.notify_all();
		for (std::thread &i : hThreads)
		{
			if (i.joinable())
				i.join();
		}
	}
	template<typename Func, typename...Args>
	inline auto submitTask(Func&& func, Args&&...args)->std::future<decltype(func(args...))>
	{
		using ReturnType = decltype(func(args...));
		std::function<ReturnType()> funcWithArgs = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
		auto task = std::make_shared<std::packaged_task<ReturnType()>>(funcWithArgs);
		std::future<ReturnType> future = task->get_future();
		tasks.emplace([task]()mutable { (*task)(); });
		taskRemain++;
		signal.notify_one();
		return future;
	}

	inline int getUndoneTaskCount()
	{
		return taskRemain;
	}
};

#endif