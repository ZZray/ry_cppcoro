/**
 * @author rayzhang
 * @brief
 * @date 2024年07月17日
 */
#pragma once

#include <coroutine>
#include <iostream>
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <future>

template <typename T>
class RyCoroTask;
namespace RyCoro::detail
{

	template<typename T>
	struct RyCoroTaskPromiseBase : std::promise<T>
	{
		std::exception_ptr exceptionPtr;
		std::shared_ptr<std::atomic<bool>> detached = std::make_shared<std::atomic<bool>>(false);
		std::coroutine_handle<> continuation = std::noop_coroutine();// 谁在等待我

		RyCoroTask<T> get_return_object();
		std::suspend_always initial_suspend()
		{
			std::cout << "协程开始执行\n"; // "协程初始挂起\n";
			return {};
		}

		struct FinalAwaiter
		{
			bool await_ready() const noexcept { return false; }

			template<typename promise_type>
			auto await_suspend(std::coroutine_handle<promise_type> h) const noexcept
			{
				auto detached = h.promise().detached;
				if (detached && detached->load()) {
					std::cout << "协程已分离，执行销毁\n";
					h.destroy();
				}
				else {
					std::cout << "协程未分离，不销毁\n";
				}
				return h.promise().continuation;// 当前协程结束的时候，返回并恢复父协程。同样在task中，co_await 子协程的时候，会让子协程恢复执行
			}
			void await_resume() const noexcept { }
		};
		FinalAwaiter final_suspend() const noexcept
		{
			std::cout << "协程最终执行" << '\n';
			return {};
		}

		void unhandled_exception()
		{
			exceptionPtr = std::current_exception();
			this->set_exception(exceptionPtr);
		}

	};

	template <typename T>
	struct RyCoroTaskPromise : RyCoro::detail::RyCoroTaskPromiseBase<T>
	{
		RyCoroTask<T> get_return_object();

		// co_return, promise_type.return_value(total)
		void return_value(T v)
		{
			std::cout << "协程返回值: " << v << '\n';
			this->set_value(std::move(v));
		}
	};

	template<>
	struct RyCoroTaskPromise<void> : RyCoroTaskPromiseBase<void>
	{
		RyCoroTask<void> get_return_object();

		void return_void()
		{
			std::cout << "void协程返回" << '\n';
			this->set_value();
		}
	};

}

template <typename T>
class RyCoroTaskBase
{
public:
	using promise_type = RyCoro::detail::RyCoroTaskPromise<T>;

	RyCoroTaskBase(std::coroutine_handle<promise_type> h)
		: m_detached(h.promise().detached), m_coro(h)
	{
	}
	~RyCoroTaskBase()
	{
		if (m_coro) {
			if (m_coro.done()) {
				std::cout << "销毁协程任务" << '\n';
				m_coro.destroy();
			}
			else {
				std::cout << "协程任务仍在执行，标记为分离" << '\n';
				detach();
			}
		}
	}
	RyCoroTaskBase(const RyCoroTaskBase&) = delete;
	RyCoroTaskBase& operator=(const RyCoroTaskBase&) = delete;

	RyCoroTaskBase(RyCoroTaskBase&& other) noexcept
		: m_detached(std::move(other.m_detached)), m_coro(other.m_coro)
	{
		other.m_coro = nullptr;
	}

	RyCoroTaskBase& operator=(RyCoroTaskBase&& other) noexcept
	{
		if (this != &other)
		{
			if (m_coro && !m_detached->load())
			{
				m_coro.destroy();
			}
			m_coro = other.m_coro;
			m_detached = std::move(other.m_detached);
			other.m_coro = nullptr;
		}
		return *this;
	}

	// 获取协程句柄
	std::coroutine_handle<> handle() { return m_coro; }

	// 获取 shared_future，可以用于异步获取结果
	std::shared_future<T> future()
	{
		if (!m_future.valid()) {
			m_future = m_coro.promise().get_future().share();
		}
		return m_future;
	}

	// 检查是否有异常
	bool hasException() const {
		return m_coro.promise().exceptionPtr != nullptr;
	}

	// 新增：获取异常（如果有）
	std::exception_ptr exception() const { return m_coro.promise().exceptionPtr; }

	//
	void resume() const
	{
		if (this->m_coro && !this->m_coro.done() && !this->m_started) {
			this->m_coro.resume();
		}
	}
	//
	void detach() const { m_detached->store(true); }

	///  
	bool await_ready() { return m_coro && m_coro.done(); }

	T await_resume() {
		if (hasException()) {
			return {};
		}
		return future().get();
	}

	// 挂起当前协程，并恢复子协程
	std::coroutine_handle<> await_suspend(std::coroutine_handle<> waiter) noexcept {
		// 储存父协程的句柄
		this->m_coro.promise().continuation = waiter;
		// 返回子协程的句柄，使其恢复执行
		return this->m_coro;
	}

protected:
	std::shared_ptr<std::atomic<bool>> m_detached;
	std::coroutine_handle<promise_type> m_coro;
	std::shared_future<T> m_future; // 缓存 shared_future 对象
	std::atomic<bool> m_started = false; // 标记协程是否已经开始执行
};

template <typename T>
class RyCoroTask : public RyCoroTaskBase<T>
{
public:
	using RyCoroTaskBase<T>::RyCoroTaskBase;

	// 获取协程结果 - 会阻塞当前线程
	std::optional<T> waitForResult(std::chrono::milliseconds timeout = std::chrono::milliseconds(3000))
	{
		if (!this->m_coro) {
			return std::nullopt;
		}
		if (!this->m_coro.done()) {
			if (!this->m_started) {
				this->resume();
			}
			auto status = this->future().wait_for(timeout);
			if (status == std::future_status::timeout) {
				std::cout << "等待协程结果超时" << '\n';
				return std::nullopt;
			}
		}
		std::cout << "返回协程结果" << '\n';
		if (this->hasException()) {
			return std::nullopt;
		}
		return this->future().get(); // 这会自动抛出存储的异常
	}
};

template <>
class RyCoroTask<void> : public RyCoroTaskBase<void>
{
public:
	using RyCoroTaskBase<void>::RyCoroTaskBase;

	// 获取协程结果 - 会阻塞当前线程
	void waitForFinished(std::chrono::milliseconds timeout) const
	{
		if (!this->m_coro.done()) {
			auto status = this->m_coro.promise().get_future().wait_for(timeout);
			if (status == std::future_status::timeout) {
				std::cout << "等待协程结果超时" << '\n';
				return;
			}
		}
		std::cout << "返回协程结果" << '\n';
	}
};
//

namespace RyCoro::detail
{
	template <typename T>
	RyCoroTask<T> RyCoroTaskPromise<T>::get_return_object()
	{
		std::cout << "创建协程任务\n";
		return RyCoroTask(std::coroutine_handle<RyCoroTaskPromise>::from_promise(*this));
	}

	inline RyCoroTask<void> RyCoroTaskPromise<void>::get_return_object()
	{

		std::cout << "创建void协程任务" << '\n';
		return RyCoroTask<void>(std::coroutine_handle<RyCoroTaskPromise>::from_promise(*this));
	}
}


// 时间等待器
class RyTimeAwaitable
{
public:
	RyTimeAwaitable(std::chrono::microseconds duration)
		: m_delay(duration)
	{ }

	bool await_ready() const noexcept { return false; }

	void await_suspend(std::coroutine_handle<> h)
	{
		std::cout << "暂停协程，等待" << m_delay.count() << "微秒" << '\n';
		std::thread([this, h]() mutable {
			std::this_thread::sleep_for(m_delay);
			h.resume();
			}).detach();
	}

	void await_resume() const noexcept { std::cout << "时间等待结束，恢复协程" << '\n'; }

private:
	std::chrono::microseconds m_delay;
};

namespace RyCoro
{
	inline RyTimeAwaitable delay(std::chrono::microseconds duration)
	{
		return RyTimeAwaitable(duration);
	}
} // namespace RyCoro



template <typename T>
class FutureAwaiter
{
public:
	FutureAwaiter(std::future<T>&& future)
		: future_(std::move(future))
	{ }

	bool await_ready() const noexcept { return future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }

	void await_suspend(std::coroutine_handle<> h)
	{
		std::thread([this, h]() mutable {
			future_.wait();
			h.resume();
			}).detach();
	}

	T await_resume() { return future_.get(); }

private:
	std::future<T> future_;
};

template <typename T>
FutureAwaiter<T> operator co_await(std::future<T>&& future)
{
	return FutureAwaiter<T>(std::move(future));
}