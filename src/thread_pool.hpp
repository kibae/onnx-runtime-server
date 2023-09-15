#ifndef ONNX_RUNTIME_SERVER_THREAD_POOL_HPP
#define ONNX_RUNTIME_SERVER_THREAD_POOL_HPP

#include <iostream>
#include <queue>
#include <thread>

namespace onnxruntime_server {
	class builtin_thread_pool {
	  public:
		explicit builtin_thread_pool(long threads) {
			for (long i = 0; i < threads; ++i) {
				workers.emplace_back([this] {
					while (true) {
						std::function<void()> task;

						{
							std::unique_lock<std::mutex> lock(this->queue_mutex);
							this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });

							if (this->stop && this->tasks.empty()) {
								return;
							}

							task = std::move(this->tasks.front());
							this->tasks.pop();
						}

						task();
					}
				});
			}
		}

		~builtin_thread_pool() {
			stop = true;
			condition.notify_all();

			for (std::thread &worker : workers) {
				worker.join();
			}
		}

		template <class F, class... Args> void enqueue(F &&f, Args &&...args) {
			{
				std::unique_lock<std::mutex> lock(queue_mutex);
				tasks.emplace([task = std::bind(std::forward<F>(f), std::forward<Args>(args)...)]() { task(); });
			}

			condition.notify_one();
		}

		void flush() {
			std::unique_lock<std::mutex> lock(queue_mutex);
			tasks = std::queue<std::function<void()>>();
		}

	  protected:
		std::vector<std::thread> workers;
		std::queue<std::function<void()>> tasks;

		std::mutex queue_mutex;
		std::condition_variable condition;
		std::atomic_bool stop = ATOMIC_VAR_INIT(false);
	};
} // namespace onnxruntime_server

#endif // ONNX_RUNTIME_SERVER_THREAD_POOL_HPP
