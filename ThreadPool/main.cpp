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
	//����һ����ô���run�����ķ���ֵ�����Ա�ʾ���������
	Any run()  //run�����������̳߳ط�����߳���ȥִ��
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
//	//	//�����̳߳�ģʽ
//	//	pool.setMode(PoolMode::MODE_CACHED);
//	//	//��ʼ�����̳߳�
//	//	pool.start(4);
//	//	//������������Result������
//	//	Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
//	//	Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
//	//	Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
//	//	pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
//	//	pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
//	//	pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
//	//	uLong sum1 = res1.get().cast_<uLong>();//get����һ��Any���ͣ���ô��ת�ɾ�������
//	//	uLong sum2 = res2.get().cast_<uLong>();
//	//	uLong sum3 = res3.get().cast_<uLong>();
//
//	//	//Master - Slave �߳�ģ��
//	//	//Master�߳������ֽ�����Ȼ�������Salve�̷߳�������
//	//	//�ȴ�����Slave�߳�������񣬷��ؽ��
//	//	//Master�̺߳ϲ�������������
//	//	std::cout << (sum1 + sum2 + sum3) << std::endl;
//	//}
//	//getchar();
//
//}