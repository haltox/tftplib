#include "pch.h"
#include "DatagramFactory.h"

namespace tftplib {

	// **********************************************************************
	// Factory impl
	// **********************************************************************

	std::shared_ptr<DatagramFactory>
	DatagramFactory::Instantiate(size_t poolSize)
	{
		std::shared_ptr<DatagramFactory> factory(
			new DatagramFactory(poolSize) );

		factory->_self = factory;

		return factory;
	}

	DatagramFactory::DatagramFactory(size_t poolSize)
		: _poolOfDatagram(poolSize)
		, _poolOfControlData(poolSize)
	{
	}

	DatagramFactory::~DatagramFactory()
	{
	}

	DatagramAssembly
	DatagramFactory::StartAssembly()
	{
		return DatagramAssembly{_self};
	}

	std::shared_ptr<Datagram>
	DatagramFactory::BuildResponse(const uint8_t *data,
			uint16_t len,
			const Datagram& respondTo)
	{
		auto txAssembly = StartAssembly();

		txAssembly.SetDestinationAddress(respondTo.GetSourceAddress());
		txAssembly.SetDestinationPort(respondTo.GetSourcePort());
		
		if (len > txAssembly.GetDataBufferSize())
		{
			return nullptr;
		}

		memcpy(txAssembly.GetDataBuffer(), data, len);
		txAssembly.SetDataSize(len);
		return txAssembly.Finalize();
	}

	void 
	DatagramFactory::Reclaim(Datagram& datagram)
	{
		_poolOfDatagram.Free(datagram.GetDataBuffer());
		_poolOfControlData.Free(datagram.GetControlBuffer());
	}

	bool
	DatagramFactory::InitializeDatagramBuffers(Datagram& datagram)
	{
		datagram._dataBufferSize = 
			static_cast<uint16_t>(_poolOfDatagram.BufferSize());
		datagram._controlBufferSize = 
			static_cast<uint16_t>(_poolOfControlData.BufferSize());
		datagram._data = _poolOfDatagram.Alloc();
		datagram._controlBuffer = _poolOfControlData.Alloc();

		return (datagram._data && datagram._controlBuffer);
	}

}
