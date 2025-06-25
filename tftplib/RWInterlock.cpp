#include "pch.h"
#include "RWInterlock.h"
#include <thread>

namespace tftplib {

	constexpr uint32_t WriteFlag = 0x80000000;
	constexpr uint32_t Attempts = 3;

	bool RWInterlock::TryLockRead()
	{
		uint32_t attempt = Attempts;
		bool result = false;
		
		while (!result && attempt--) 
		{
			result = LockReadAttempt();
			std::this_thread::yield();
		}

		return result;
	}

	bool RWInterlock::TryLockWrite()
	{
		uint32_t attempt = Attempts;
		bool result = false;

		while (!result && attempt--)
		{
			result = LockWriteAttempt();
			std::this_thread::yield();
		}

		return result;
	}
	
	void RWInterlock::UnlockRead()
	{
		_counter.fetch_add(-1);
	}
	
	void RWInterlock::UnlockWrite()
	{
		_counter.fetch_and(~WriteFlag);
	}

	bool RWInterlock::IsFree() const
	{
		return _counter == 0;
	}

	bool RWInterlock::LockReadAttempt()
	{
		if (_counter.fetch_add(1) & WriteFlag)
		{
			_counter.fetch_add(-1);
			return false;
		}

		return true;
	}

	bool RWInterlock::LockWriteAttempt()
	{
		uint32_t r = _counter.fetch_or(WriteFlag);
		if ((r & WriteFlag) == WriteFlag)
		{
			// Other writer has lock. Leave lock alone and exit.
			return false;
		}
		else if (r != 0)
		{
			// Reader has lock. Remove write lock and exit.
			_counter.fetch_and(~WriteFlag);
			return false;
		}

		return true;
	}
}