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

