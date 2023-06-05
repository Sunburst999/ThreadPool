#include"threadpool.h"
#include<functional>
#include<thread>
#include <iostream>
const int TASK_MAX_THREADHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 60; //��λ��

//�̳߳ع���
ThreadPool::ThreadPool()
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
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;
	//�ȴ��̳߳��������е��̷߳��� ������״̬������ & ����ִ��������
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}

//�����̳߳صĹ���ģʽ
void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState())
		return;
	poolMode_ = mode;
}

//����task�������������ֵ
void ThreadPool::setTaskQueThreadHold(int threadhold)
{
	if (checkRunningState())
		return;
	taskQueMaxThreadHold_ = threadhold;
}

//�����̳߳�cachedģʽ���߳���ֵ
void ThreadPool::setThreadSizeThreshHold(int threshhold)
{
	if (checkRunningState())
		return;
	if (poolMode_ == PoolMode::MODE_CACHED)
	{
		threadSizeThreshHold_ = threshhold;
	}
}

//���̳߳��ύ����
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//��ȡ�� 
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	//�߳�ͨ�ţ��ȴ���������п���
	//�����ύ�����ʱ�䲻����������1S�������ж��ύ����ʧ�ܣ�����
	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool {return taskQue_.size() < taskQueMaxThreadHold_; }))
	{
		//�ȴ�1S����Ȼû������
		std::cerr << "task queue is full,submit task fail!" << std::endl;
		return Result(sp,false);
	}
	//����п��࣬������Ž����������
	taskQue_.emplace(sp);
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
	return Result(sp);
}

//�����̳߳�
void ThreadPool::start(int initThreadSize)
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

//�����̺߳���
void ThreadPool::threadFunc(int threadid)
{
	/*std::cout << "begin threadfunc id:" << std::this_thread::get_id() << std::endl;
	std::cout << "end threadfunc id:" << std::this_thread::get_id() << std::endl;*/
	auto lastTime = std::chrono::high_resolution_clock().now();

	//�����������ִ����ɣ��̳߳زſ��Ի��������߳���Դ
	for(;;)
	{
		std::shared_ptr<Task> task;
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

					//�̳߳�Ҫ�����������߳���Դ
					/*if (!isPoolRunning_)
					{
						threads_.erase(threadid);
						std::cout << "threadid:" << std::this_thread::get_id() << " exit!" << std::endl;
						exitCond_.notify_all();
						return;
					}*/
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
			task->exec();
		}
		
		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now(); //�����߳�ִ�����ʱ��
	}
}

bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;
}
///////////////////////////�̷߳���ʵ��

int Thread::generateId_ = 0;

//�̹߳���
Thread::Thread(ThreadFunc func)
	:func_(func)
	,threadId_(generateId_++)
{}

//�߳�����
Thread::~Thread()
{}

void Thread::start()
{
	//����һ���߳���ִ��һ���̺߳���
	std::thread t(func_, threadId_);
	t.detach();//���÷����߳� pthread_detach����Ϊ�����߳�
}

int Thread::getId()const
{
	return threadId_;
}

//////////////////Task������ʵ��
Task::Task()
	:result_(nullptr)
{}
void Task::exec()
{
	//�����淢����̬
	if (result_ != nullptr)
	{
		result_->setVal(run());
	}
}

void Task::setResult(Result* res)
{
	result_ = res;
}
//////////////////Result������ʵ��

Result::Result(std::shared_ptr<Task> task, bool isValid)
	: isValid_(isValid)
	, task_(task)
{
	task_->setResult(this);
}

Any Result::get()  //�û�����
{
	if (!isValid_)
	{
		return "";
	}
	sem_.wait();//task�������û��ִ���꣬���������
	return std::move(any_);
}

void Result::setVal(Any any)
{
	//�洢task�ķ���ֵ
	this->any_ = std::move(any);
	sem_.post();//�Ѿ���ȡ������ķ���ֵ�������ź���Դ��
}
