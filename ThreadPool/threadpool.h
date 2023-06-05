#pragma once
#ifndef THREADPOOL_H
#define THREADPOOL_H


#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<functional>
#include<condition_variable>
#include<unordered_map>

//Any����:���Խ����������ݵ�����
class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;
	
	//������췽������Any����������������
	template<typename T>
	Any(T data) :base_(std::make_unique<Derive<T>>(data))
	{}

	//��������ܰ�Any��������洢��data������ȡ����
	template<typename T>
	T cast_()
	{
		//����ָ��-��������ָ��
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			throw"type is unmatch!";
		}
		return pd->data_;
	}
private:
	//��������
	class Base
	{
	public:
		virtual ~Base() = default;
	};

	//����������
	template<typename T>
	class Derive :public Base
	{
	public:
		Derive(T data) :data_(data)
		{}
		T data_; //�����˾�������
	};

private:
	//����һ�������ָ��
	std::unique_ptr<Base> base_;
};

//ʵ��һ���ź�����
class Semaphore
{ 
public:
	Semaphore(int limit = 0)
		:resLimit_(limit)
	{}
	~Semaphore() = default;

	//��ȡһ���ź�����Դ
	void wait()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		//�ȴ��ź�������Դ��û����Դ�Ļ�����������ǰ�߳�
		cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
		resLimit_--;
	}
	//����һ���ź�����Դ
	void post()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		resLimit_++;
		cond_.notify_all();
	}
private:
	int resLimit_;
	std::mutex mtx_;
	std::condition_variable cond_;
};

//Task��ǰ������
class Task;

//ʵ�ֽ����ύ���̳߳ص�task����ִ����ɺ�ķ���ֵ����Result
class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;

	//����һ��setVal��������ȡ����ִ����ķ���ֵ
	void setVal(Any any);
	//�������get�������û��������������ȡtask�ķ���ֵ
	Any get();
private:
	Any any_;//�洢����ķ���ֵ
	Semaphore sem_;//�߳�ͨ���ź���
	std::shared_ptr<Task> task_;//ָ���Ӧ��ȡ����ֵ���������
	std::atomic_bool isValid_;//����ֵ�Ƿ���Ч
};

//����������
class Task {
public:
	Task();
	~Task() = default;
	void exec();
	void setResult(Result* res);
	virtual Any run() = 0;//�û������Զ��������������ͣ���Task�̳У���дrun����
private:
	Result* result_; //Result�������������Ҫ����Task
};

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
	Thread(ThreadFunc func);
	//�߳�����
	~Thread();
	//�����߳�
	void start();

	//��ȡ�߳�ID
	int getId() const;
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId_; //�߳�ID
};


//�̳߳�����
class ThreadPool {
public:
	//�̳߳ع���
	ThreadPool();
	//�̳߳�����
	~ThreadPool();
	//�����̳߳صĹ���ģʽ
	void setMode(PoolMode mode);

	//����task�������������ֵ
	void setTaskQueThreadHold(int threadhold);

	//�����̳߳�cachedģʽ���߳���ֵ
	void setThreadSizeThreshHold(int threshhold);

	//���̳߳��ύ����
	Result submitTask(std::shared_ptr<Task> sp);

	//�����̳߳�
	void start(int initThreadSize = std::thread::hardware_concurrency());

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	//�����̺߳���
	void threadFunc(int threadid);
	//���pool������״̬
	bool checkRunningState() const; 
private:
	//std::vector<std::unique_ptr<Thread>> threads_;//�߳��б�
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;
	int initThreadSize_;//��ʼ���߳�����
	std::atomic_int curThreadSize_;//��¼��ǰ�̳߳�������߳�������
	std::atomic_int idleThreadSize_;//��¼�����̵߳�����
	int threadSizeThreshHold_;//�߳�����������ֵ

	std::queue<std::shared_ptr<Task>> taskQue_;//�������
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

