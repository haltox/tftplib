#pragma once

#include <mutex>
#include <cstdint>
#include <bit>

namespace tftplib {
	class Allocator
	{
	public:
	
		static constexpr size_t MaxBlockSize = 0x10000;
		static constexpr size_t MinBlockSize = 0x00010;
		static constexpr size_t CacheSize = 
			1 + std::bit_width(MaxBlockSize) - std::bit_width(MinBlockSize);

	public:
		Allocator(size_t bufSz);
		~Allocator();
		Allocator(Allocator&&);
		Allocator& operator=(Allocator&&);

		Allocator(const Allocator&) = delete;
		Allocator& operator=(const Allocator&) = delete;

		void* allocate(size_t sz);
		void free(void* ptr);

	private:
		size_t blockSize(size_t sz) const;
		void* allocFromCache(size_t bs);
		void* allocFromBuffer(size_t bs);

	private:
		void* _freeLists[CacheSize] = {0};
		uint8_t *_buffer {nullptr};
		size_t _bufferSize{ 0 };
		size_t _cursor{ 0 };
		
		std::mutex _mutex;
	};
}