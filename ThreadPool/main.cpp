#include<iostream>
#include<chrono>
#include<thread>
#include"threadpool.h"

using uLong = unsigned long long;

class MyTask : public Task
{
public:

	MyTask(uLong begin, uLong end)
		:begin_(begin)
		,end_(end)
	{}
	//问题一，怎么设计run函数的返回值，可以表示任意的类型
	Any run()  //run方法最终在线程池分配的线程中去执行
	{
		std::cout << "begin tid:" << std::this_thread::get_id() << std::endl;
		//std::this_thread::sleep_for(std::chrono::seconds(3));
		uLong sum = 0;
		for (uLong i = begin_; i <= end_; i++)
		{
			sum += i;
		}
		std::cout << "end tid:" << std::this_thread::get_id() << std::endl;
		return sum;
	}
private:
	int begin_;
	int end_;
};
//int main()
//{
//	{
//		ThreadPool pool;
//		pool.setMode(PoolMode::MODE_CACHED);
//		pool.start(4);
//		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
//		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
//		/*uLong sum2 = res2.get().cast_<uLong>();
//		uLong sum1 = res1.get().cast_<uLong>();
//		std::cout << sum1 + sum2 << std::endl;*/
//	}
//	std::cout << "main over" << std::endl;
//	getchar();
//
//	//{
//	//	ThreadPool pool;
//	//	//设置线程池模式
//	//	pool.setMode(PoolMode::MODE_CACHED);
//	//	//开始启动线程池
//	//	pool.start(4);
//	//	//如何设置这里的Result机制呢
//	//	Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
//	//	Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
//	//	Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
//	//	pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
//	//	pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
//	//	pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
//	//	uLong sum1 = res1.get().cast_<uLong>();//get返回一个Any类型，怎么样转成具体类型
//	//	uLong sum2 = res2.get().cast_<uLong>();
//	//	uLong sum3 = res3.get().cast_<uLong>();
//
//	//	//Master - Slave 线程模型
//	//	//Master线程用来分解任务，然后给各个Salve线程分配任务
//	//	//等待各个Slave线程完成任务，返回结果
//	//	//Master线程合并各个结果，输出
//	//	std::cout << (sum1 + sum2 + sum3) << std::endl;
//	//}
//	//getchar();
//
//}