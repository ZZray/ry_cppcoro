#include <cassert>

#include "RyCoro.h"
// 测试用例
RyCoroTask<int> simpleTask() {
	std::cout << "开始执行简单任务" << '\n';
	co_return 42;
}

RyCoroTask<void> voidTask() {
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

RyCoroTask<void> multiStepTask(int id) {
	auto& sched = RyCoroScheduler::getInstance();
	for (int i = 0; i < 3; ++i) {
		std::cout << "任务 " << id << ": 步骤 " << i << '\n';
		co_await RyCoro::delay(std::chrono::milliseconds(500));
	}
}

int main() {
	auto& sched = RyCoroScheduler::getInstance();

	std::cout << "测试简单任务" << '\n';
	auto simple = simpleTask();
	//sched.schedule(simple.getHandle());
	sched.exec();
	assert(simple.getResult() == 42);
	assert(simple.getState() == RyCoroTaskState::Completed);
	std::cout << "简单任务测试通过" << '\n';

	std::cout << "\n测试void任务" << '\n';
	auto voidT = voidTask();
	//sched.schedule(voidT.getHandle());
	sched.exec();
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
	sched.exec();
	assert(task1.getState() == RyCoroTaskState::Completed);
	assert(task2.getState() == RyCoroTaskState::Completed);
	std::cout << "多步骤任务测试通过" << '\n';

	std::cout << "\n所有测试完成" << '\n';
	return 0;
}