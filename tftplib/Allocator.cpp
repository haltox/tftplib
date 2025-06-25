#include "pch.h"
#include "Allocator.h"
#include <bit>
#include <algorithm>

namespace tftplib {

	Allocator::Allocator(size_t bufSz)
	{
		if (bufSz < MinBlockSize) {
			bufSz = MinBlockSize;
		}
		_bufferSize = bufSz;
		_buffer = new uint8_t[bufSz];
		_cursor = 0;
	}

	Allocator::~Allocator()
	{
		delete[] _buffer;
	}

	Allocator::Allocator(Allocator&& rhs) noexcept
	{
		*this = std::move(rhs);
	}

	Allocator& Allocator::operator=(Allocator&& rhs) noexcept
	{
		std::swap(_buffer, rhs._buffer);
		std::swap(_bufferSize, rhs._bufferSize);
		std::swap(_cursor, rhs._cursor);
		for (size_t i = 0; i < CacheSize; i++) {
			std::swap(_freeLists[i], rhs._freeLists[i]);
		}

		return *this;
	}

	void* Allocator::allocate(size_t sz)
	{
		std::lock_guard<std::mutex> lg {_mutex};

		if (sz == 0) {
			return nullptr;
		}

		sz += 8; // Add space for sz and alignment
		size_t bs = blockSize(sz);

		if (bs == 0) {
			return nullptr;
		}

		void* ptr = allocFromCache(sz);
		if (ptr == nullptr) {
			ptr = allocFromBuffer(sz);
		}

		return ptr;
	}

	void Allocator::free(void* ptr)
	{
		std::lock_guard<std::mutex> lg{ _mutex };

		if (ptr == nullptr) {
			return;
		}

		uint8_t* handle = reinterpret_cast<uint8_t*>(ptr);
		handle -= 8;

		size_t sz = *reinterpret_cast<size_t*>(handle);
		size_t cacheIndex = std::bit_width(sz) - std::bit_width(MinBlockSize);

		*reinterpret_cast<void**>(handle) = _freeLists[cacheIndex];
		_freeLists[cacheIndex] = handle;
	}

	size_t Allocator::blockSize(size_t sz) const
	{
		if( sz >= MaxBlockSize ) return 0;

		size_t bw = std::bit_width(sz);
		size_t bs = 1ull << bw;

		return std::max(bs, MinBlockSize);
	}

	void* Allocator::allocFromCache(size_t bs)
	{
		size_t index = std::bit_width(bs) - std::bit_width(MinBlockSize);
		if (_freeLists[index] == nullptr) {
			return nullptr;
		};

		void** next = reinterpret_cast<void**>(_freeLists[index]);
		size_t* header = reinterpret_cast<size_t*>(_freeLists[index]);
		uint8_t* data = reinterpret_cast<uint8_t*>(_freeLists[index]) + 8;

		_freeLists[index] = *next;
		*header = bs;
		return data;
	}

	void* Allocator::allocFromBuffer(size_t bs)
	{
		if (_cursor + bs > _bufferSize) {
			// oom
			return nullptr;
		}

		uint8_t *ptr = &_buffer[_cursor];
		_cursor += bs;
		*(size_t*)ptr = bs;
		return (ptr + 8);
	}

}