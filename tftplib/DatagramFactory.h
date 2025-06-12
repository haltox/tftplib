#pragma once

#include "Datagram.h"
#include "PoolOfBuffers.h"
#include "DatagramAssembly.h"

namespace tftplib
{
	class DatagramFactory
	{
	public:
		static std::shared_ptr<DatagramFactory> Instantiate(size_t poolSize = 16);

	public:
		~DatagramFactory();

		DatagramAssembly StartAssembly();
	
		void Reclaim(Datagram &datagram);

	private:
		DatagramFactory(size_t poolSize = 16);

		bool InitializeDatagramBuffers(Datagram& datagram);

	private:
		tftplib::PoolOfBuffers<0xFFFF> _poolOfDatagram;
		tftplib::PoolOfBuffers<0x0080> _poolOfControlData;
		std::weak_ptr<DatagramFactory> _self {};

		friend class DatagramAssembly;
	};
}
