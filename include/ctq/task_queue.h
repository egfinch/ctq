#pragma once

#include <stop_token>
#include <variant>
#include <type_traits>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <optional>
#include <vector>
#include <thread>
#include <utility>

#include <ctq/circular_buffer.h>

namespace ctq {

	// Helper namespace for internal implementations
namespace detail {

	/** @brief This is helper type, an adapter to provide a uniform interface for different container types
	 * This struct template adapts different container types to provide a uniform interface
	 * for use in the task queue implementation. It adds support for maximum size and pop_front operation
	 * where applicable.
	 * @tparam Container The type of the underlying container.
	 */
template<typename Container>
struct queue_adapter : Container{
	std::optional<size_t> max_elements_;

	explicit queue_adapter(std::optional<size_t> max_elements) : max_elements_(max_elements) {
	}

	std::optional<size_t> max_elements() const {
		return max_elements_;
	}
};

template<typename T>
struct queue_adapter<std::vector<T>> : std::vector<T>
{
	std::optional<size_t> max_elements_;

	explicit queue_adapter(std::optional<size_t> max_elements) : max_elements_(max_elements) {
		if (max_elements_) {
			this->reserve(*max_elements_);
		}
	}

	std::optional<size_t> max_elements() const {
		return max_elements_;
	}

	void pop_front() {
		this->erase(this->begin());
	}
};

template<typename T>
struct queue_adapter<circular_buffer<T>> : circular_buffer<T>
{
	explicit queue_adapter(std::optional<size_t> max_elements) : circular_buffer<T>(*max_elements) {}

	std::optional<size_t> max_elements() const {
		return this->capacity();
	}

};

} // namespace detail


// Forward declaration of basic_task_queue
template<typename Container>
struct basic_task_queue;

/** @brief Task queue type definition
 *
 * This struct defines a task queue that can hold messages of multiple types.
 * It uses std::variant to hold any of the specified types.
 * Example: ctq::task_queue<std::vector, int, std::string> for a task queue holding either integers or strings.
 *          In this example the underlying container is std::vector<std::variant<int, std::string>>.
 *
 * @tparam Container A template template parameter representing the container type (e.g., std::vector, std::list).
 * @tparam Ts A variadic list of types that the task queue can hold.
 */
template<template<typename... U> class Container, typename... Ts>
struct task_queue {
    using type = std::variant<Ts...>;
    using queue = Container<type>;
	using callbacks = std::tuple<std::function<void(Ts)>...>;

	/** @brief Constructor for task_queue
	 *
	 * This constructor initializes the task queue with a set of callbacks for each type
	 * and optional maximum elements and number of worker threads.
	 *
	 * @param cb A tuple of callback functions, one for each type in Ts.
	 * @param max_elements An optional maximum number of elements in the queue.
	 * @param workers The number of worker threads to process the queue.
	 */
	task_queue(callbacks cb, std::optional<size_t> max_elements, size_t workers = 1)
	{
		basic_ = std::make_unique<basic_task_queue<queue>>(
			[cb](type item) {
				std::visit([cb](auto&& arg) {
					using T = std::decay_t<decltype(arg)>;
					auto& c = std::get<std::function<void(T)>>(cb);
					c(std::forward<decltype(arg)>(arg));
					}, item);
			}, max_elements, workers);
	}

	explicit task_queue(callbacks cb, size_t workers = 1)
		:task_queue(cb, std::nullopt, workers)
	{ }

	~task_queue() = default;

	/** @brief Add an item to the task queue
	 *
	 * This method adds an item to the task queue.
	 *
	 * @param item The item to be added to the queue.
	 */
	void push(type item) {
		basic_->push(std::move(item));
	}

	/** @brief Emplace an item into the task queue. Same as push but constructs in place. */
	template<typename... Args>
	void emplace(Args&&... args) {
		basic_->emplace(std::forward<Args>(args)...);
	}

	/** This method provides access to the underlying queue. The provided function is executed 
	 *  with a lock held on the queue to ensure thread safety.
	 */
	void access_queue(std::function<void(queue&)> f) {
		basic_->access_queue(f);
	}

private:
	std::unique_ptr<basic_task_queue<queue>> basic_;
};

/** @brief Task queue type definition for a single type
 *
 * This struct defines a task queue that holds messages of a single type.
 * Example: ctq::task_queue<std::vector, int> for a task queue holding integers. 
 *          In this example the underlying container is std::vector<int>.
 *
 * @tparam Container A template template parameter representing the container type (e.g., std::vector, std::list).
 * @tparam T The type that the task queue will hold.
 */
template<template<typename T, typename... U> class Container, typename T>
struct task_queue<Container, T> {
    using type = T;
    using queue = Container<type>;
	using callback = std::function<void(T)>;

	task_queue(callback cb, std::optional<size_t> max_elements, size_t workers = 1)
	{
		basic_ = std::make_unique<basic_task_queue<queue>>(
			[cb](type item) { cb(std::move(item)); }, max_elements, workers);
	}
	explicit task_queue(callback cb, size_t workers = 1)
		:task_queue(cb, std::nullopt, workers)
	{ }

	~task_queue() = default;

	void push(type item) {
		basic_->push(std::move(item));
	}

	template<typename... Args>
	void emplace(Args&&... args) {
		basic_->emplace(std::forward<Args>(args)...);
	}

	/** This method provides access to the underlying queue. The provided function is executed 
	 *  with a lock held on the queue to ensure thread safety.
	 */
	void access_queue(std::function<void(queue&)> f) {
		basic_->access_queue(f);
	}

private:
	std::unique_ptr<basic_task_queue<queue>> basic_;
};

/** @brief A simple task queue implementation
 *
 * This struct implements a simple task queue that processes items of a specified type using a provided callback function.
 * It does the actual work of managing the queue and worker threads. Multiple worker threads process items concurrently.
 *
 * @tparam Container The type of the underlying queue container.
 */
template<typename Container>
struct basic_task_queue {
	// adapt to the underlying container
	using queue = detail::queue_adapter<Container>;
	using type = typename queue::value_type;
	using callback = std::function<void(type)>;

	basic_task_queue(callback cb, std::optional<size_t> max_elements, size_t workers = 1)
		: cb_(std::move(cb))
		  ,q_(max_elements)
	{
		for (size_t i = 0; i < workers; ++i) {
			workers_.emplace_back([this](std::stop_token st) {
				while (!st.stop_requested()) {
					std::optional<type> item;
					{
						std::unique_lock lock(mutex_);
						if (!cv_.wait(lock, st, [this]() { return !q_.empty(); })) {
							return; // stop requested
						}
						item = std::move(q_.front());
						q_.pop_front();
						if (q_.max_elements().has_value()) {
							cv_.notify_all();
						}
					}
					cb_(std::move(*item));
				}
			});
		}
	}

	basic_task_queue(const basic_task_queue&) = delete;
	basic_task_queue(basic_task_queue&&) = delete;
	const basic_task_queue& operator=(const basic_task_queue&) = delete;

	~basic_task_queue() = default;

	/** @brief Add an item to the task queue
	 *
	 * This method adds an item to the task queue. If the queue has a maximum size and is full,
	 * the method will block until space becomes available.
	 *
	 * @param item The item to be added to the queue.
	 */
	void push(type item) {
		{
			std::unique_lock lock(mutex_);
			if (q_.max_elements().has_value()) {
				cv_.wait(lock, [this]() { return q_.size() < q_.max_elements().value(); });
			}
			q_.push_back(std::move(item));
		}
		cv_.notify_one();
	}

	/** @brief Emplace an item into the task queue. Same as push but constructs in place. */
	template<typename... Args>
	void emplace(Args&&... args) {
		{
			std::unique_lock lock(mutex_);
			if (q_.max_elements().has_value()) {
				cv_.wait(lock, [this]() { return q_.size() < q_.max_elements().value(); });
			}
			q_.emplace_back(std::forward<Args>(args)...);
		}
		cv_.notify_one();
	}

	/** @brief Access the underlying queue
	 *
	 * This method provides access to the underlying queue. The provided function is executed 
	 * with a lock held on the queue to ensure thread safety.
	 *
	 * @param f A function that takes a reference to the queue and performs operations on it.
	 */
	void access_queue(std::function<void(queue&)> f) {
		std::unique_lock lock(mutex_);
		f(q_);
	}

private:
	callback cb_;
	queue q_;
	std::mutex mutex_;
	std::condition_variable_any cv_;
	std::vector<std::jthread> workers_;
};

}
