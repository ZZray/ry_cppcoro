/**
 * @author rayzhang
 * @brief
 * @date 2024年07月16日
 */
#pragma once
#include "RyCoroScheduler.h"

 // 时间等待器
class RyTimeAwaitable {
public:
	RyTimeAwaitable(RyCoroScheduler* sched, std::chrono::microseconds duration)
		: sched_(sched), duration_(duration) {}

	bool await_ready() const noexcept { return false; }

	void await_suspend(RyCoroSchedulerHandle h)
	{
		std::cout << "暂停协程，等待" << duration_.count() << "微秒" << std::endl;
		sched_->schedule(h, duration_);
	}


	void await_resume() const noexcept {
		std::cout << "时间等待结束，恢复协程" << std::endl;
	}

private:
	RyCoroScheduler* sched_;
	std::chrono::microseconds duration_;
};
namespace RyCoro {
	inline RyTimeAwaitable delay(std::chrono::microseconds duration) {
		return RyTimeAwaitable(&RyCoroScheduler::getInstance(), duration);
	}
}