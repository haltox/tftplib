#pragma once

#include <memory>
#include <string>
#include <cstdint>

namespace tftplib {

	class DatagramFactory;
	class Datagram;

	class DatagramAssembly
	{
	private:
		DatagramAssembly(std::weak_ptr<DatagramFactory> parent);

	public:
		DatagramAssembly& SetBroadcast(bool broadcast);
		DatagramAssembly& SetSourceAddress(std::string addr);
		DatagramAssembly& SetDestinationAddress(std::string addr);
		DatagramAssembly& SetSourcePort(uint16_t port);
		DatagramAssembly& SetDestinationPort(uint16_t port);

		char* GetDataBuffer();
		uint16_t GetDataBufferSize();

		char* GetControlBuffer();
		uint16_t GetControlBufferSize();

		DatagramAssembly& SetDataSize(uint16_t size);

		std::shared_ptr<tftplib::Datagram> Finalize();

		bool IsValid() const { return _isValid; }

	private:
		std::shared_ptr<DatagramFactory> _parent;
		std::shared_ptr<tftplib::Datagram> _datagram;

		bool _isValid {false};

		friend class DatagramFactory;
	};

}

