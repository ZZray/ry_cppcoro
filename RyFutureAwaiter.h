#pragma once

#include <chrono>
#include <coroutine>
#include <thread>
#include <future>
using namespace std::literals;

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
