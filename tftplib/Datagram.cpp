#include "pch.h"
#include "Datagram.h"
#include "DatagramFactory.h"
#include <bit>
namespace tftplib {
	
	Datagram::Datagram(Datagram&& rhs) noexcept
	{
		*this = std::move(rhs);
	}

	Datagram& Datagram::operator=(Datagram&& rhs) noexcept
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

	const std::string &Datagram::GetSourceAddress() const
	{
		return _sourceAddress;
	}

	const std::string &Datagram::GetDestAddress() const
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

	void Datagram::SetDataSize(uint16_t sz)
	{
		_dataSize = sz;
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


/***************************************************************************
 *	S T R E A M   W R I T E   O P E R A T O R S
 ***************************************************************************/
	
	Datagram& Datagram::Write(const void* data, size_t size)
	{
		if ((size + _dataSize) > _dataBufferSize) {
			return *this;
		}

		memcpy(&_data[_dataSize], data, size);
		_dataSize += static_cast<uint16_t>(size);

		return *this;
	}
	
	Datagram& Datagram::operator<<(bool value)
	{
		_data[_dataSize++] = value ? 1 : 0;
		return *this;
	}

	Datagram& Datagram::operator<<(uint8_t value)
	{
		_data[_dataSize++] = value;
		return *this;
	}

	Datagram& Datagram::operator<<(uint16_t value)
	{
		_data[_dataSize++] = (value >> 1) & 0xFF;
		_data[_dataSize++] = (value >> 0) & 0xFF;

		return *this;
	}

	Datagram& Datagram::operator<<(uint32_t value)
	{
		_data[_dataSize++] = (value >> 3) & 0xFF;
		_data[_dataSize++] = (value >> 2) & 0xFF;
		_data[_dataSize++] = (value >> 1) & 0xFF;
		_data[_dataSize++] = (value >> 0) & 0xFF;

		return *this;
	}

	Datagram& Datagram::operator<<(uint64_t value)
	{
		_data[_dataSize++] = (value >> 7) & 0xFF;
		_data[_dataSize++] = (value >> 6) & 0xFF;
		_data[_dataSize++] = (value >> 5) & 0xFF;
		_data[_dataSize++] = (value >> 4) & 0xFF;
		_data[_dataSize++] = (value >> 3) & 0xFF;
		_data[_dataSize++] = (value >> 2) & 0xFF;
		_data[_dataSize++] = (value >> 1) & 0xFF;
		_data[_dataSize++] = (value >> 0) & 0xFF;

		return *this;
	}

	Datagram& Datagram::operator<<(int8_t value)
	{
		_data[_dataSize++] = value;
		return *this;
	}

	Datagram& Datagram::operator<<(int16_t value)
	{
		_data[_dataSize++] = (value >> 1) & 0xFF;
		_data[_dataSize++] = (value >> 0) & 0xFF;

		return *this;
	}

	Datagram& Datagram::operator<<(int32_t value)
	{
		_data[_dataSize++] = (value >> 3) & 0xFF;
		_data[_dataSize++] = (value >> 2) & 0xFF;
		_data[_dataSize++] = (value >> 1) & 0xFF;
		_data[_dataSize++] = (value >> 0) & 0xFF;

		return *this;
	}

	Datagram& Datagram::operator<<(int64_t value)
	{
		_data[_dataSize++] = (value >> 7) & 0xFF;
		_data[_dataSize++] = (value >> 6) & 0xFF;
		_data[_dataSize++] = (value >> 5) & 0xFF;
		_data[_dataSize++] = (value >> 4) & 0xFF;
		_data[_dataSize++] = (value >> 3) & 0xFF;
		_data[_dataSize++] = (value >> 2) & 0xFF;
		_data[_dataSize++] = (value >> 1) & 0xFF;
		_data[_dataSize++] = (value >> 0) & 0xFF;

		return *this;
	}

	Datagram& Datagram::operator<<(const char* value)
	{
		if (value == nullptr) {
			return *this;
		}
		
		size_t len = strlen(value);
		if (len + 1 > _dataBufferSize - _dataSize) {
			return *this;
		}

		memcpy(_data + _dataSize, value, len);
		_dataSize += static_cast<uint16_t>(len);
		_data[_dataSize++] = '\0';

		return *this;
	}

}