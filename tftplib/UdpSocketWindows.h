#pragma once

#include <memory>
#include <iostream>

namespace tftplib
{
	class DatagramFactory;
	class Datagram;

	class UdpSocketWindows
	{
	public:
		enum State {
			Inactive,
			Bound
		};

	public:
		UdpSocketWindows();
		~UdpSocketWindows();

		UdpSocketWindows(const UdpSocketWindows&) = delete;
		UdpSocketWindows& operator=(const UdpSocketWindows&) = delete;
		UdpSocketWindows(UdpSocketWindows&&) = delete;

		UdpSocketWindows& SetOutStream(std::ostream* os);
		UdpSocketWindows& SetErrStream(std::ostream* os);

		bool Bind(const char* hostname, uint16_t port);

		State GetState() const {
			return _state;
		}

		bool IsBound() const {
			return _state == Bound;
		}

		bool IsInactive() const {
			return _state == Inactive;
		}

		uint16_t GetSocketPort() const;

		bool HasDatagram() const;

		std::shared_ptr<tftplib::Datagram> Receive(DatagramFactory &factory);

		bool IsIpv6() const;
	private:
		struct OsSpecific;

	private:
		std::ostream& Out() const {
			return _out ? *_out : std::cout;
		}

		std::ostream& Err() const {
			return _err ? *_err : std::cerr;
		}

		void InitWSA();

		bool PrepareOSAddress(const char* hostname,
			uint16_t port,
			OsSpecific* os);
		bool CreateSocket(OsSpecific* os);
		bool SetSocketOptions(OsSpecific* os);
		bool BindSocket(OsSpecific* os);

		bool InitRecvMsg(OsSpecific* os);

		void LogSocketError(const char* what) const;

		std::ostream* _out;
		std::ostream* _err;

	private:
		State _state;
		std::unique_ptr<OsSpecific> _os;

	};

}