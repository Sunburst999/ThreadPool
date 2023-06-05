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
const int THREAD_MAX_IDLE_TIME = 60; //��λ��


//�̳߳ؿ�֧�ֵ�ģʽ
enum class PoolMode
{
	MODE_FIXED,//�̶��������߳�
	MODE_CACHED,//�߳������ɶ�̬����
};

//�߳�����
class Thread {
public:
	using ThreadFunc = std::function<void(int)>;
	//�̹߳���
	Thread(ThreadFunc func)
		:func_(func)
		, threadId_(generateId_++)
	{}
	//�߳�����
	~Thread() = default;
	//�����߳�
	void start()
	{
		//����һ���߳���ִ��һ���̺߳���
		std::thread t(func_, threadId_);
		t.detach();//���÷����߳� pthread_detach����Ϊ�����߳�
	}


	//��ȡ�߳�ID
	int getId()const
	{
		return threadId_;
	}
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_; //�߳�ID
};

int Thread::generateId_ = 0;

//�̳߳�����
class ThreadPool {
public:
	//�̳߳ع���
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
	//�̳߳�����
	~ThreadPool()
	{
		isPoolRunning_ = false;
		//�ȴ��̳߳��������е��̷߳��� ������״̬������ & ����ִ��������
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		notEmpty_.notify_all();
		exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
	}
	//�����̳߳صĹ���ģʽ
	void setMode(PoolMode mode)
	{
		if (checkRunningState())
			return;
		poolMode_ = mode;
	}

	//����task�������������ֵ
	void setTaskQueThreadHold(int threadhold)
	{
		if (checkRunningState())
			return;
		taskQueMaxThreadHold_ = threadhold;
	}

	//�����̳߳�cachedģʽ���߳���ֵ
	void setThreadSizeThreshHold(int threshhold)
	{
		if (checkRunningState())
			return;
		if (poolMode_ == PoolMode::MODE_CACHED)
		{
			threadSizeThreshHold_ = threshhold;
		}
	}

	//���̳߳��ύ����
	//ʹ�ÿɱ��ģ���̣���submitTask���Խ������������������������Ĳ���
	//pool.submitTask(sum1, 10, 20);
	template<typename Func, typename... Args>
	auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
	{
		//������񣬷����������
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
			);
		std::future<RType> result = task->get_future();

		//��ȡ�� 
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		//�߳�ͨ�ţ��ȴ���������п���
		//�����ύ�����ʱ�䲻����������1S�������ж��ύ����ʧ�ܣ�����
		if (!notFull_.wait_for(lock, std::chrono::seconds(1),
			[&]()->bool {return taskQue_.size() < taskQueMaxThreadHold_; }))
		{
			//�ȴ�1S����Ȼû������
			std::cerr << "task queue is full,submit task fail!" << std::endl;
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]()->RType {return RType(); });
			(* task)();
			return task->get_future();
		}
		//����п��࣬������Ž����������
		//taskQue_.emplace(sp);
		taskQue_.emplace([task]() {(*task)(); });
		taskSize_++;
		//��Ϊ�·�������������п϶������ˣ���notEmpty_�Ͻ���֪ͨ,�Ͻ������߳�ִ������
		notEmpty_.notify_all();

		//cached ������ȽϽ��� ������С��������� ��Ҫ�������������Ϳ����̵߳��������ж��Ƿ���Ҫ�����µ��̳߳���
		if (poolMode_ == PoolMode::MODE_CACHED
			&& taskSize_ > idleThreadSize_
			&& curThreadSize_ < threadSizeThreshHold_)
		{
			std::cout << "creat new thread" << std::endl;
			//�������߳�
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			//threads_.emplace_back(std::move(ptr));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			//�����߳�
			threads_[threadId]->start();
			//�޸��̸߳�����صı���
			curThreadSize_++;
			idleThreadSize_++;
		}

		//���������Result����
		return result;
	}


	//�����̳߳�
	void start(int initThreadSize = std::thread::hardware_concurrency())
	{
		//�����̳߳ص�����״̬
		isPoolRunning_ = true;

		//��¼��ʼ�̸߳���
		initThreadSize_ = initThreadSize;
		curThreadSize_ = initThreadSize;

		//����thread�̶߳����ʱ�򣬰��̺߳�������thread�̶߳���
		for (int i = 0; i < initThreadSize_; i++)
		{
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			//threads_.emplace_back(std::move(ptr));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
		}

		//���������߳� std::vector<Thread*> threads_;
		for (int i = 0; i < initThreadSize_; i++)
		{
			threads_[i]->start(); //��Ҫȥִ��һ���̺߳���
			idleThreadSize_++;//��¼��ʼ�����̵߳�����
		}
	}

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	//�����̺߳���
	void threadFunc(int threadid)
	{
		/*std::cout << "begin threadfunc id:" << std::this_thread::get_id() << std::endl;
	std::cout << "end threadfunc id:" << std::this_thread::get_id() << std::endl;*/
		auto lastTime = std::chrono::high_resolution_clock().now();

		//�����������ִ����ɣ��̳߳زſ��Ի��������߳���Դ
		for (;;)
		{
			Task task;
			{
				//�Ȼ�ȡ��
				std::unique_lock <std::mutex> lock(taskQueMtx_);

				std::cout << "���Ի�ȡ����... tid:" << std::this_thread::get_id() << std::endl;

				//cachedģʽ�£��п����Ѿ������˺ܶ���̣߳����ǿ���ʱ�䳬��60S��Ӧ�ðѶ�����̻߳��յ�
				//�������յ�(����initThreadSize_�������߳�Ҫ���л���)
				//��ǰʱ�� - ��һ���߳�ִ�е�ʱ�� > 60s

					//ÿһ���ӷ���һ�� ��ô���֣���ʱ���أ������������ִ�з���
				while (taskQue_.size() == 0)
				{
					//�̳߳�Ҫ������������Դ
					if (!isPoolRunning_)
					{
						threads_.erase(threadid);
						std::cout << "threadid:" << std::this_thread::get_id() << " exit!" << std::endl;
						exitCond_.notify_all();
						return; //�̺߳����������߳̽���
					}
					if (poolMode_ == PoolMode::MODE_CACHED)
					{
						//����������ʱ����
						if (std::cv_status::timeout ==
							notEmpty_.wait_for(lock, std::chrono::seconds(1)))
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() >= THREAD_MAX_IDLE_TIME
								&& curThreadSize_ > initThreadSize_)
							{
								//��ʼ���յ�ǰ�߳�
								//��¼�߳���������ر�����ֵ�޸�
								//���̶߳�����߳��б�������ɾ��
								//threadid -> thread���� -> ɾ��
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
						//�ȴ�notEmpty����
						notEmpty_.wait(lock);
					}
				}
				idleThreadSize_--;

				std::cout << "��ȡ����ɹ�... tid:" << std::this_thread::get_id() << std::endl;

				//������գ������������ȡһ���������
				task = taskQue_.front();
				taskQue_.pop();
				taskSize_--;

				//�����Ȼ��ʣ�����񣬼���֪ͨ�������߳�ִ������
				if (taskQue_.size() > 0)
				{
					notEmpty_.notify_all();
				}

				//ȡ��һ�����񣬽���֪ͨ��֪ͨ�����ύ��������
				notFull_.notify_all();
			}//Ӧ�ð����ͷŵ�

			//��ǰ�̸߳���ִ���������
			if (task != nullptr)
			{
				//task->run(); //ִ�����񣬰�����ķ���ֵ��setVal��������Result
				//task->exec();
				task();
			}

			idleThreadSize_++;
			lastTime = std::chrono::high_resolution_clock().now(); //�����߳�ִ�����ʱ��
		}
	}
	//���pool������״̬
	bool checkRunningState() const
	{
		return isPoolRunning_;
	}
private:
	//std::vector<std::unique_ptr<Thread>> threads_;//�߳��б�
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;
	int initThreadSize_;//��ʼ���߳�����
	std::atomic_int curThreadSize_;//��¼��ǰ�̳߳�������߳�������
	std::atomic_int idleThreadSize_;//��¼�����̵߳�����
	int threadSizeThreshHold_;//�߳�����������ֵ

	//Task = ��������
	using Task = std::function<void()>;
	std::queue<Task> taskQue_;//�������
	std::atomic_int taskSize_;//���������
	int taskQueMaxThreadHold_;//��������������޵���ֵ

	std::mutex taskQueMtx_;//��֤������е��̰߳�ȫ
	std::condition_variable notFull_;//��ʾ������в���
	std::condition_variable notEmpty_;//��ʾ������в���
	std::condition_variable exitCond_; //��ʾ�ȴ��߳���Դȫ������

	PoolMode poolMode_; //��ǰ�̳߳صĹ���ģʽ
	std::atomic_bool isPoolRunning_;//��ʾ��ǰ�̳߳ص�����״̬
};


#endif // !THREADPOOL_H


