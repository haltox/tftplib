#include "pch.h"
#include "Signal.h"

void tftplib::Signal::EmitSignal()
{
	if (_mallet.try_acquire())
	{
		_bell.release();
	}
}

bool tftplib::Signal::WaitForSignal()
{
	_bell.acquire();
	_mallet.release();
	return true;
}

void tftplib::Signal::Reset()
{
	(void)_bell.try_acquire();
	(void)_mallet.try_acquire();
	_mallet.release();
}
