# CTQ - Concurrent Task Queue

Straightforward and flexible concurrent task queue library in modern >= C++20. It's thread-safe with support for single or multiple message types, customizable container backends, and configurable worker thread pools. It's header-only library.

## Overview

CTQ provides a lightweight concurrent task queue implementation that allows you to define queues with:
- Custom container types (e.g., `std::vector`, `std::list`, custom `circular_buffer`)
- Single or multiple message types using `std::variant`
- Configurable number of worker threads
- Optional queue size limits with blocking behavior

## Requirements

- C++20 compatible compiler
- CMake 3.15 or higher (for building tests)
- Google Test (for running unit tests)

## Installation

```bash
git clone https://github.com/egfinch/ctq.git
cd ctq
mkdir build
cd build
cmake ..
make install
```

Now you can include CTQ in your project by adding the following to your CMakeList.txt:

```cmake
find_package(ctq REQUIRED)
target_link_libraries(your_target PRIVATE ctq::ctq)
```

Since CTQ is header-only, you can simply copy the `include/ctq` directory to your project or add it to your include path:

```bash
cp -r include/ctq /path/to/your/project/include/
```

To build and run the tests, in the `build` (after running cmake, above) execute:

```bash
ctest
```

## Core Components

### 1. `circular_buffer<T>`

A simple circular buffer implementation with:
- Fixed capacity
- FIFO semantics
- Methods: `push_back()`, `pop_front()`, `emplace_back()`
- Additional `next()` method (pop and return)
- `empty()`, `size()`, `capacity()`, and `front()` queries
- Can be used as underlying container for `basic_task_queue`

### 2. `basic_task_queue<Container>`

The core task queue implementation that:
- Manages a pool of worker threads using `std::jthread`
- Processes items from the queue using a provided callback function
- Supports optional maximum queue size with blocking behavior
- Automatically stops workers on destruction

### 3. `task_queue<Container, Ts...>`

A high-level wrapper that:
- Supports single or multiple message types
- Uses `std::variant` for multiple types
- Maps each type to its corresponding callback function
- Provides a simple `push()` interface
- Works with `std::vector`, `std::list`, `std::deque`, and `circular_buffer`

## Usage

### Basic Example - Single Type Queue

```cpp
#include "ctq/task_queue.h"
#include <vector>
#include <iostream>

int main() {
    // Create a task queue with a single callback for integers
    ctq::task_queue<std::vector, int> queue(
        [](int n) {
            std::cout << "Processing: " << n << std::endl;
        },
        2 // 2 worker threads
    );

    // Add tasks to the queue
    queue.push(42);
    queue.push(100);
    queue.push(7);

    // Workers process tasks in the background
    // Queue destructor will wait for all tasks to complete
}
```

### Multi-Type Queue with Variant

```cpp
#include "ctq/task_queue.h"
#include <vector>
#include <string>
#include <iostream>

int main() {
    // Create a queue that handles multiple types
    ctq::task_queue<std::vector, int, std::string, double> queue(
        {
            [](int n) { std::cout << "Int: " << n << std::endl; },
            [](std::string s) { std::cout << "String: " << s << std::endl; },
            [](double d) { std::cout << "Double: " << d << std::endl; }
        },
        std::nullopt, // No max size
        3 // 3 worker threads
    );

    queue.push(42);
    queue.push(std::string("hello"));
    queue.push(3.14);
    queue.push(100);
}
```

### Bounded Queue with Size Limit

```cpp
#include "ctq/task_queue.h"
#include <vector>

int main() {
    // Create a queue with maximum 10 items
    // Pushing to a full queue will block until space is available
    ctq::task_queue<std::vector, int> queue(
        [](int n) { /* process */ },
        10, // max 10 items
        2   // 2 workers
    );

    for (int i = 0; i < 100; ++i) {
        queue.push(i); // May block if queue is full
    }
}
```

### Using basic_task_queue Directly

```cpp
#include "ctq/task_queue.h"
#include <vector>

int main() {
    ctq::basic_task_queue<std::vector<int>> queue(
        [](int item) {
            // Process item
        },
        std::nullopt, // No max size
        4 // 4 worker threads
    );

    queue.push(1);
    queue.emplace(2);
    queue.push(3);
}
```

### Using Different Container Types

The library supports multiple container types as the underlying queue storage:

#### With std::list
```cpp
#include "ctq/task_queue.h"
#include <list>

ctq::task_queue<std::list, int> queue(
    [](int n) { /* process */ },
    2 // workers
);

queue.push(42);
```

#### With std::deque
```cpp
#include "ctq/task_queue.h"
#include <deque>

ctq::task_queue<std::deque, int> queue(
    [](int n) { /* process */ },
    3 // workers
);

queue.push(42);
```

#### With circular_buffer
```cpp
#include "ctq/task_queue.h"

ctq::basic_task_queue<ctq::circular_buffer<int>> queue(
    [](int n) { /* process */ },
    100, // circular_buffer capacity
    2    // workers
);

queue.push(42);
```

### Thread-Safe Queue Access with access_queue

The `access_queue` method provides thread-safe access to the underlying queue container. This is useful when you need to inspect or manipulate the queue directly, such as checking its size, clearing it, or performing custom operations. The provided function is executed with the queue's internal mutex locked, ensuring thread safety.

```cpp
#include "ctq/task_queue.h"
#include <vector>
#include <iostream>

int main() {
    ctq::task_queue<std::vector, int> queue(
        [](int n) {
            // Process items slowly
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        },
        2 // workers
    );

    // Add some items
    for (int i = 0; i < 10; ++i) {
        queue.push(i);
    }

    // Check queue size in a thread-safe way
    queue.access_queue([](auto& q) {
        std::cout << "Current queue size: " << q.size() << std::endl;
    });

    // Clear the queue if needed
    queue.access_queue([](auto& q) {
        q.clear();
        std::cout << "Queue cleared. New size: " << q.size() << std::endl;
    });
}
```

**Important:** The function passed to `access_queue` should be quick to execute, as it holds the queue's mutex and blocks all queue operations while running.

## Building and Testing

The project uses CMake and Google Test for building and testing:

```bash
mkdir build
cd build
cmake ..
make
```

### Running Tests

To run the comprehensive unit test suite:

```bash
cd build
./ctq_test
```

Or using CTest:

```bash
cd build
ctest --verbose
```

## Test Coverage

The unit test suite (`test/ctq_test.cpp`) includes comprehensive tests for all components:

### circular_buffer Tests (7 tests)
- Constructor and capacity verification
- `push_back()` and size tracking
- `next()` method (pop and return)
- `pop_front()` operation
- Circular wrapping behavior
- `emplace_back()` operation
- Complex type support
- `front()` method verification

### basic_task_queue Tests (6 tests)
- Basic callback execution
- Multiple worker threads
- Queue size constraints and blocking behavior
- `emplace()` method
- Processing order verification
- Complex type handling

### task_queue Tests (7 tests)
- Single type queue operations
- Multiple type queue with `std::variant`
- Max element constraints
- Multiple worker threads
- Complex multi-type scenarios
- Callback routing for different types

### Container Type Tests
- **std::list**: Basic operations, single/multi-type queues, bounded queues, complex types
- **std::deque tests**: Basic operations, single/multi-type queues, bounded queues, order preservation
- **circular_buffer tests**: Basic operations, multiple workers, bounded behavior, complex types, order preservation
- **Cross-container tests**: Verification that all containers produce identical results

The tests verify:
- Thread safety with atomic counters and mutexes
- Correct callback invocation
- FIFO ordering (with single worker)
- Concurrent processing with multiple workers
- Blocking behavior on bounded queues
- Proper cleanup on destruction

## Project Structure

```
ctq/
├── include/
│   └── ctq/
│       ├── circular_buffer.h   # Circular buffer implementation
│       └── task_queue.h        # Task queue implementations
├── test/
│   └── ctq_test.cpp           # Comprehensive unit tests
├── CMakeLists.txt             # CMake configuration
└── README.md                  # This file
```

## API Reference

### `ctq::circular_buffer<T>`

**Methods:**
- `circular_buffer(size_t max_size)` - Constructor
- `void push_back(T&& v)` - Add item to buffer
- `void emplace_back(Args&&... args)` - Construct item in place
- `T next()` - Get and remove front item
- `void pop_front()` - Remove front item
- `T front()` - Get front item without removing
- `size_t size() const` - Get current size
- `size_t capacity() const` - Get maximum capacity
- `bool empty() const` - Check if empty

**Note:** Can be used as a container for `basic_task_queue`

### `ctq::basic_task_queue<Container>`

**Constructor:**
- `basic_task_queue(callback cb, std::optional<size_t> max_elements, size_t workers = 1)`

**Methods:**
- `void push(type item)` - Add item to queue (may block if bounded)
- `void emplace(Args&&... args)` - Construct item in place
- `void access_queue(std::function<void(queue&)> f)` - Thread-safe queue access

**Supported Containers:**
- `std::vector<T>`
- `std::list<T>`
- `std::deque<T>`
- `ctq::circular_buffer<T>`
- your custom container with required interface

### `ctq::task_queue<Container, Ts...>`

**Constructor (single type):**
- `task_queue(callback cb, std::optional<size_t> max_elements, size_t workers = 1)`

**Methods:**
- `void push(type item)` - Add item to queue
- `void emplace(Args&&... args)` - Construct item in place
- `void access_queue(std::function<void(queue&)> f)` - Thread-safe queue access

## Thread Safety

All queue operations are thread-safe:
- Multiple producers can call `put()`/`push()` concurrently
- Multiple workers process items concurrently

