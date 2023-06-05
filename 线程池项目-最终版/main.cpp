#include<iostream>
#include<functional>
#include<thread>
#include<future>
#include"threadpool.h"
#include<chrono>
/*
如何能让线程池提交任务更加方便
*/
int sum1(int a, int b)
{
	std::this_thread::sleep_for(std::chrono::seconds(2));
	return a + b;
}

int main()
{	
	ThreadPool pool;
	pool.start(2);
	std::future<int> res1 = pool.submitTask(sum1, 1, 2);
	std::future<int> res2 = pool.submitTask(sum1, 3, 5);
	std::future<int> res3 = pool.submitTask(sum1, 6, 7);
	std::future<int> res4 = pool.submitTask([](int b, int e)->int {
		int sum = 0;
		for (int i = b; i <= e; i++)
		{
			sum += i;
		}
		return sum;
		}, 1, 100
	);
	std::future<int> res5 = pool.submitTask(sum1, 9, 7);
	std::cout << res1.get() << std::endl;
	std::cout << res2.get() << std::endl;
	std::cout << res3.get() << std::endl;
	std::cout << res4.get() << std::endl;
	std::cout << res5.get() << std::endl;
	return 0;
}