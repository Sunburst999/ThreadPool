#pragma once
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<iostream>
#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<functional>
#include<condition_variable>
#include<unordered_map>
#include<thread>
#include<future>

const int TASK_MAX_THREADHOLD = 2;//INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 60; //单位秒


//线程池可支持的模式
enum class PoolMode
{
	MODE_FIXED,//固定数量的线程
	MODE_CACHED,//线程数量可动态增长
};

//线程类型
class Thread {
public:
	using ThreadFunc = std::function<void(int)>;
	//线程构造
	Thread(ThreadFunc func)
		:func_(func)
		, threadId_(generateId_++)
	{}
	//线程析构
	~Thread() = default;
	//启动线程
	void start()
	{
		//创建一个线程来执行一个线程函数
		std::thread t(func_, threadId_);
		t.detach();//设置分离线程 pthread_detach设置为分离线程
	}


	//获取线程ID
	int getId()const
	{
		return threadId_;
	}
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_; //线程ID
};

int Thread::generateId_ = 0;

//线程池类型
class ThreadPool {
public:
	//线程池构造
	ThreadPool()
		: initThreadSize_(0)
		, taskSize_(0)
		, idleThreadSize_(0)
		, curThreadSize_(0)
		, taskQueMaxThreadHold_(TASK_MAX_THREADHOLD)
		, threadSizeThreshHold_(200)
		, poolMode_(PoolMode::MODE_FIXED)
		, isPoolRunning_(false)
	{}
	//线程池析构
	~ThreadPool()
	{
		isPoolRunning_ = false;
		//等待线程池里面所有的线程返回 有两种状态：阻塞 & 正在执行任务中
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		notEmpty_.notify_all();
		exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
	}
	//设置线程池的工作模式
	void setMode(PoolMode mode)
	{
		if (checkRunningState())
			return;
		poolMode_ = mode;
	}

	//设置task任务队列上限阈值
	void setTaskQueThreadHold(int threadhold)
	{
		if (checkRunningState())
			return;
		taskQueMaxThreadHold_ = threadhold;
	}

	//设置线程池cached模式下线程阈值
	void setThreadSizeThreshHold(int threshhold)
	{
		if (checkRunningState())
			return;
		if (poolMode_ == PoolMode::MODE_CACHED)
		{
			threadSizeThreshHold_ = threshhold;
		}
	}

	//给线程池提交任务
	//使用可变参模板编程，让submitTask可以接受任意任务函数合任意数量的参数
	//pool.submitTask(sum1, 10, 20);
	template<typename Func, typename... Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
	{
		//打包任务，放入任务队列
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
			);
		std::future<RType> result = task->get_future();

		//获取锁 
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		//线程通信，等待任务队列有空余
		//任务提交任务，最长时间不能阻塞超过1S，否则判断提交任务失败，返回
		if (!notFull_.wait_for(lock, std::chrono::seconds(1),
			[&]()->bool {return taskQue_.size() < taskQueMaxThreadHold_; }))
		{
			//等待1S，依然没有满足
			std::cerr << "task queue is full,submit task fail!" << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]()->RType {return RType(); });
			(* task)();
			return task->get_future();
		}
		//如果有空余，把任务放进任务队列里
		//taskQue_.emplace(sp);
		taskQue_.emplace([task]() {(*task)(); });
		taskSize_++;
		//因为新放了任务，任务队列肯定不空了，在notEmpty_上进行通知,赶紧分配线程执行任务
		notEmpty_.notify_all();

		//cached 任务处理比较紧急 场景：小而快的任务 需要根据任务数量和空闲线程的数量，判断是否需要创建新的线程出来
		if (poolMode_ == PoolMode::MODE_CACHED
			&& taskSize_ > idleThreadSize_
			&& curThreadSize_ < threadSizeThreshHold_)
		{
			std::cout << "creat new thread" << std::endl;
			//创建新线程
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			//threads_.emplace_back(std::move(ptr));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			//启动线程
			threads_[threadId]->start();
			//修改线程个数相关的变量
			curThreadSize_++;
			idleThreadSize_++;
		}

		//返回任务的Result对象
		return result;
	}


	//开启线程池
	void start(int initThreadSize = std::thread::hardware_concurrency())
	{
		//设置线程池的运行状态
		isPoolRunning_ = true;

		//记录初始线程个数
		initThreadSize_ = initThreadSize;
		curThreadSize_ = initThreadSize;

		//创建thread线程对象的时候，把线程函数给到thread线程对象
		for (int i = 0; i < initThreadSize_; i++)
		{
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			//threads_.emplace_back(std::move(ptr));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
		}

		//启动所有线程 std::vector<Thread*> threads_;
		for (int i = 0; i < initThreadSize_; i++)
		{
			threads_[i]->start(); //需要去执行一个线程函数
			idleThreadSize_++;//记录初始空闲线程的数量
		}
	}

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	//定义线程函数
	void threadFunc(int threadid)
	{
		/*std::cout << "begin threadfunc id:" << std::this_thread::get_id() << std::endl;
	std::cout << "end threadfunc id:" << std::this_thread::get_id() << std::endl;*/
		auto lastTime = std::chrono::high_resolution_clock().now();

		//所有任务必须执行完成，线程池才可以回收所有线程资源
		for (;;)
		{
			Task task;
			{
				//先获取锁
				std::unique_lock <std::mutex> lock(taskQueMtx_);

				std::cout << "尝试获取任务... tid:" << std::this_thread::get_id() << std::endl;

				//cached模式下，有可能已经创建了很多的线程，但是空闲时间超过60S，应该把多余的线程回收掉
				//结束回收掉(超过initThreadSize_数量的线程要进行回收)
				//当前时间 - 上一次线程执行的时间 > 60s

					//每一秒钟返回一次 怎么区分：超时返回？还是有任务带执行返回
				while (taskQue_.size() == 0)
				{
					//线程池要结束，回收资源
					if (!isPoolRunning_)
					{
						threads_.erase(threadid);
						std::cout << "threadid:" << std::this_thread::get_id() << " exit!" << std::endl;
						exitCond_.notify_all();
						return; //线程函数结束，线程结束
					}
					if (poolMode_ == PoolMode::MODE_CACHED)
					{
						//条件变量超时返回
						if (std::cv_status::timeout ==
							notEmpty_.wait_for(lock, std::chrono::seconds(1)))
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() >= THREAD_MAX_IDLE_TIME
								&& curThreadSize_ > initThreadSize_)
							{
								//开始回收当前线程
								//记录线程数量的相关变量的值修改
								//把线程对象从线程列表容器中删除
								//threadid -> thread对象 -> 删除
								threads_.erase(threadid);
								curThreadSize_--;
								idleThreadSize_--;
								std::cout << "threadid:" << std::this_thread::get_id() << " exit!" << std::endl;
								return;
							}
						}
					}
					else
					{
						//等待notEmpty条件
						notEmpty_.wait(lock);
					}
				}
				idleThreadSize_--;

				std::cout << "获取任务成功... tid:" << std::this_thread::get_id() << std::endl;

				//如果不空，从任务队列中取一个任务出来
				task = taskQue_.front();
				taskQue_.pop();
				taskSize_--;

				//如果依然有剩余任务，继续通知其他的线程执行任务
				if (taskQue_.size() > 0)
				{
					notEmpty_.notify_all();
				}

				//取出一个任务，进行通知，通知继续提交生产任务
				notFull_.notify_all();
			}//应该把锁释放掉

			//当前线程负责执行这个任务
			if (task != nullptr)
			{
				//task->run(); //执行任务，把任务的返回值用setVal方法给到Result
				//task->exec();
				task();
			}

			idleThreadSize_++;
			lastTime = std::chrono::high_resolution_clock().now(); //更新线程执行完的时间
		}
	}
	//检查pool的运行状态
	bool checkRunningState() const
	{
		return isPoolRunning_;
	}
private:
	//std::vector<std::unique_ptr<Thread>> threads_;//线程列表
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;
	int initThreadSize_;//初始的线程数量
	std::atomic_int curThreadSize_;//记录当前线程池里面的线程总数量
	std::atomic_int idleThreadSize_;//记录空闲线程的数量
	int threadSizeThreshHold_;//线程数量上线阈值

	//Task = 函数对象
	using Task = std::function<void()>;
	std::queue<Task> taskQue_;//任务队列
	std::atomic_int taskSize_;//任务的数量
	int taskQueMaxThreadHold_;//任务队列数量上限的阈值

	std::mutex taskQueMtx_;//保证任务队列的线程安全
	std::condition_variable notFull_;//表示任务队列不满
	std::condition_variable notEmpty_;//表示任务队列不空
	std::condition_variable exitCond_; //表示等待线程资源全部回收

	PoolMode poolMode_; //当前线程池的工作模式
	std::atomic_bool isPoolRunning_;//表示当前线程池的启动状态
};


#endif // !THREADPOOL_H


