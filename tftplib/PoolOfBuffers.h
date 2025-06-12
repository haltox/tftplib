#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <stdexcept>

namespace tftplib
{
	// **********************************************************************
	// Pool of fixed size buffers.
	// BUF_SZ is a size in bytes.
	// PTR_TYPE is for convenience - returned buffers will
	//	be pointers to PTR_TYPE.
	//
	// Only the following safeguards are provided : 
	//	- BUF_SZ must be greater than 0
	//	- PTR_TYPE must be smaller than BUF_SZ
	// **********************************************************************
	
	template <size_t BUF_SZ, typename PTR_TYPE = char>
	class PoolOfBuffers
	{
		static_assert(BUF_SZ > 0, 
			"Buffer size must be greater than 0");

		template <typename T> static constexpr bool IsSizeValid(size_t SZ) {
			return sizeof(T) <= BUF_SZ;
		}

		template <> static constexpr bool IsSizeValid<void>(size_t SZ) {
			return true;
		}

		static_assert(IsSizeValid<PTR_TYPE>(BUF_SZ),
			"PTR_TYPE is bigger than buffer");

	public:
		PoolOfBuffers(size_t poolSize);
		~PoolOfBuffers();
		PoolOfBuffers(const PoolOfBuffers&) = delete;
		PoolOfBuffers& operator=(const PoolOfBuffers&) = delete;
		PoolOfBuffers(PoolOfBuffers&&) = delete;

		PTR_TYPE* Alloc();
		void Free(PTR_TYPE* ptr);
		size_t BufferSize() const {
			return BUF_SZ;
		}

	private:
		char* _buffer{ nullptr };
		bool* _used{ nullptr }; // Array of flags indicating if a buffer is used

		size_t _poolSize{ 0 }; // Number of buffers in the pool
		size_t _cursor{ 0 }; // Current position in the pool
	};

}

template <size_t BUF_SZ, typename PTR_TYPE>
tftplib::PoolOfBuffers<BUF_SZ, PTR_TYPE>::PoolOfBuffers(size_t poolSize)
	: _poolSize(poolSize)
{
	if (poolSize == 0) {
		throw std::runtime_error("Pool size must be greater than 0");
	}

	_buffer = new char[poolSize * BUF_SZ];
	_used = new bool[poolSize];
	if (!_buffer || !_used) {
		throw std::bad_alloc();
	}

	for (size_t i = 0; i < _poolSize; ++i) {
		_used[i] = false;
	}
}

template <size_t BUF_SZ, typename PTR_TYPE>
tftplib::PoolOfBuffers<BUF_SZ, PTR_TYPE>::~PoolOfBuffers()
{
	delete[] _buffer;
}

template <size_t BUF_SZ, typename PTR_TYPE>
PTR_TYPE*
tftplib::PoolOfBuffers<BUF_SZ, PTR_TYPE>::Alloc()
{
	size_t backupCursor = _cursor;

	do {
		size_t current = _cursor;
		_cursor = (_cursor + 1) % _poolSize;

		if (!_used[current]) {
			_used[current] = true;
			return reinterpret_cast<PTR_TYPE*>(_buffer + current * BUF_SZ);
		}
	} while (_cursor != backupCursor);

	return nullptr;
}

template <size_t BUF_SZ, typename PTR_TYPE>
void
tftplib::PoolOfBuffers<BUF_SZ, PTR_TYPE>::Free(PTR_TYPE* ptr)
{
	if (!ptr) {
		return;
	}

	size_t index = (reinterpret_cast<char*>(ptr) - _buffer) / BUF_SZ;
	if (index < _poolSize && _used[index]) {
		_used[index] = false;
	}
	else {
		throw std::runtime_error("Pointer does not belong to this pool");
	}
}