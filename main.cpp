#include <chrono>
#include <coroutine>
#include <deque>
#include <iostream>
#include <thread>
#include <atomic>
#include <cassert>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
using namespace std::literals;
// 定义协程的等待状态
enum class RyCoroAwaitState {
	ScheduleNextFrame,  // 在下一帧调度
	ScheduleImmediately,// 立即调度
	NoSchedule          // 不调度
};

// 定义任务的状态
enum class RyCoroTaskState {
	Created,    // 已创建
	Running,    // 运行中
	Suspended,  // 已暂停
	Completed,  // 已完成
	Cancelled   // 已取消
};

// 调度器统计信息
struct RyCoroSchedulerStats {
	std::atomic<size_t> pendingTaskCount{ 0 };  // 待处理任务数量
};

// 调度选项
struct RyCoroScheduleOptions {
	bool immediate = true;  // 是否立即执行
};
class RyCoroScheduler;
using RyCoroSchedulerHandle = std::coroutine_handle<>;
// 时间等待器
class TimeAwaitable {
public:
	TimeAwaitable(RyCoroScheduler* sched, std::chrono::microseconds duration)
		: sched_(sched), duration_(duration) {}

	bool await_ready() const noexcept;

	void await_suspend(RyCoroSchedulerHandle h);

	void await_resume() const noexcept {
		std::cout << "时间等待结束，恢复协程" << std::endl;
	}

private:
	RyCoroScheduler* sched_;
	std::chrono::microseconds duration_;
};

template<typename T>
class FutureAwaiter {
public:
	FutureAwaiter(std::future<T>&& future) : future_(std::move(future)) {}

	bool await_ready() const noexcept {
		return future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
	}

	void await_suspend(std::coroutine_handle<> h) {
		std::thread([this, h]() mutable {
			future_.wait();
			h.resume();
			}).detach();
	}

	T await_resume() {
		return future_.get();
	}

private:
	std::future<T> future_;
};

template<typename T>
FutureAwaiter<T> operator co_await(std::future<T>&& future) {
	return FutureAwaiter<T>(std::move(future));
}



template<typename T>
class RyCoroTask {
public:
	struct promise_type {
		std::optional<T> value;
		RyCoroAwaitState lastAwaitState = RyCoroAwaitState::ScheduleNextFrame;
		RyCoroTaskState state = RyCoroTaskState::Created;
		std::atomic<bool> cancelled{ false };

		RyCoroTask get_return_object() {
			std::cout << "创建协程任务" << std::endl;
			return RyCoroTask(std::coroutine_handle<promise_type>::from_promise(*this));
		}
		std::suspend_always initial_suspend() {
			std::cout << "协程初始挂起" << std::endl;
			return {};
		}
		std::suspend_always final_suspend() noexcept {
			std::cout << "协程最终挂起" << std::endl;
			return {};
		}
		void unhandled_exception() { std::terminate(); }

		void return_value(T v) {
			std::cout << "协程返回值: " << v << std::endl;
			value = std::move(v);
			state = RyCoroTaskState::Completed;
		}
	};

	RyCoroTask(std::coroutine_handle<promise_type> h) : coro(h) {}
	~RyCoroTask() {
		if (coro) {
			std::cout << "销毁协程任务" << std::endl;
			coro.destroy();
		}
	}

	T getResult() {
		if (!coro.promise().value.has_value()) {
			std::cout << "恢复协程以获取结果" << std::endl;
			coro.resume();
		}
		std::cout << "返回协程结果" << std::endl;
		return coro.promise().value.value();
	}

	std::coroutine_handle<> getHandle() { return coro; }

	RyCoroTaskState getState() const { return coro.promise().state; }

	void cancel() {
		std::cout << "取消协程任务" << std::endl;
		coro.promise().cancelled = true;
	}
	bool isCancelled() const { return coro.promise().cancelled; }

private:
	std::coroutine_handle<promise_type> coro;
};

template<>
class RyCoroTask<void> {
public:
	struct promise_type {
		RyCoroAwaitState lastAwaitState = RyCoroAwaitState::ScheduleNextFrame;
		RyCoroTaskState state = RyCoroTaskState::Created;
		std::atomic<bool> cancelled{ false };

		RyCoroTask get_return_object() {
			std::cout << "创建void协程任务" << std::endl;
			return RyCoroTask(std::coroutine_handle<promise_type>::from_promise(*this));
		}
		std::suspend_always initial_suspend() {
			std::cout << "void协程初始挂起" << std::endl;
			return {};
		}
		std::suspend_always final_suspend() noexcept {
			std::cout << "void协程最终挂起" << std::endl;
			return {};
		}
		void unhandled_exception() { std::terminate(); }

		void return_void() {
			std::cout << "void协程返回" << std::endl;
			state = RyCoroTaskState::Completed;
		}
	};

	RyCoroTask(std::coroutine_handle<promise_type> h) : coro(h) {}
	~RyCoroTask() {
		if (coro) {
			std::cout << "销毁void协程任务" << std::endl;
			coro.destroy();
		}
	}

	void getResult() {
		if (coro.promise().state != RyCoroTaskState::Completed) {
			std::cout << "恢复void协程以完成执行" << std::endl;
			coro.resume();
		}
		std::cout << "void协程执行完毕" << std::endl;
	}

	std::coroutine_handle<> getHandle() { return coro; }

	RyCoroTaskState getState() const { return coro.promise().state; }

	void cancel() {
		std::cout << "取消void协程任务" << std::endl;
		coro.promise().cancelled = true;
	}
	bool isCancelled() const { return coro.promise().cancelled; }

private:
	std::coroutine_handle<promise_type> coro;
};

class RyCoroScheduler {
public:
	using HandleT = RyCoroSchedulerHandle;

	// 获取调度器单例
	static RyCoroScheduler& getInstance() {
		static RyCoroScheduler instance;
		return instance;
	}

	// 调度协程
	void schedule(HandleT handle, std::chrono::microseconds delay = 0ms) {
		std::cout << "调度协程任务" << std::endl;
		if (delay.count() != 0) {
			std::thread([this, handle, delay]() {
				std::cout << "开始延迟调度，等待" << delay.count() << "微秒" << std::endl;
				std::this_thread::sleep_for(delay);
				std::cout << "延迟结束，调度协程" << std::endl;
				this->schedule(handle);
				}).detach();
		}
		else {
			scheduleImpl(handle);
		}
	}

	TimeAwaitable delay(std::chrono::microseconds duration) {
		return TimeAwaitable(this, duration);
	}
	// 运行调度器
	void run() {
		std::cout << "开始运行调度器" << std::endl;
		while (!readyQueue.empty()) {
			auto coro = readyQueue.front();
			readyQueue.pop_front();

			stats.pendingTaskCount--;

			if (!coro.done()) {
				std::cout << "恢复协程执行" << std::endl;
				coro.resume();
				if (!coro.done()) {
					std::cout << "协程未完成，重新调度" << std::endl;
					scheduleImpl(coro, RyCoroAwaitState::ScheduleNextFrame);
				}
				else {
					std::cout << "协程已完成" << std::endl;
				}
			}
		}
		std::cout << "调度器运行结束" << std::endl;
	}

	const RyCoroSchedulerStats& getStats() const {
		return stats;
	}


private:
	RyCoroScheduler() = default;
	RyCoroScheduler(const RyCoroScheduler&) = delete;
	RyCoroScheduler& operator=(const RyCoroScheduler&) = delete;

	void scheduleImpl(HandleT coro, RyCoroAwaitState state = RyCoroAwaitState::ScheduleImmediately) {

		/**
		 * threadPool.enqueue([this, task]() {
			while (task->resume()) {
				if (task->isCancelled()) {
					break;
				}
				// 如果任务被挂起，我们可以在这里进行一些调度决策
				// 比如将任务重新加入队列或者进行其他操作
			}
		});
		 */

		std::lock_guard<std::mutex> lock(queueMutex);
		switch (state) {
		case RyCoroAwaitState::ScheduleNextFrame:
			std::cout << "将协程加入下一帧队列" << std::endl;
			readyQueue.emplace_back(coro);
			break;
		case RyCoroAwaitState::ScheduleImmediately:
			std::cout << "将协程加入立即执行队列" << std::endl;
			readyQueue.emplace_front(coro);
			break;
		case RyCoroAwaitState::NoSchedule:
		default:
			std::cout << "协程不被调度" << std::endl;
			return;
		}
		stats.pendingTaskCount++;
		std::cout << "当前待处理任务数: " << stats.pendingTaskCount << std::endl;
	}

	std::deque<HandleT> readyQueue;
	RyCoroSchedulerStats stats;
	std::mutex queueMutex;
};


bool TimeAwaitable::await_ready() const noexcept
{
	return false;
}

void TimeAwaitable::await_suspend(RyCoroSchedulerHandle h)
{
	std::cout << "暂停协程，等待" << duration_.count() << "微秒" << std::endl;
	sched_->schedule(h, duration_);
}


// 测试用例
RyCoroTask<int> simpleTask() {
	std::cout << "开始执行简单任务" << std::endl;
	co_return 42;
}

RyCoroTask<void> voidTask() {
	std::cout << "开始执行void任务" << std::endl;
	co_return;
}

//RyCoroTask<int> cancellableTask() {
//	auto& sched = RyCoroScheduler::getInstance();
//	std::cout << "开始执行可取消任务" << std::endl;
//	co_await sched.delay(std::chrono::seconds(2));
//	if (co_await std::suspend_always{}; static_cast<RyCoroTask<int>::promise_type*>(std::coroutine_handle<>::from_address(co_await std::noop_coroutine()).address())->cancelled) {
//		std::cout << "任务被取消" << std::endl;
//		co_return -1;
//	}
//	std::cout << "可取消任务正常完成" << std::endl;
//	co_return 24;
//}

RyCoroTask<void> multiStepTask(int id) {
	auto& sched = RyCoroScheduler::getInstance();
	for (int i = 0; i < 3; ++i) {
		std::cout << "任务 " << id << ": 步骤 " << i << std::endl;
		co_await sched.delay(std::chrono::milliseconds(500));
	}
}

int main() {
	auto& sched = RyCoroScheduler::getInstance();

	std::cout << "测试简单任务" << std::endl;
	auto simple = simpleTask();
	sched.schedule(simple.getHandle());
	sched.run();
	assert(simple.getResult() == 42);
	assert(simple.getState() == RyCoroTaskState::Completed);
	std::cout << "简单任务测试通过" << std::endl;

	std::cout << "\n测试void任务" << std::endl;
	auto voidT = voidTask();
	sched.schedule(voidT.getHandle());
	sched.run();
	assert(voidT.getState() == RyCoroTaskState::Completed);
	std::cout << "Void任务测试通过" << std::endl;

	//std::cout << "\n测试可取消任务" << std::endl;
	//auto cancellable = cancellableTask();
	//sched.schedule(cancellable.getHandle());
	//std::this_thread::sleep_for(std::chrono::seconds(1));
	//cancellable.cancel();
	//sched.run();
	//assert(cancellable.getResult() == -1);
	//assert(cancellable.getState() == RyCoroTaskState::Completed);
	//std::cout << "可取消任务测试通过" << std::endl;

	std::cout << "\n测试多步骤任务" << std::endl;
	auto task1 = multiStepTask(1);
	auto task2 = multiStepTask(2);
	sched.schedule(task1.getHandle());
	sched.schedule(task2.getHandle());
	sched.run();
	assert(task1.getState() == RyCoroTaskState::Completed);
	assert(task2.getState() == RyCoroTaskState::Completed);
	std::cout << "多步骤任务测试通过" << std::endl;

	std::cout << "\n所有测试完成" << std::endl;
	return 0;
}