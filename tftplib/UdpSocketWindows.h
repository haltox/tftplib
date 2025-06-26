#pragma once

#include <memory>
#include <iostream>
#include <atomic>

namespace tftplib
{
	class DatagramFactory;
	class Datagram;

	class UdpSocketWindows
	{
	public:
		enum State {
			Inactive,
			Binding,
			Bound,
			Unbinding
		};

		class GlobalOsContext {
		public:
			GlobalOsContext();
			~GlobalOsContext();
		};

		static std::unique_ptr<GlobalOsContext> InitGlobalOsContext();
		
	public:
		UdpSocketWindows();
		~UdpSocketWindows();

		UdpSocketWindows(const UdpSocketWindows&) = delete;
		UdpSocketWindows& operator=(const UdpSocketWindows&) = delete;
		UdpSocketWindows(UdpSocketWindows&&) = delete;

		UdpSocketWindows& SetOutStream(std::ostream* os);
		UdpSocketWindows& SetErrStream(std::ostream* os);

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

		bool IsIpv6() const;

		std::string GetLocalAddress() const;

		uint16_t GetLocalPort() const;

		bool HasDatagram() const;

		bool Poll(uint32_t timeout = 0) const;

		bool Bind(const char* hostname, uint16_t port = 0);

		bool Unbind();

		std::shared_ptr<tftplib::Datagram> Receive(DatagramFactory &factory);
		
		bool Send(std::shared_ptr<tftplib::Datagram> datagram);

	private:
		struct OsSpecific;

	private:
		std::ostream& Out() const {
			return _out ? *_out : std::cout;
		}

		std::ostream& Err() const {
			return _err ? *_err : std::cerr;
		}

		std::shared_ptr<OsSpecific> Os() const;
		
		bool CreateSocket(OsSpecific* os, bool isIpv6);
		bool SetSocketOptions(OsSpecific* os, bool isIpv6);
		bool BindSocket(const char* hostname,
			uint16_t port, 
			OsSpecific* os);

		bool InitRecvMsg(OsSpecific* os);

		void LogSocketError(const char* what) const;


	private:

		std::ostream* _out;
		std::ostream* _err;

		std::atomic<State> _state;
		mutable std::atomic<int> _activityCounter {0};
		
		std::shared_ptr<OsSpecific> _osLifeCycle;
		std::weak_ptr<OsSpecific> _osHandle;
	};

}