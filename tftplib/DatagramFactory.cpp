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

	void 
	DatagramFactory::Reclaim(Datagram& datagram)
	{
		_poolOfDatagram.Free(datagram.GetDataBuffer());
		_poolOfControlData.Free(datagram.GetControlBuffer());
	}

	bool
	DatagramFactory::InitializeDatagramBuffers(Datagram& datagram)
	{
		datagram._dataBufferSize = _poolOfDatagram.BufferSize();
		datagram._controlBufferSize = _poolOfControlData.BufferSize();
		datagram._data = _poolOfDatagram.Alloc();
		datagram._controlBuffer = _poolOfControlData.Alloc();

		return (datagram._data && datagram._controlBuffer);
	}

}
