# CTQ - Concurrent Task Queue

A header-only C++20 library providing a flexible, compile-time task queue template that supports single or multiple message types with customizable container backends.

## Overview

CTQ provides a lightweight template-based task queue implementation that allows you to define queues with:
- Custom container types (e.g., `std::vector`, `std::list`, `std::deque`)
- Single or multiple message types
- Zero runtime overhead through compile-time type resolution

When a single type is specified, the queue uses that type directly. When multiple types are provided, the library automatically uses `std::variant` to support heterogeneous message types.

## Features

- **Header-only**: No compilation required, just include and use
- **Type-safe**: Full compile-time type checking
- **Flexible containers**: Works with any STL-compatible container template
- **C++20**: Modern C++ standards and features
- **Zero overhead**: Template metaprogramming resolves everything at compile time

## Requirements

- C++20 compatible compiler
- CMake 3.15 or higher (for building tests)

## Installation

Since CTQ is header-only, simply copy the `include/ctq` directory to your project or add it to your include path:

```bash
cp -r include/ctq /path/to/your/project/include/
```

Or use CMake to include it in your project.

## Usage

### Basic Example - Single Type Queue

```cpp
#include "ctq/task_queue.h"
#include <vector>

// Define a task queue that holds integers in a vector
ctq::task_queue<std::vector, int> queue;

// Access the message container type
ctq::task_queue<std::vector, int>::Messages messages;
```

### Multi-Type Queue with Variant

```cpp
#include "ctq/task_queue.h"
#include <vector>
#include <string>

// Define a task queue that can hold int, double, or string
ctq::task_queue<std::vector, int, double, std::string> queue;

// The underlying type is std::variant<int, double, std::string>
// stored in a std::vector
```

### Using Different Containers

```cpp
#include "ctq/task_queue.h"
#include <list>
#include <deque>

// Use std::list as the container
ctq::task_queue<std::list, int> list_queue;

// Use std::deque as the container
ctq::task_queue<std::deque, std::string> deque_queue;
```

## Building and Testing

The project uses CMake and Google Test for building and testing:

```bash
mkdir build
cd build
cmake ..
cmake --build .
ctest
```

## Project Structure

```
ctq/
├── include/
│   └── ctq/
│       └── task_queue.h    # Main header file
├── test/
│   └── ctq_test.cpp        # Unit tests
├── CMakeLists.txt          # CMake configuration
└── README.md               # This file
```

## How It Works

The library uses template metaprogramming to conditionally select the value type:

- **Single type**: When only one type `T` is provided, the queue directly uses `T`
- **Multiple types**: When multiple types are provided, the queue uses `std::variant<Ts...>`

The `task_queue` template takes:
1. A container template (e.g., `std::vector`, `std::list`)
2. One or more value types

The `Messages` type alias provides access to the fully instantiated container type.

## API Reference

### `ctq::task_queue<Container, Ts...>`

Template parameters:
- `Container`: A template template parameter for the container type (e.g., `std::vector`)
- `Ts...`: Variadic list of types the queue can hold

Type aliases:
- `type`: The value type (`T` or `std::variant<Ts...>`)
- `Messages`: The complete container type (`Container<type>`)

## License

[Add your license information here]

## Contributing

[Add contribution guidelines here]

## Author

[Add author information here]
