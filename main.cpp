#include <cassert>

#include "RyCoro.h"
// 测试用例
RyCoroTask<int> simpleTask()
{
	std::cout << "开始执行简单任务" << '\n';
	co_return 42;
}

RyCoroTask<void> voidTask()
{
	std::cout << "开始执行void任务" << '\n';
	co_return;
}

//RyCoroTask<int> cancellableTask() {
//	auto& sched = RyCoroScheduler::getInstance();
//	std::cout << "开始执行可取消任务" << '\n';
//	co_await sched.delay(std::chrono::seconds(2));
//	if (co_await std::suspend_always{}; static_cast<RyCoroTask<int>::promise_type*>(std::coroutine_handle<>::from_address(co_await std::noop_coroutine()).address())->cancelled) {
//		std::cout << "任务被取消" << '\n';
//		co_return -1;
//	}
//	std::cout << "可取消任务正常完成" << '\n';
//	co_return 24;
//}

RyCoroTask<void> multiStepTask(int id)
{
	auto& sched = RyCoroScheduler::getInstance();
	for (int i = 0; i < 3; ++i)
	{
		std::cout << "任务 " << id << ": 步骤 " << i << '\n';
		co_await RyCoro::delay(std::chrono::milliseconds(500));
	}
}

// 模拟一个耗时的异步操作
std::future<int> asyncOperation(int value)
{
	return std::async(std::launch::async, [value]()
		{
			std::this_thread::sleep_for(2s);
			return value * 2;
		});
}

RyCoroTask<void> asyncTask()
{
	auto future1 = asyncOperation(21);
	auto future2 = asyncOperation(42);

	std::cout << "等待第一个异步操作...\n";
	int result1 = co_await std::move(future1);
	std::cout << "第一个异步操作结果: " << result1 << '\n';

	std::cout << "等待第二个异步操作...\n";
	int result2 = co_await std::move(future2);
	std::cout << "第二个异步操作结果: " << result2 << '\n';
}

// 简单的随机数生成器
class IntReader {
public:
	bool await_ready() {
		return false;
	}

	void await_suspend(std::coroutine_handle<> handle) {
		std::thread thread([this, handle]() {
			std::srand(static_cast<unsigned int>(std::time(nullptr)));
			value_ = std::rand();
			std::this_thread::sleep_for(std::chrono::milliseconds{ 1000 });
			handle.resume();
			std::cout << "await_suspend handle " << handle.address() << '\n';
			});
		thread.detach();
	}

	int await_resume() {
		return value_;
	}

private:
	int value_{};
};

// 协程函数，读取三个随机数并打印其总和
RyCoroTask<void> PrintInt() {
	IntReader reader1;
	std::cout << "1\n";
	int total = co_await reader1;
	//std::cout << "2\n";

	//IntReader reader2;
	//std::cout << "3\n";
	//total += co_await reader2;
	//std::cout << "4\n";

	//IntReader reader3;
	//std::cout << "5\n";
	//total += co_await reader3;
	//std::cout << "6\n";

	std::cout << total << std::endl;
}

int main()
{
	auto& sched = RyCoroScheduler::getInstance();

	std::cout << "Starting the scheduler thread." << std::endl;

	std::jthread scheduler_thread([&]
		{
			sched.run();
		});

	{
		PrintInt();
		sched.wait();
	}
	{
		//std::cout << "开始测试 FutureAwaiter\n";
		//auto task = asyncTask();
		//sched.wait();
		//std::cout << "FutureAwaiter 测试完成\n";
	}
	sched.stop();
	return 0;
	{
		std::cout << "测试简单任务" << '\n';
		auto simple = simpleTask();
		//sched.schedule(simple.getHandle());
		sched.wait();
		assert(simple.getResult() == 42);
		assert(simple.getState() == RyCoroTaskState::Completed);
		std::cout << "简单任务测试通过" << '\n';
	}

	std::cout << "\n测试void任务" << '\n';
	auto voidT = voidTask();
	//sched.schedule(voidT.getHandle());
	sched.wait();
	assert(voidT.getState() == RyCoroTaskState::Completed);
	std::cout << "Void任务测试通过" << '\n';

	//std::cout << "\n测试可取消任务" << '\n';
	//auto cancellable = cancellableTask();
	//sched.schedule(cancellable.getHandle());
	//std::this_thread::sleep_for(std::chrono::seconds(1));
	//cancellable.cancel();
	//sched.exec();
	//assert(cancellable.getResult() == -1);
	//assert(cancellable.getState() == RyCoroTaskState::Completed);
	//std::cout << "可取消任务测试通过" << '\n';

	std::cout << "\n测试多步骤任务" << '\n';
	auto task1 = multiStepTask(1);
	auto task2 = multiStepTask(2);
	//sched.schedule(task1.getHandle());
	//sched.schedule(task2.getHandle());
	sched.wait();
	assert(task1.getState() == RyCoroTaskState::Completed);
	assert(task2.getState() == RyCoroTaskState::Completed);
	std::cout << "多步骤任务测试通过" << '\n';

	sched.stop();
	std::cout << "\n所有测试完成" << '\n';
	return 0;
}
