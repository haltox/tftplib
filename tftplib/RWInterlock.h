#pragma once

#include <atomic>
#include <semaphore>
#include <cstdint>

namespace tftplib {
	class RWInterlock
	{
	public:
		bool TryLockRead();
		bool TryLockWrite();

		void UnlockRead();
		void UnlockWrite();

		bool IsFree() const;

	private:
		bool LockReadAttempt();
		bool LockWriteAttempt();

	private:
		std::atomic<uint_fast32_t> _counter{ 0 };
	};
}