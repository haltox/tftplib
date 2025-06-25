#pragma once

#include <semaphore>
#include <chrono>

namespace tftplib {
	class Signal
	{
	public:
		void EmitSignal();

		bool WaitForSignal();

		template< class Rep, class Period >
		bool WaitForSignal(const std::chrono::duration<Rep, Period>& rel_time);

		void Reset();

	private:
		std::binary_semaphore _bell{ 0 };
		std::binary_semaphore _mallet{ 1 };
	};
}



template< class Rep, class Period >
bool 
tftplib::Signal::WaitForSignal(const std::chrono::duration<Rep, Period>& rel_time)
{
	if (_bell.try_acquire_for(rel_time))
	{
		_mallet.release();
		return true;
	}
	else
	{
		return false;
	}
}
