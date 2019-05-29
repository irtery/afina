#include <afina/concurrency/Executor.h>

namespace Afina {
namespace Concurrency {

Executor::Executor(int low_watermark, int high_watermark, int max_queue_size, std::chrono::milliseconds idle_time)
    : _low_watermark(low_watermark), _high_watermark(high_watermark), _max_queue_size(max_queue_size),
      _idle_time(idle_time), _active_workers(0), _free_workers(0), _state(Executor::State::kRun) {
    std::unique_lock<std::mutex> lock(_mutex);
    for (int i = 0; i < low_watermark; ++i) {
        std::thread(&perform, this).detach();
        ++_free_workers;
    }
}

void Executor::Stop(bool await) {
    std::unique_lock<std::mutex> lock(_mutex);
    _state = Executor::State::kStopping;
    _empty_condition.notify_all();

    if (_tasks.size() == 0) {
        _state = Executor::State::kStopped;
        return;
    }

    if (await) {
        _stop_condition.wait(lock, [this]() { return _active_workers + _free_workers == 0; });
        _state = Executor::State::kStopped;
    }
}

void perform(Executor *executor) {
    while (executor->_state == Executor::State::kRun || !executor->_tasks.empty()) {
        std::function<void()> task;
        auto now = std::chrono::system_clock::now();
        {
            std::unique_lock<std::mutex> lock(executor->_mutex);
            while ((executor->_tasks.empty()) && (executor->_state == Executor::State::kRun)) {
                if (executor->_empty_condition.wait_until(lock, now + executor->_idle_time) ==
                    std::cv_status::timeout) {
                    if (executor->_active_workers + executor->_free_workers <= executor->_low_watermark) {
                        executor->_empty_condition.wait(lock);
                    } else {
                        break;
                    }
                }
            }
            if (executor->_tasks.empty()) {
                --executor->_free_workers;
                return;
            }
            --executor->_free_workers;
            ++executor->_active_workers;

            task = executor->_tasks.back();
            executor->_tasks.pop_back();
        }
        task();
        {
            std::unique_lock<std::mutex> lock(executor->_mutex);
            --executor->_active_workers;
            ++executor->_free_workers;
        }
    }
    {
        std::unique_lock<std::mutex> lock(executor->_mutex);
        --executor->_free_workers;
        if ((executor->_tasks.size() == 0) && (executor->_state == Executor::State::kStopping)) {
            executor->_stop_condition.notify_all();
        }
    }
}

} // namespace Concurrency
} // namespace Afina
