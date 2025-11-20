#include <gtest/gtest.h>
#include "ctq/circular_buffer.h"
#include "ctq/task_queue.h"
#include <vector>
#include <list>
#include <deque>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>

// ============================================================================
// circular_buffer Tests
// ============================================================================

TEST(CircularBufferTest, ConstructorAndCapacity) {
	ctq::circular_buffer<int> buf(5);
	EXPECT_EQ(buf.capacity(), 5);
	EXPECT_TRUE(buf.empty());
	EXPECT_EQ(buf.size(), 0);
}

TEST(CircularBufferTest, PushAndSize) {
	ctq::circular_buffer<int> buf(3);

	buf.push_back(10);
	EXPECT_EQ(buf.size(), 1);
	EXPECT_FALSE(buf.empty());

	buf.push_back(20);
	EXPECT_EQ(buf.size(), 2);

	buf.push_back(40);
	EXPECT_EQ(buf.size(), 3);
}

TEST(CircularBufferTest, NextReturnsAndPops) {
	ctq::circular_buffer<int> buf(3);

	buf.push_back(10);
	buf.emplace_back(20);
	buf.emplace_back(30);

	EXPECT_EQ(buf.next(), 10);
	EXPECT_EQ(buf.size(), 2);

	EXPECT_EQ(buf.front(), 20);
	EXPECT_EQ(buf.front(), 20);

	buf.pop_front();
	EXPECT_EQ(buf.size(), 1);

	EXPECT_EQ(buf.next(), 30);
	EXPECT_EQ(buf.size(), 0);
	EXPECT_TRUE(buf.empty());
}

TEST(CircularBufferTest, PopOperation) {
	ctq::circular_buffer<int> buf(3);

	buf.push_back(10);
	buf.push_back(20);
	buf.push_back(30);

	buf.pop_front();
	EXPECT_EQ(buf.size(), 2);

	buf.pop_front();
	EXPECT_EQ(buf.size(), 1);

	buf.pop_front();
	EXPECT_TRUE(buf.empty());
}

TEST(CircularBufferTest, CircularWrapping) {
	ctq::circular_buffer<int> buf(3);

	// Fill the buffer
	buf.push_back(10);
	buf.push_back(20);
	buf.push_back(30);

	// Remove one element
	EXPECT_EQ(buf.next(), 10);

	// Add another (should wrap around)
	buf.push_back(40);
	EXPECT_EQ(buf.size(), 3);

	// Verify order
	EXPECT_EQ(buf.next(), 20);
	EXPECT_EQ(buf.next(), 30);
	EXPECT_EQ(buf.next(), 40);
	EXPECT_TRUE(buf.empty());

}

TEST(CircularBufferTest, EmplaceOperation) {
	ctq::circular_buffer<std::string> buf(3);

	buf.emplace_back("hello");
	buf.emplace_back("world");

	EXPECT_EQ(buf.size(), 2);
	EXPECT_FALSE(buf.empty());
}

TEST(CircularBufferTest, WithComplexTypes) {
	struct Data {
		int id;
		std::string name;

		bool operator==(const Data& other) const {
			return id == other.id && name == other.name;
		}
	};

	ctq::circular_buffer<Data> buf(3);

	buf.push_back(Data{1, "first"});
	buf.push_back(Data{2, "second"});

	EXPECT_EQ(buf.size(), 2);

	Data first = buf.next();
	EXPECT_EQ(first.id, 1);
	EXPECT_EQ(first.name, "first");
}

// ============================================================================
// basic_task_queue Tests
// ============================================================================

TEST(BasicTaskQueueTest, ConstructorAndCallback) {
	std::atomic<int> counter{0};

	{
		ctq::basic_task_queue<std::vector<int>> queue(
			[&counter](int n) { counter += n; },
			std::nullopt,
			1
		);

		queue.push(5);
		queue.push(10);
		queue.emplace(15);

		// Give threads time to process
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	} // Destructor waits for threads to complete

	EXPECT_EQ(counter.load(), 30);
}

TEST(BasicTaskQueueTest, MultipleWorkers) {
	std::atomic<int> counter{0};
	const int num_items = 100;

	{
		ctq::basic_task_queue<std::vector<int>> queue(
			[&counter](int n) {
				counter += n;
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			},
			std::nullopt,
			4 // 4 workers
		);

		for (int i = 1; i <= num_items; ++i) {
			queue.push(i);
		}

		// Give threads time to process
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	// Sum of 1 to 100 = 5050
	EXPECT_EQ(counter.load(), 5050);
}

TEST(BasicTaskQueueTest, MaxElementsConstraint) {
	std::atomic<int> processed{0};

	{
		// Create queue with max 2 elements
		ctq::basic_task_queue<std::vector<int>> queue(
			[&processed](int n) {
				processed++;
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			},
			2, // max 2 elements
			1
		);

		// These should not block immediately
		queue.push(1);
		queue.push(2);

		// This might block if queue is full, but should eventually succeed
		std::thread pusher([&queue]() {
			queue.push(3);
		});

		pusher.join();
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	EXPECT_EQ(processed.load(), 3);
}

TEST(BasicTaskQueueTest, EmplaceMethod) {
	std::atomic<int> sum{0};

	{
		ctq::basic_task_queue<std::vector<int>> queue(
			[&sum](int n) { sum += n; },
			std::nullopt,
			1
		);

		queue.emplace(10);
		queue.emplace(20);
		queue.emplace(30);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	EXPECT_EQ(sum.load(), 60);
}

TEST(BasicTaskQueueTest, ProcessingOrder) {
	std::vector<int> results;
	std::mutex results_mutex;

	{
		ctq::basic_task_queue<std::vector<int>> queue(
			[&results, &results_mutex](int n) {
				std::lock_guard<std::mutex> lock(results_mutex);
				results.push_back(n);
			},
			std::nullopt,
			1 // Single worker ensures order
		);

		for (int i = 1; i <= 5; ++i) {
			queue.push(i);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	ASSERT_EQ(results.size(), 5);
	for (int i = 0; i < 5; ++i) {
		EXPECT_EQ(results[i], i + 1);
	}
}

TEST(BasicTaskQueueTest, WithComplexTypes) {
	struct Task {
		int id;
		std::string description;
	};

	std::vector<Task> processed_tasks;
	std::mutex tasks_mutex;

	{
		ctq::basic_task_queue<std::vector<Task>> queue(
			[&processed_tasks, &tasks_mutex](Task task) {
				std::lock_guard<std::mutex> lock(tasks_mutex);
				processed_tasks.push_back(task);
			},
			std::nullopt,
			1
		);

		queue.push(Task{1, "First task"});
		queue.push(Task{2, "Second task"});

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	ASSERT_EQ(processed_tasks.size(), 2);
	EXPECT_EQ(processed_tasks[0].id, 1);
	EXPECT_EQ(processed_tasks[1].id, 2);
}

// ============================================================================
// task_queue Tests (Single Type)
// ============================================================================

TEST(TaskQueueTest, SingleTypeQueue) {
	std::atomic<int> sum{0};

	{
		ctq::task_queue<std::vector, int> queue(
			[&sum](int n) { sum += n; },
			1 // 1 worker
		);

		queue.push(10);
		queue.push(20);
		queue.push(30);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	EXPECT_EQ(sum.load(), 60);
}

TEST(TaskQueueTest, SingleTypeWithMaxElements) {
	std::atomic<int> counter{0};

	{
		ctq::task_queue<std::vector, int> queue(
			[&counter](int n) {
				counter++;
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			},
			2, // max 2 elements
			1  // 1 worker
		);

		queue.push(1);
		queue.push(2);

		std::this_thread::sleep_for(std::chrono::milliseconds(150));
	}

	EXPECT_EQ(counter.load(), 2);
}

TEST(TaskQueueTest, SingleTypeMultipleWorkers) {
	std::atomic<int> counter{0};

	{
		ctq::task_queue<std::vector, int> queue(
			[&counter](int n) {
				counter += n;
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			},
			2 // 2 workers
		);

		for (int i = 1; i <= 10; ++i) {
			queue.push(i);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	EXPECT_EQ(counter.load(), 55); // Sum of 1 to 10
}

TEST(TaskQueueTest, SingleTypeEmplace) {
	std::atomic<int> sum{0};

	{
		ctq::task_queue<std::vector, int> queue(
			[&sum](int n) { sum += n; },
			1 // 1 worker
		);

		queue.emplace(10);
		queue.emplace(20);
		queue.emplace(30);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	EXPECT_EQ(sum.load(), 60);
}

TEST(TaskQueueTest, SingleTypeEmplaceWithComplexType) {
	struct Message {
		int id;
		std::string content;

		Message(int i, std::string c) : id(i), content(std::move(c)) {}
	};

	std::vector<int> ids;
	std::vector<std::string> contents;
	std::mutex results_mutex;

	{
		ctq::task_queue<std::vector, Message> queue(
			[&ids, &contents, &results_mutex](Message msg) {
				std::lock_guard<std::mutex> lock(results_mutex);
				ids.push_back(msg.id);
				contents.push_back(msg.content);
			},
			1
		);

		// Use emplace to construct Message in-place
		queue.emplace(1, "first");
		queue.emplace(2, "second");
		queue.emplace(3, "third");

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	ASSERT_EQ(ids.size(), 3);
	EXPECT_EQ(ids[0], 1);
	EXPECT_EQ(ids[1], 2);
	EXPECT_EQ(ids[2], 3);
	EXPECT_EQ(contents[0], "first");
	EXPECT_EQ(contents[1], "second");
	EXPECT_EQ(contents[2], "third");
}

// ============================================================================
// task_queue Tests (Multiple Types with Variant)
// ============================================================================

TEST(TaskQueueTest, MultiTypeQueue) {
	std::atomic<int> int_sum{0};
	std::string string_result;
	std::mutex string_mutex;

	{
		ctq::task_queue<std::vector, int, std::string> queue(
			{
				[&int_sum](int n) { int_sum += n; },
				[&string_result, &string_mutex](std::string s) {
					std::lock_guard<std::mutex> lock(string_mutex);
					string_result += s;
				}
			},
			std::nullopt,
			1
		);

		queue.push(10);
		queue.push(std::string("Hello"));
		queue.push(20);
		queue.push(std::string(" World"));

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	EXPECT_EQ(int_sum.load(), 30);
	EXPECT_EQ(string_result, "Hello World");
}

TEST(TaskQueueTest, MultiTypeWithMaxElements) {
	std::atomic<int> int_counter{0};
	std::atomic<int> string_counter{0};

	{
		ctq::task_queue<std::vector, int, std::string> queue(
			{
				[&int_counter](int n) { int_counter++; },
				[&string_counter](std::string s) { string_counter++; }
			},
			3, // max 3 elements
			1
		);

		queue.push(1);
		queue.push(std::string("a"));
		queue.push(2);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	EXPECT_EQ(int_counter.load(), 2);
	EXPECT_EQ(string_counter.load(), 1);
}

TEST(TaskQueueTest, MultiTypeMultipleWorkers) {
	std::atomic<int> total_processed{0};

	{
		ctq::task_queue<std::vector, int, std::string, double> queue(
			{
				[&total_processed](int n) { total_processed++; },
				[&total_processed](std::string s) { total_processed++; },
				[&total_processed](double d) { total_processed++; }
			},
			std::nullopt,
			3 // 3 workers
		);

		queue.push(1);
		queue.push(std::string("test"));
		queue.push(3.14);
		queue.push(2);
		queue.push(std::string("another"));
		queue.push(2.71);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	EXPECT_EQ(total_processed.load(), 6);
}

TEST(TaskQueueTest, ComplexMultiTypeScenario) {
	struct Command {
		int id;
		std::string action;
	};

	std::vector<int> int_results;
	std::vector<std::string> string_results;
	std::vector<Command> command_results;
	std::mutex results_mutex;

	{
		ctq::task_queue<std::vector, int, std::string, Command> queue(
			{
				[&int_results, &results_mutex](int n) {
					std::lock_guard<std::mutex> lock(results_mutex);
					int_results.push_back(n);
				},
				[&string_results, &results_mutex](std::string s) {
					std::lock_guard<std::mutex> lock(results_mutex);
					string_results.push_back(s);
				},
				[&command_results, &results_mutex](Command cmd) {
					std::lock_guard<std::mutex> lock(results_mutex);
					command_results.push_back(cmd);
				}
			},
			std::nullopt,
			2
		);

		queue.push(42);
		queue.push(std::string("test"));
		queue.push(Command{1, "start"});
		queue.push(100);
		queue.push(Command{2, "stop"});
		queue.push(std::string("done"));

		std::this_thread::sleep_for(std::chrono::milliseconds(150));
	}

	EXPECT_EQ(int_results.size(), 2);
	EXPECT_EQ(string_results.size(), 2);
	EXPECT_EQ(command_results.size(), 2);
}

TEST(TaskQueueTest, MultiTypeEmplaceInts) {
	std::atomic<int> int_sum{0};
	std::atomic<int> string_count{0};

	{
		ctq::task_queue<std::vector, int, std::string> queue(
			{
				[&int_sum](int n) { int_sum += n; },
				[&string_count](std::string s) { string_count++; }
			},
			std::nullopt,
			1
		);

		// Emplace int values
		queue.emplace(10);
		queue.emplace(20);
		queue.emplace(30);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	EXPECT_EQ(int_sum.load(), 60);
	EXPECT_EQ(string_count.load(), 0);
}

TEST(TaskQueueTest, MultiTypeEmplaceStrings) {
	std::atomic<int> int_count{0};
	std::string string_result;
	std::mutex string_mutex;

	{
		ctq::task_queue<std::vector, int, std::string> queue(
			{
				[&int_count](int n) { int_count++; },
				[&string_result, &string_mutex](std::string s) {
					std::lock_guard<std::mutex> lock(string_mutex);
					string_result += s;
				}
			},
			std::nullopt,
			1
		);

		// Emplace string values - constructs std::string in place
		queue.emplace("Hello");
		queue.emplace(" ");
		queue.emplace("World");

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	EXPECT_EQ(int_count.load(), 0);
	EXPECT_EQ(string_result, "Hello World");
}

TEST(TaskQueueTest, MultiTypeEmplaceMixed) {
	std::atomic<int> int_sum{0};
	std::string string_result;
	std::mutex string_mutex;

	{
		ctq::task_queue<std::vector, int, std::string> queue(
			{
				[&int_sum](int n) { int_sum += n; },
				[&string_result, &string_mutex](std::string s) {
					std::lock_guard<std::mutex> lock(string_mutex);
					string_result += s;
				}
			},
			std::nullopt,
			1
		);

		// Mix emplace and push
		queue.emplace(10);
		queue.emplace("A");
		queue.push(20);
		queue.push(std::string("B"));
		queue.emplace(30);
		queue.emplace("C");

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	EXPECT_EQ(int_sum.load(), 60);
	EXPECT_EQ(string_result, "ABC");
}

TEST(TaskQueueTest, MultiTypeEmplaceWithComplexType) {
	struct Task {
		int priority;
		std::string description;

		Task() = default;
		Task(int p, std::string d) : priority(p), description(std::move(d)) {}
	};

	std::vector<int> int_results;
	std::vector<Task> task_results;
	std::mutex results_mutex;

	{
		ctq::task_queue<std::vector, int, Task> queue(
			{
				[&int_results, &results_mutex](int n) {
					std::lock_guard<std::mutex> lock(results_mutex);
					int_results.push_back(n);
				},
				[&task_results, &results_mutex](Task t) {
					std::lock_guard<std::mutex> lock(results_mutex);
					task_results.push_back(t);
				}
			},
			std::nullopt,
			1
		);

		// Emplace constructs Task in-place
		queue.emplace(Task{1, "high priority"});
		queue.emplace(100);
		queue.emplace(Task{2, "medium priority"});
		queue.emplace(200);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	ASSERT_EQ(int_results.size(), 2);
	ASSERT_EQ(task_results.size(), 2);
	EXPECT_EQ(int_results[0], 100);
	EXPECT_EQ(int_results[1], 200);
	EXPECT_EQ(task_results[0].priority, 1);
	EXPECT_EQ(task_results[0].description, "high priority");
	EXPECT_EQ(task_results[1].priority, 2);
	EXPECT_EQ(task_results[1].description, "medium priority");
}

// ============================================================================
// Container Type Tests - std::list
// ============================================================================

TEST(ContainerTypeTest, BasicTaskQueueWithList) {
	std::atomic<int> sum{0};

	{
		ctq::basic_task_queue<std::list<int>> queue(
			[&sum](int n) { sum += n; },
			std::nullopt,
			2 // 2 workers
		);

		queue.push(10);
		queue.push(20);
		queue.push(30);
		queue.emplace(40);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	EXPECT_EQ(sum.load(), 100);
}

TEST(ContainerTypeTest, TaskQueueWithList_SingleType) {
	std::atomic<int> counter{0};

	{
		ctq::task_queue<std::list, int> queue(
			[&counter](int n) { counter += n; },
			2 // 2 workers
		);

		for (int i = 1; i <= 10; ++i) {
			queue.push(i);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(150));
	}

	EXPECT_EQ(counter.load(), 55); // Sum of 1 to 10
}

TEST(ContainerTypeTest, TaskQueueWithList_MultiType) {
	std::atomic<int> int_count{0};
	std::atomic<int> string_count{0};

	{
		ctq::task_queue<std::list, int, std::string> queue(
			{
				[&int_count](int n) { int_count++; },
				[&string_count](std::string s) { string_count++; }
			},
			std::nullopt,
			2
		);

		queue.push(1);
		queue.push(std::string("hello"));
		queue.push(2);
		queue.push(std::string("world"));
		queue.push(3);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	EXPECT_EQ(int_count.load(), 3);
	EXPECT_EQ(string_count.load(), 2);
}

TEST(ContainerTypeTest, TaskQueueWithList_BoundedQueue) {
	std::atomic<int> processed{0};

	{
		ctq::task_queue<std::list, int> queue(
			[&processed](int n) {
				processed++;
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			},
			3, // max 3 elements
			1
		);

		queue.push(1);
		queue.push(2);
		queue.push(3);

		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	EXPECT_EQ(processed.load(), 3);
}

TEST(ContainerTypeTest, TaskQueueWithList_ComplexType) {
	struct Message {
		int id;
		std::string data;
	};

	std::vector<int> ids;
	std::mutex ids_mutex;

	{
		ctq::task_queue<std::list, Message> queue(
			[&ids, &ids_mutex](Message msg) {
				std::lock_guard<std::mutex> lock(ids_mutex);
				ids.push_back(msg.id);
			},
			1
		);

		queue.push(Message{1, "first"});
		queue.push(Message{2, "second"});
		queue.push(Message{3, "third"});

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	EXPECT_EQ(ids.size(), 3);
	EXPECT_EQ(ids[0], 1);
	EXPECT_EQ(ids[1], 2);
	EXPECT_EQ(ids[2], 3);
}

// ============================================================================
// Container Type Tests - std::deque
// ============================================================================

TEST(ContainerTypeTest, BasicTaskQueueWithDeque) {
	std::atomic<int> sum{0};

	{
		ctq::basic_task_queue<std::deque<int>> queue(
			[&sum](int n) { sum += n; },
			std::nullopt,
			2 // 2 workers
		);

		queue.push(5);
		queue.push(15);
		queue.push(25);
		queue.emplace(35);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	EXPECT_EQ(sum.load(), 80);
}

TEST(ContainerTypeTest, TaskQueueWithDeque_SingleType) {
	std::atomic<int> counter{0};

	{
		ctq::task_queue<std::deque, int> queue(
			[&counter](int n) { counter += n; },
			3 // 3 workers
		);

		for (int i = 1; i <= 20; ++i) {
			queue.push(i);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(150));
	}

	EXPECT_EQ(counter.load(), 210); // Sum of 1 to 20
}

TEST(ContainerTypeTest, TaskQueueWithDeque_MultiType) {
	std::atomic<int> int_sum{0};
	std::atomic<double> double_sum{0};

	{
		ctq::task_queue<std::deque, int, double> queue(
			{
				[&int_sum](int n) { int_sum += n; },
				[&double_sum](double d) { double_sum += d; }
			},
			std::nullopt,
			2
		);

		queue.push(10);
		queue.push(3.5);
		queue.push(20);
		queue.push(2.5);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	EXPECT_EQ(int_sum.load(), 30);
	EXPECT_DOUBLE_EQ(double_sum.load(), 6.0);
}

TEST(ContainerTypeTest, TaskQueueWithDeque_BoundedQueue) {
	std::atomic<int> processed{0};

	{
		ctq::task_queue<std::deque, int> queue(
			[&processed](int n) {
				processed++;
				std::this_thread::sleep_for(std::chrono::milliseconds(30));
			},
			5, // max 5 elements
			2  // 2 workers
		);

		for (int i = 0; i < 10; ++i) {
			queue.push(i);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(300));
	}

	EXPECT_EQ(processed.load(), 10);
}

TEST(ContainerTypeTest, TaskQueueWithDeque_ThreeTypes) {
	std::atomic<int> total{0};

	{
		ctq::task_queue<std::deque, int, std::string, double> queue(
			{
				[&total](int n) { total++; },
				[&total](std::string s) { total++; },
				[&total](double d) { total++; }
			},
			std::nullopt,
			3
		);

		queue.push(1);
		queue.push(std::string("test"));
		queue.push(3.14);
		queue.push(2);
		queue.push(std::string("hello"));
		queue.push(2.71);
		queue.push(3);
		queue.push(std::string("world"));
		queue.push(1.41);

		std::this_thread::sleep_for(std::chrono::milliseconds(150));
	}

	EXPECT_EQ(total.load(), 9);
}

TEST(ContainerTypeTest, TaskQueueWithDeque_OrderPreservation) {
	std::vector<int> results;
	std::mutex results_mutex;

	{
		ctq::task_queue<std::deque, int> queue(
			[&results, &results_mutex](int n) {
				std::lock_guard<std::mutex> lock(results_mutex);
				results.push_back(n);
			},
			std::nullopt,
			1 // Single worker to preserve order
		);

		for (int i = 1; i <= 10; ++i) {
			queue.push(i);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(150));
	}

	ASSERT_EQ(results.size(), 10);
	for (int i = 0; i < 10; ++i) {
		EXPECT_EQ(results[i], i + 1);
	}
}

// ============================================================================
// Cross-Container Comparison Tests
// ============================================================================

TEST(ContainerTypeTest, AllContainersProduceSameResults) {
	std::atomic<int> vector_sum{0};
	std::atomic<int> list_sum{0};
	std::atomic<int> deque_sum{0};

	auto process_fn = [](std::atomic<int>& sum) {
		return [&sum](int n) { sum += n; };
	};

	{
		ctq::task_queue<std::vector, int> vector_queue(process_fn(vector_sum), 2);
		ctq::task_queue<std::list, int> list_queue(process_fn(list_sum), 2);
		ctq::task_queue<std::deque, int> deque_queue(process_fn(deque_sum), 2);

		for (int i = 1; i <= 50; ++i) {
			vector_queue.push(i);
			list_queue.push(i);
			deque_queue.push(i);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	// All should produce the same sum (1+2+...+50 = 1275)
	EXPECT_EQ(vector_sum.load(), 1275);
	EXPECT_EQ(list_sum.load(), 1275);
	EXPECT_EQ(deque_sum.load(), 1275);
}

TEST(ContainerTypeTest, AllContainersMultiType) {
	std::atomic<int> vector_count{0};
	std::atomic<int> list_count{0};
	std::atomic<int> deque_count{0};

	auto callbacks = [](std::atomic<int>& counter) {
		return std::make_tuple(
			[&counter](int n) { counter++; },
			[&counter](std::string s) { counter++; }
		);
	};

	{
		ctq::task_queue<std::vector, int, std::string> vector_queue(callbacks(vector_count), 2);
		ctq::task_queue<std::list, int, std::string> list_queue(callbacks(list_count), 2);
		ctq::task_queue<std::deque, int, std::string> deque_queue(callbacks(deque_count), 2);

		for (int i = 0; i < 10; ++i) {
			vector_queue.push(i);
			list_queue.push(i);
			deque_queue.push(i);
		}

		for (int i = 0; i < 10; ++i) {
			vector_queue.push(std::string("msg"));
			list_queue.push(std::string("msg"));
			deque_queue.push(std::string("msg"));
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	// All should process 20 items
	EXPECT_EQ(vector_count.load(), 20);
	EXPECT_EQ(list_count.load(), 20);
	EXPECT_EQ(deque_count.load(), 20);
}

// ============================================================================
// Container Type Tests - circular_buffer
// ============================================================================

TEST(ContainerTypeTest, BasicTaskQueueWithCircularBuffer) {
	std::atomic<int> sum{0};

	{
		ctq::basic_task_queue<ctq::circular_buffer<int>> queue(
			[&sum](int n) { sum += n; },
			10, // circular_buffer size
			2   // 2 workers
		);

		queue.push(5);
		queue.push(15);
		queue.push(25);
		queue.emplace(35);

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	EXPECT_EQ(sum.load(), 80);
}

TEST(ContainerTypeTest, CircularBufferAsQueue) {
	std::atomic<int> counter{0};

	{
		ctq::basic_task_queue<ctq::circular_buffer<int>> queue(
			[&counter](int n) {
				counter += n;
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			},
			20, // circular_buffer size
			2   // 2 workers
		);

		for (int i = 1; i <= 10; ++i) {
			queue.push(i);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(150));
	}

	EXPECT_EQ(counter.load(), 55); // Sum of 1 to 10
}

TEST(ContainerTypeTest, CircularBufferBoundedBehavior) {
	std::atomic<int> processed{0};

	{
		// Circular buffer with capacity 5
		ctq::basic_task_queue<ctq::circular_buffer<int>> queue(
			[&processed](int n) {
				processed++;
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			},
			5, // small circular_buffer size
			1  // 1 worker
		);

		// Add items
		queue.push(1);
		queue.push(2);
		queue.push(3);

		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	EXPECT_EQ(processed.load(), 3);
}

TEST(ContainerTypeTest, CircularBufferWithComplexTypes) {
	struct Task {
		int id;
		std::string description;
	};

	std::vector<int> task_ids;
	std::mutex ids_mutex;

	{
		ctq::basic_task_queue<ctq::circular_buffer<Task>> queue(
			[&task_ids, &ids_mutex](Task task) {
				std::lock_guard<std::mutex> lock(ids_mutex);
				task_ids.push_back(task.id);
			},
			10, // circular_buffer size
			1   // 1 worker for order preservation
		);

		queue.push(Task{1, "Task One"});
		queue.push(Task{2, "Task Two"});
		queue.push(Task{3, "Task Three"});

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	ASSERT_EQ(task_ids.size(), 3);
	EXPECT_EQ(task_ids[0], 1);
	EXPECT_EQ(task_ids[1], 2);
	EXPECT_EQ(task_ids[2], 3);
}

TEST(ContainerTypeTest, CircularBufferMultipleWorkers) {
	std::atomic<int> total_processed{0};

	{
		ctq::basic_task_queue<ctq::circular_buffer<int>> queue(
			[&total_processed](int n) {
				total_processed += n;
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			},
			50, // large circular_buffer
			3   // 3 workers
		);

		for (int i = 1; i <= 20; ++i) {
			queue.push(i);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	EXPECT_EQ(total_processed.load(), 210); // Sum of 1 to 20
}

TEST(ContainerTypeTest, CircularBufferOrderPreservation) {
	std::vector<int> results;
	std::mutex results_mutex;

	{
		ctq::basic_task_queue<ctq::circular_buffer<int>> queue(
			[&results, &results_mutex](int n) {
				std::lock_guard<std::mutex> lock(results_mutex);
				results.push_back(n);
			},
			15, // circular_buffer size
			1   // Single worker to preserve order
		);

		for (int i = 1; i <= 10; ++i) {
			queue.push(i);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	ASSERT_EQ(results.size(), 10);
	for (int i = 0; i < 10; ++i) {
		EXPECT_EQ(results[i], i + 1);
	}
}

TEST(ContainerTypeTest, CircularBufferVsOtherContainers) {
	std::atomic<int> circular_sum{0};
	std::atomic<int> vector_sum{0};
	std::atomic<int> list_sum{0};

	auto process_fn = [](std::atomic<int>& sum) {
		return [&sum](int n) { sum += n; };
	};

	{
		ctq::basic_task_queue<ctq::circular_buffer<int>> circular_queue(process_fn(circular_sum), 100, 2);
		ctq::basic_task_queue<std::vector<int>> vector_queue(process_fn(vector_sum), std::nullopt, 2);
		ctq::basic_task_queue<std::list<int>> list_queue(process_fn(list_sum), std::nullopt, 2);

		for (int i = 1; i <= 30; ++i) {
			circular_queue.push(i);
			vector_queue.push(i);
			list_queue.push(i);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	// All should produce the same sum (1+2+...+30 = 465)
	EXPECT_EQ(circular_sum.load(), 465);
	EXPECT_EQ(vector_sum.load(), 465);
	EXPECT_EQ(list_sum.load(), 465);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
