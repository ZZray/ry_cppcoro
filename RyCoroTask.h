/**
 * @author rayzhang
 * @brief
 * @date 2024年07月16日
 */
#pragma once
#include <coroutine>
#include <iostream>
#include <optional>
#include <ostream>
#include "RyCoro.h"
#include "RyCoroScheduler.h"


 // 定义任务的状态
enum class RyCoroTaskState {
	Created,    // 已创建
	Running,    // 运行中
	Suspended,  // 已暂停
	Completed,  // 已完成
	Cancelled   // 已取消
};

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
		// co_return, promise_type.return_value(total)
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
		if (coro && !coro.done()) {
			coro.promise().cancelled = true;
		}
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