#pragma once

#include <vector>
#include <utility>

namespace ctq {

template< typename T >
struct circular_buffer {
	typedef T value_type;

	circular_buffer(size_t max_size) {
		b_.resize(max_size);
	}

	size_t capacity() const {
		return b_.size();
	}

	bool empty() const {
		return cnt_ == 0;
	}

	void push_back(T&& v) {
		assert(cnt_ <= b_.size());
		assert(read_pnt_ < b_.size());
		// next index
		auto i = ( read_pnt_ + cnt_ ) % b_.size();
		b_[i] = v;
		++cnt_;
	}

	template<typename... Args>
	void emplace_back(Args&&... args) {
		assert(cnt_ <= b_.size());
		assert(read_pnt_ < b_.size());
		// next index
		auto i = ( read_pnt_ + cnt_ ) % b_.size();
		new (b_.data() + i) T(std::forward<Args>(args)...);
		++cnt_;
	}

	T front() {
		assert(cnt_ > 0);
		return b_[read_pnt_];
	}

	void pop_front() {
		--cnt_;
		++read_pnt_;
		if (read_pnt_ == b_.size())
			read_pnt_ = 0;
	}

	// return and pop
	T next() {
		--cnt_;
		auto i = read_pnt_++;
		if (read_pnt_ == b_.size())
			read_pnt_ = 0;
		return b_[i];
	}

	size_t size() const
	{
		return cnt_;
	}

private:
	std::vector<value_type> b_;
	size_t cnt_{}; // number of element in the buffer
	size_t read_pnt_{};

};

} // namespace ctq
