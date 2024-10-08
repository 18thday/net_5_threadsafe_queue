#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

std::mutex io_m;

void function_first() {
    std::lock_guard<std::mutex> lg(io_m);
    std::cout << "Thread id : " << std::this_thread::get_id() << " : "
              << __FUNCTION__ << std::endl;
}

void function_second() {
    std::lock_guard<std::mutex> lg(io_m);
    std::cout << "Thread id : " << std::this_thread::get_id() << " : "
              << __FUNCTION__ << std::endl;
}

template <typename Func>
class safe_queue {
public:
    void push(Func task) {
        std::lock_guard<std::mutex> lg(m_);
        tasks_.push(task);
        cv_.notify_one();
    }
//    std::shared_ptr<Func> pop() {
//        std::unique_lock<std::mutex> lg(m_);
//        cv_.wait(lg, [this](){ return !tasks_.empty();});
//        std::shared_ptr<Func> func(std::make_shared(tasks_.front()));
//        tasks_.pop();
//        return func;
//    }
    bool pop(Func& func) {
        std::unique_lock<std::mutex> lg(m_);
        if (cv_.wait_for(lg, std::chrono::seconds(1), [this](){ return !tasks_.empty();})) {
            func = tasks_.front();
            tasks_.pop();
            return true;
        }
        return false;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lg(m_);
        return tasks_.empty();
    }

private:
    std::queue<Func> tasks_;
    mutable std::mutex m_;
    std::condition_variable cv_;
};

class thread_pool {
public:
    explicit thread_pool() {
        size_t thread_count = std::thread::hardware_concurrency();
        std::cerr << thread_count << std::endl;
        thread_pool_.reserve(thread_count > 0 ? thread_count : 2);
        for (size_t i = 0; i < thread_count; ++i) {
            thread_pool_.push_back(std::thread(&thread_pool::work, this));
        }
    }
    ~thread_pool() {
        is_finished = true;
        for (auto& t : thread_pool_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    void work() {
        while (!is_finished || !safe_queue_.empty()) {
            std::function<void()> func;
            if (safe_queue_.pop(func)) {
                func();
            } else {
                std::this_thread::yield();
            }
        }
    }
    void submit(std::function<void()> f) {
        safe_queue_.push(f);
    }

private:
    std::atomic<bool> is_finished = false;
    safe_queue<std::function<void()>> safe_queue_;
    std::vector<std::thread> thread_pool_;

};

int main() {
    thread_pool tp;

    size_t task_count = 50;
    for (size_t i = 0; i < task_count; ++i) {
        std::cout << i << std::endl;
        tp.submit(function_first);
        tp.submit(function_second);
        std::this_thread::sleep_for(std::chrono::milliseconds{1000});
    }
    return 0;
}
