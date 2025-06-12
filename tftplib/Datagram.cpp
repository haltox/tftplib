#include "pch.h"
#include "Datagram.h"
#include "DatagramFactory.h"

namespace tftplib {
	
	Datagram::Datagram(Datagram&& rhs)
	{
		*this = std::move(rhs);
	}

	Datagram& Datagram::operator=(Datagram&& rhs)
	{
		std::swap(_valid, rhs._valid);
		std::swap(_isBroadcast, rhs._isBroadcast);
		std::swap(_sourceAddress, rhs._sourceAddress);
		std::swap(_destAddress, rhs._destAddress);
		std::swap(_sourcePort, rhs._sourcePort);
		std::swap(_destPort, rhs._destPort);
		std::swap(_data, rhs._data);
		std::swap(_dataSize, rhs._dataSize);
		std::swap(_dataBufferSize, rhs._dataBufferSize);
		std::swap(_controlBuffer, rhs._controlBuffer);
		std::swap(_controlSize, rhs._controlSize);
		std::swap(_controlBufferSize, rhs._controlBufferSize);
		std::swap(_reclaimer, rhs._reclaimer);

		return *this;
	}

	Datagram::~Datagram() 
	{
		std::shared_ptr<DatagramFactory> reclaimer = _reclaimer.lock();
		if (!reclaimer) {
			return;
		}

		reclaimer->Reclaim(*this);
	}

	bool Datagram::IsValid() const 
	{
		return _valid;
	}

	bool Datagram::IsBroadcast() const
	{
		return _isBroadcast;
	}

	const std::string Datagram::GetSourceAddress() const
	{
		return _sourceAddress;
	}

	const std::string Datagram::GetDestAddress() const
	{
		return _destAddress;
	}

	uint16_t Datagram::GetSourcePort() const
	{
		return _sourcePort;
	}

	uint16_t Datagram::GetDestPort() const
	{
		return _destPort;
	}

	const char* Datagram::GetData() const
	{
		return _data;
	}

	uint16_t Datagram::GetDataSize() const
	{
		return _dataSize;
	}

	char* Datagram::GetDataBuffer()
	{
		return _data;
	}

	char* Datagram::GetControlBuffer()
	{
		return _controlBuffer;
	}

	uint16_t Datagram::GetControlSize() const
	{
		return _controlSize;
	}
}