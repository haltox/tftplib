#pragma once

#include <cstdint>
#include <string>
#include <memory>

namespace tftplib
{
	class DatagramFactory;
	class DatagramAssembly;

	class Datagram
	{
	public:
		
		// Copy does not make sense.
		Datagram(const Datagram&) = delete;
		Datagram& operator=(const Datagram&) = delete;
		
		// Move does make sense
		Datagram(Datagram&&) noexcept;
		Datagram& operator=(Datagram&&) noexcept;
		~Datagram();

		bool IsValid() const;
		bool IsBroadcast() const;
		const std::string &GetSourceAddress() const;
		const std::string &GetDestAddress() const;
		uint16_t GetSourcePort() const;
		uint16_t GetDestPort() const;
		
		const char* GetData() const;
		uint16_t GetDataSize() const;

		char* GetDataBuffer();
		char* GetControlBuffer();
		uint16_t GetControlSize() const;

	private:
		// Delegate construction to factory class.
		Datagram() = default;

	private:
		bool _valid{ false };
		bool _isBroadcast{ false };
		std::string _sourceAddress {};
		std::string _destAddress{};
		uint16_t _sourcePort{ 0 };
		uint16_t _destPort{ 0 };

		char* _data{ nullptr };
		uint16_t _dataSize{ 0 };
		uint16_t _dataBufferSize{ 0 };
		char* _controlBuffer{ nullptr };
		uint16_t _controlSize{ 0 };
		uint16_t _controlBufferSize{ 0 };

		std::weak_ptr<DatagramFactory>_reclaimer {};

		friend class DatagramFactory;
		friend class DatagramAssembly;

	public:
		Datagram& Write(const void* data, size_t size);
		Datagram& operator<<(bool value);
		Datagram& operator<<(uint8_t value);
		Datagram& operator<<(uint16_t value);
		Datagram& operator<<(uint32_t value);
		Datagram& operator<<(uint64_t value);
		Datagram& operator<<(int8_t value);
		Datagram& operator<<(int16_t value);
		Datagram& operator<<(int32_t value);
		Datagram& operator<<(int64_t value);
		Datagram& operator<<(const char* value);
	};
}


