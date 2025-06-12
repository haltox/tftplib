#include "pch.h"
#include "DatagramAssembly.h"
#include "DatagramFactory.h"

namespace tftplib {
	// **********************************************************************
	// Assembly impl
	// **********************************************************************

	DatagramAssembly::DatagramAssembly(
		std::weak_ptr<DatagramFactory> parent)
		: _parent{ parent }
		, _datagram{ new Datagram{} }
	{
		_parent->InitializeDatagramBuffers(*_datagram);
	}

	DatagramAssembly&
	DatagramAssembly::SetBroadcast(bool broadcast)
	{
		_datagram->_isBroadcast = broadcast;
		return *this;
	}

	DatagramAssembly&
	DatagramAssembly::SetSourceAddress(std::string addr)
	{
		_datagram->_sourceAddress = addr;
		return *this;
	}

	DatagramAssembly&
	DatagramAssembly::SetDestinationAddress(std::string addr)
	{
		_datagram->_destAddress = addr;
		return *this;
	}

	DatagramAssembly&
	DatagramAssembly::SetSourcePort(uint16_t port)
	{
		_datagram->_sourcePort = port;
		return *this;
	}

	DatagramAssembly&
	DatagramAssembly::SetDestinationPort(uint16_t port)
	{
		_datagram->_destPort = port;
		return *this;
	}

	char*
	DatagramAssembly::GetDataBuffer()
	{
		return _datagram->GetDataBuffer();
	}

	uint16_t
	DatagramAssembly::GetDataBufferSize()
	{
		return _datagram->_dataBufferSize;
	}

	char*
	DatagramAssembly::GetControlBuffer()
	{
		return _datagram->GetControlBuffer();
	}

	uint16_t
	DatagramAssembly::GetControlBufferSize()
	{
		return _datagram->_controlBufferSize;
	}

	DatagramAssembly&
	DatagramAssembly::SetDataSize(uint16_t size)
	{
		_datagram->_dataSize = size;
		return *this;
	}

	std::shared_ptr<tftplib::Datagram> DatagramAssembly::Finalize()
	{
		std::shared_ptr<tftplib::Datagram> finalized {nullptr};
		std::swap(finalized, _datagram);

		finalized->_valid = true;
		return finalized;
	}
}