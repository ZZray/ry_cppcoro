#include "RyCoro.h"
#include <cassert>
//// 简单的随机数生成器
class IntReader {
public:
	bool await_ready() {
		return false;
	}

	void await_suspend(std::coroutine_handle<> handle) {
		std::thread thread([this, handle]() {
			std::cout << "await_suspend handle " << handle.address() << '\n';
			std::srand(static_cast<unsigned int>(std::time(nullptr)));
			std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });
			value_ = 1;
			handle.resume();
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
RyCoroTask<int> PrintInt() {
	IntReader reader1;
	std::cout << "1\n";
	int total = co_await reader1;

	IntReader reader2;
	std::cout << "2\n";
	total += co_await reader2;

	IntReader reader3;
	std::cout << "3\n";
	total += co_await reader3;

	std::cout << "total: " << total << '\n';
	co_return total;

}
// 模拟可能抛出异常的协程
RyCoroTask<int> mayThrowException(bool shouldThrow) {
	co_await RyCoro::delay(std::chrono::milliseconds(100));
	if (shouldThrow) {
		throw std::runtime_error("Intentional exception");
	}
	co_return 42;
}

// void 类型的协程
RyCoroTask<void> voidCoroutine() {
	std::cout << "开始执行 void 协程\n";
	co_await RyCoro::delay(std::chrono::milliseconds(100));
	std::cout << "void 协程执行完毕\n";
}

// 长时间运行的协程
RyCoroTask<int> longRunningTask() {
	std::cout << "开始长时间运行的任务\n";
	co_await RyCoro::delay(std::chrono::seconds(5));
	std::cout << "长时间运行的任务完成\n";
	co_return 100;
}

RyCoroTask<int> nestedCoroutine() {
	auto innerResult = co_await PrintInt();
	co_return innerResult * 2;
}

int main() {
	std::cout << "开始测试协程实现\n\n";

	// 1. 测试基本的协程功能
	std::cout << "1. 测试基本的协程功能：\n";
	auto printIntTask = PrintInt();
	auto result = printIntTask.waitForResult();
	assert(result.has_value() && "PrintInt should return a value");
	assert(*result == 3 && "PrintInt should return 3"); // 假设我们期望总和为3
	std::cout << "PrintInt 测试通过\n\n";

	// 2. 测试异常处理
	std::cout << "2. 测试异常处理：\n";
	auto exceptionTask = mayThrowException(true);
	exceptionTask.waitForResult();
	assert(exceptionTask.hasException() && "Exception should have been thrown");
	std::cout << "异常处理测试通过\n\n";

	// 3. 测试超时机制
	std::cout << "3. 测试超时机制：\n";
	auto timeoutTask = longRunningTask();
	auto timeoutResult = timeoutTask.waitForResult(std::chrono::seconds(1));
	assert(!timeoutResult.has_value() && "Task should have timed out");
	std::cout << "超时机制测试通过\n\n";

	// 4. 测试void类型的协程
	std::cout << "4. 测试void类型的协程：\n";
	auto voidTask = voidCoroutine();
	voidTask.waitForFinished(std::chrono::seconds(2));
	// 对于void协程，我们只能测试它是否正常完成，无法断言具体的返回值
	std::cout << "void协程测试完成\n\n";

	// 5. 测试协程的分离（detach）功能
	std::cout << "5. 测试协程的分离（detach）功能：\n";
	{
		auto detachedTask = longRunningTask();
		detachedTask.detach();
		std::cout << "协程已分离，主函数继续执行\n";
	}
	std::this_thread::sleep_for(std::chrono::seconds(6));
	std::cout << "分离的协程应该已经完成\n\n";

	// 6. 测试协程的取消（通过超时）
	std::cout << "6. 测试协程的取消（通过超时）：\n";
	{
		auto cancelTask = longRunningTask();
		auto cancelResult = cancelTask.waitForResult(std::chrono::milliseconds(100));
		assert(!cancelResult.has_value() && "Task should have been cancelled due to timeout");
		std::cout << "协程取消测试通过\n\n";
	}

	// 7. 测试正常完成的协程
	std::cout << "7. 测试正常完成的协程：\n";
	auto normalTask = mayThrowException(false);
	auto normalResult = normalTask.waitForResult();
	assert(normalResult.has_value() && "Task should complete normally");
	assert(*normalResult == 42 && "Task should return 42");
	std::cout << "正常完成的协程测试通过\n\n";

	// 8. 测试多个协程并发执行
	std::cout << "8. 测试多个协程并发执行：\n";
	auto task1 = PrintInt();
	auto task2 = PrintInt();
	auto task3 = PrintInt();

	auto result1 = task1.waitForResult();
	auto result2 = task2.waitForResult();
	auto result3 = task3.waitForResult();

	assert(result1.has_value() && result2.has_value() && result3.has_value() && "All tasks should complete");
	assert(*result1 == 3 && *result2 == 3 && *result3 == 3 && "All tasks should return 3");
	std::cout << "多个协程并发执行测试通过\n\n";

	// 9. 测试协程的嵌套
	std::cout << "9. 测试协程的嵌套：\n";
	auto nestedTask = nestedCoroutine();
	auto nestedResult = nestedTask.waitForResult();
	assert(nestedResult.has_value() && "Nested coroutine should complete");
	assert(*nestedResult == 6 && "Nested coroutine should return 6");
	std::cout << "协程嵌套测试通过\n\n";

	std::cout << "所有测试完成，测试通过！\n";

	return 0;
}