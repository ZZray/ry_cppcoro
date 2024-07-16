/**
 * @author rayzhang
 * @brief
 * @date 2024年07月16日
 */
#pragma once

#include <chrono>
#include <coroutine>
#include <deque>
#include <iostream>
#include <thread>
#include <atomic>
#include <future>
#include <mutex>

 // 定义协程的等待状态
enum class RyCoroAwaitState
{
	ScheduleNextFrame, // 在下一帧调度
	ScheduleImmediately, // 立即调度
	NoSchedule // 不调度
};

// 调度器统计信息
struct RyCoroSchedulerStats
{
	std::atomic<size_t> pendingTaskCount{ 0 }; // 待处理任务数量
};

// 调度选项
struct RyCoroScheduleOptions
{
	bool immediate = true; // 是否立即执行
};

class RyCoroScheduler;
using RyCoroSchedulerHandle = std::coroutine_handle<>;


class RyCoroScheduler
{
public:
	using HandleT = RyCoroSchedulerHandle;

	// 获取调度器单例
	static RyCoroScheduler& getInstance()
	{
		static RyCoroScheduler instance;
		return instance;
	}

	~RyCoroScheduler();

	// 调度协程
	void scheduleNextFrame(HandleT coro)
	{
		scheduleImpl(coro, RyCoroAwaitState::ScheduleNextFrame);
	}

	void scheduleImmediately(HandleT coro)
	{
		scheduleImpl(coro, RyCoroAwaitState::ScheduleImmediately);
	}

	void schedule(HandleT handle, std::chrono::microseconds delay = std::chrono::microseconds(0));


	const RyCoroSchedulerStats& getStats() const
	{
		return m_stats;
	}

	// 运行调度器
	void run();

	// 停止调度器
	void stop()
	{
		{
			std::lock_guard<std::mutex> lock(m_queueMutex);
			std::cout << "Stopping the scheduler.\n";
			m_stop = true;
		}
		m_queueCv.notify_all();
	}

	// 等待所有协程完成
	void wait()
	{
		std::unique_lock<std::mutex> lock(m_queueMutex);
		std::cout << "Waiting for all coroutines to complete.\n";
		m_queueCv.wait(lock, [this] { return m_stats.pendingTaskCount == 0; });
		std::cout << "All coroutines have completed.\n";
	}

private:
	RyCoroScheduler() = default;
	RyCoroScheduler(const RyCoroScheduler&) = delete;
	RyCoroScheduler& operator=(const RyCoroScheduler&) = delete;

	// 入栈
	void scheduleImpl(HandleT coro, RyCoroAwaitState state = RyCoroAwaitState::ScheduleNextFrame);


	// members
	std::deque<HandleT> m_readyQueue;
	RyCoroSchedulerStats m_stats;
	std::mutex m_queueMutex;
	std::condition_variable m_queueCv;
	std::atomic<bool> m_stop{ false };
};

inline RyCoroScheduler::~RyCoroScheduler()
{
	stop();
	wait();
}

inline void RyCoroScheduler::schedule(HandleT handle, std::chrono::microseconds delay)
{
	std::cout << "调度协程任务" << '\n';
	if (delay.count() != 0)
	{
		std::thread([this, handle, delay]()
			{
				std::cout << "开始延迟调度，等待" << delay.count() << "微秒" << '\n';
				std::this_thread::sleep_for(delay);
				std::cout << "延迟结束，调度协程" << '\n';
				this->scheduleNextFrame(handle);
			}).detach();
	}
	else
	{
		scheduleNextFrame(handle);
	}
}

inline void RyCoroScheduler::run()
{
	std::cout << "开始运行调度器" << '\n';
	m_stop = false;
	while (!m_stop)
	{
		std::coroutine_handle<> handle;
		{
			std::unique_lock<std::mutex> lock(m_queueMutex);
			m_queueCv.wait(lock, [this] { return !m_readyQueue.empty() || m_stop; });
			if (m_readyQueue.empty())
			{
				std::cout << "No coroutines to run, continuing the loop.\n";
				continue;
			}
			handle = m_readyQueue.front();
			m_readyQueue.pop_front();
		}
		std::cout << "Resuming coroutine.\n";
		if (!handle.done())
		{
			handle.resume();
		}
		else
		{
			std::cout << "Coroutine is done, handle destroyed.\n";
			handle.destroy();
		}
		{
			std::lock_guard<std::mutex> lock(m_queueMutex);
			--m_stats.pendingTaskCount;
			if (m_stats.pendingTaskCount == 0 && m_stop)
			{
				std::cout << "All coroutines finished, notifying all waiting threads.\n";
			}
			m_queueCv.notify_all();
		}
	}
	m_stop = true;
	std::cout << "调度器运行结束" << '\n';
}

inline void RyCoroScheduler::scheduleImpl(HandleT coro, RyCoroAwaitState state)
{
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

	std::lock_guard<std::mutex> lock(m_queueMutex);
	switch (state)
	{
	case RyCoroAwaitState::ScheduleNextFrame:
		std::cout << "将协程加入下一帧队列" << '\n';
		m_readyQueue.emplace_back(coro);
		break;
	case RyCoroAwaitState::ScheduleImmediately:
		std::cout << "将协程加入立即执行队列" << '\n';
		m_readyQueue.emplace_front(coro);
		break;
	case RyCoroAwaitState::NoSchedule:
	default:
		std::cout << "协程不被调度" << '\n';
		return;
	}
	++m_stats.pendingTaskCount;
	std::cout << "当前待处理任务数: " << m_stats.pendingTaskCount << '\n';
}
