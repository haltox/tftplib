#include "pch.h"
#include "UdpSocketWindows.h"
#include "DatagramFactory.h"

#include <Winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <string>
#include <Mswsock.h>
#include <tchar.h>
#include <combaseapi.h>

#include <thread>
#include <chrono>

namespace tftplib
{

/***************************************************************************
 *	L O C A L   A N D   I N T E R N A L   T Y P E S 
 ***************************************************************************/

	/*
	 * struct UdpSocketWindows::OsSpecific
	 *		RAII container for OS specific resources.
	 */
	struct UdpSocketWindows::OsSpecific 
	{
		sockaddr LocalAddress {0};
		SOCKET Socket{};
		LPFN_WSARECVMSG fnRcvMsg{ nullptr };

		OsSpecific() {
		}

		~OsSpecific() {
			if (Socket != INVALID_SOCKET) {
				closesocket(Socket);
				Socket = INVALID_SOCKET;
			}
		}
	};

	/*
	 * struct AddrInfoBox
	 *		RAII container for addr information.
	 */
	struct AddrInfoBox {
		addrinfo* addrInfo{ nullptr };
		~AddrInfoBox() {
			if (addrInfo) {
				freeaddrinfo(addrInfo);
				addrInfo = nullptr;
			}
		}
	};

	/*
	 * class ActivityGuard
	 *		RAII container for activity counter.
	 */
	class ActivityGuard {
	private:
		std::atomic<int>& _counter;
	public:
		ActivityGuard(std::atomic<int>& activityCounter)
			: _counter{ activityCounter }
		{
			_counter.fetch_add(1);
		}

		~ActivityGuard()
		{
			_counter.fetch_add(-1);
		}
	};

	/*
	 * class UdpSocketWindows::GlobalOsContext
	 *		RAII container for global os context.
	 *		Initialized and deinitialize required APIs and DLLs
	 *		that are required across socket lifetimes.
	 */
	UdpSocketWindows::GlobalOsContext::GlobalOsContext() {
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			throw std::runtime_error("WSAStartup failed");
		}

		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
			WSACleanup();
			throw std::runtime_error("Could not find a usable version of Winsock.");
		}
	}

	UdpSocketWindows::GlobalOsContext::~GlobalOsContext() {
		WSACleanup();
	}

	/*
	 * class ProviderList
	 *		Query list of provider and manage required buffers
	 */

	class ProviderList
	{
	private:
		uint32_t _count {0};
		uint8_t *_buffer{ nullptr };
		WSAPROTOCOL_INFO *_listOfProviders {nullptr};

	private:
		size_t RequiredBufferSize() 
		{
			DWORD bufSize = 0;
			WSAEnumProtocols(nullptr, nullptr, &bufSize);
			return bufSize;
		}

		void QueryProviders(WSAPROTOCOL_INFO* list, uint32_t bufferSize)
		{
			DWORD bufSz = bufferSize;
			auto result = WSAEnumProtocols(nullptr, list, &bufSz);

			if (result != SOCKET_ERROR)
			{
				_count = result;
			}
		}

	public:
		ProviderList()
		{
			_buffer = new uint8_t[RequiredBufferSize()];
			_listOfProviders = (WSAPROTOCOL_INFO *)_buffer;

		}

		~ProviderList()
		{
			delete[] _buffer;
		}

		WSAPROTOCOL_INFO* FindProvider(bool ipv6)
		{
			int af = ipv6 ? AF_INET6 : AF_INET;
			for (uint32_t i = 0; i < _count; i++)
			{
				WSAPROTOCOL_INFO* provider = &_listOfProviders[i];
				if (provider->dwServiceFlags1 & XP1_CONNECTIONLESS
					&& provider->iProtocol == IPPROTO_UDP 
					&& provider->iAddressFamily == af)
				{
					std::wstring proto { provider->szProtocol};
					if (proto.starts_with(L"MSAFD"))
					{
						return provider;
					}
				}
			}

			return nullptr;
		}
	};

/***************************************************************************
 *	F I L E   L O C A L   S T A T I C   F U N C T I O N S
 ***************************************************************************/

	/*
	 * Addr to String conversion
	 */
	std::string AddrToStr(IN_ADDR* addr)
	{
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, addr, ip, sizeof(ip));
		return std::string(ip);
	}

	/*
	 * Addr to String conversion
	 */
	std::string AddrToStr(IN6_ADDR* addr)
	{
		char ip[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, addr, ip, sizeof(ip));
		return std::string(ip);
	}

	/*
	 * Addr to String conversion
	 */
	std::string AddrToStr(sockaddr* addr) 
	{
		if (addr == nullptr) {
			return "[nullptr]";
		}
		else if (addr->sa_family == AF_INET) {
			sockaddr_in* inet4 = (sockaddr_in*)addr;
			return AddrToStr(&inet4->sin_addr) 
				+ ":" 
				+ std::to_string(ntohs(inet4->sin_port));
		}
		else if (addr->sa_family == AF_INET6) {
			sockaddr_in6* inet6 = (sockaddr_in6*)addr;
			return AddrToStr(&inet6->sin6_addr)
				+ ":"
				+ std::to_string(ntohs(inet6->sin6_port));
		}

		return "[error]";
	}

	/*
	 * String to addr conversion
	 */
	int
	ParseAddress(const char* hostname,
			uint16_t port,
			addrinfo** address,
			int addressFamily)
	{
		addrinfo hints{};
		hints.ai_family = addressFamily;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;

		std::string portStr = std::to_string(port);

		int result = getaddrinfo(hostname,
			portStr.c_str(),
			&hints,
			address);

		return result;
	}

	/*
	 * String to addr conversion
	 */
	int
	ParseAddress(const std::string &hostname,
			uint16_t port,
			addrinfo** address,
			int addressFamily)
	{
		return ParseAddress(hostname.c_str(), port, address, addressFamily);
	}
	
	
	/*
	 * Identify whether an addr is ipv6 formatted.
	 */
	bool
	IsHostnameIpv6(const char* hostname)
	{
		AddrInfoBox addr;

		ParseAddress(hostname, 0, &addr.addrInfo, AF_UNSPEC);
		return addr.addrInfo->ai_family == AF_INET6;
	}

/***************************************************************************
 *	C L A S S   S T A T I C   F U N C T I O N S
 ***************************************************************************/

	std::unique_ptr<UdpSocketWindows::GlobalOsContext>
	UdpSocketWindows::InitGlobalOsContext()
	{
		return std::make_unique<GlobalOsContext>();
	}

/***************************************************************************
 *	U D P _ S O C K E T _ W I N D O W S   P U B L I C   A P I
 ***************************************************************************/
	
	UdpSocketWindows::UdpSocketWindows()
		: _out{ nullptr }
		, _err{ nullptr }
		, _state{ Inactive }
		, _osLifeCycle{ nullptr }
		, _osHandle { _osLifeCycle }
	{
	}

	UdpSocketWindows::~UdpSocketWindows() 
	{
	}

	UdpSocketWindows& 
	UdpSocketWindows::SetOutStream(std::ostream* os)
	{
		_out = os;
		return *this;
	}

	UdpSocketWindows&
	UdpSocketWindows::SetErrStream(std::ostream* os) {
		_err = os;
		return *this;
	}

	uint16_t
	UdpSocketWindows::GetSocketPort() const
	{
		if (!IsBound()) {
			Out() << "Socket is not bound." << std::endl;
			return 0;
		}

		std::shared_ptr<OsSpecific> os = _osHandle.lock();
		if (!os) {
			Out() << "Socket is not bound." << std::endl;
			return 0;
		}

		switch (os->LocalAddress.sa_family)
		{
		case AF_INET: {
			sockaddr_in* inet4 = (sockaddr_in*)&os->LocalAddress;
			return ntohs(inet4->sin_port);
		}
		case AF_INET6: {
			sockaddr_in6* inet6 = (sockaddr_in6*)&os->LocalAddress;
			return ntohs(inet6->sin6_port);
		}
		}

		return 0;
	}

	bool
	UdpSocketWindows::IsIpv6() const {
		std::shared_ptr<OsSpecific> os = Os();
		return os && os->LocalAddress.sa_family== AF_INET6;
	}

	std::string 
	UdpSocketWindows::GetLocalAddress() const
	{
		std::shared_ptr<OsSpecific> os = Os();
		if (os) 
		{
			switch (os->LocalAddress.sa_family)
			{
				case AF_INET: {
					sockaddr_in* addr = (sockaddr_in*)&os->LocalAddress;
					return AddrToStr(&addr->sin_addr);
				} break;

				case AF_INET6: {
					sockaddr_in6* addr = (sockaddr_in6*)&os->LocalAddress;
					return AddrToStr(&addr->sin6_addr);
				} break;
			}
		}

		return "[Not bound]";
	}

	uint16_t 
	UdpSocketWindows::GetLocalPort() const
	{
		std::shared_ptr<OsSpecific> os = Os();
		if (os)
		{
			switch (os->LocalAddress.sa_family)
			{
				case AF_INET: {
					sockaddr_in* addr = (sockaddr_in*)&os->LocalAddress;
					return ntohs(addr->sin_port);
				} break;

				case AF_INET6: {
					sockaddr_in6* addr = (sockaddr_in6*)&os->LocalAddress;
					return ntohs(addr->sin6_port);
				} break;
			}
		}

		return 0;
	}

	bool
	UdpSocketWindows::HasDatagram() const
	{
		return Poll(0);
	}

	bool
	UdpSocketWindows::Poll(uint32_t timeout) const
	{
		std::shared_ptr<OsSpecific> os = Os();
		if (!os) {
			return false;
		}

		WSAPOLLFD pollFd{ 0 };
		pollFd.fd = _osLifeCycle->Socket;
		pollFd.events = POLLRDNORM;

		int result = WSAPoll(&pollFd, 1, timeout);
		if (result == SOCKET_ERROR) {
			LogSocketError("WSAPoll");
			return false;
		}

		return result > 0
			&& (pollFd.revents == POLLRDNORM);
	}

	bool
	UdpSocketWindows::Bind(const char* hostname, uint16_t port) 
	{
		State expectedState = Inactive;
		if (! _state.compare_exchange_strong(expectedState, Binding) ) {
			Out() << "Socket is already bound." << std::endl;
			return false;
		}

		while (_activityCounter > 0) 
		{
			std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
		}

		std::shared_ptr<UdpSocketWindows::OsSpecific> os{ std::make_shared<OsSpecific>() };

		bool isIpv6 = IsHostnameIpv6(hostname);
		if (!CreateSocket(os.get(), isIpv6)) {
			return false;
		}

		if (!InitRecvMsg(os.get())) {
			return false;
		}

		if (!SetSocketOptions(os.get(), isIpv6)) {
			return false;
		}

		if (!BindSocket(hostname, port, os.get())) {
			return false;
		}

		Out() << "UDP socket addr: "
			<< AddrToStr(&os->LocalAddress)
			<< std::endl;

		// ************************************************************
		// Socket is bound. Update state.
		// ************************************************************
		std::swap(_osLifeCycle, os);
		_osHandle = _osLifeCycle;

		_state = Bound;

		return true;
	}

	bool 
	UdpSocketWindows::Unbind()
	{
		State expectedState = Bound;
		bool exchanged = false;

		do
		{
			expectedState = Bound;
			exchanged = _state.compare_exchange_weak(expectedState, Unbinding);
			if (expectedState == Inactive || expectedState == Unbinding) {
				return true;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds{10});
		} while(!exchanged);

		_osLifeCycle = nullptr;
		_state = Inactive;
		return true;
	}

	std::shared_ptr<tftplib::Datagram>
	UdpSocketWindows::Receive(DatagramFactory& factory)
	{
		std::shared_ptr<OsSpecific> os = Os();
		if (!os) {
			return nullptr;
		}
		
		DatagramAssembly assembly = factory.StartAssembly();
		if (!assembly.IsValid())
		{
			Err() << "[Socket] Could not allocate memory for datagram"
				<< std::endl;
			return nullptr;
		}

		// ************************************************************
		// Prepare WSAMSG struct and assign all of its buffers
		// ************************************************************
		sockaddr remoteHost = {0};

		WSABUF buffer;
		buffer.buf = assembly.GetDataBuffer();
		buffer.len = assembly.GetDataBufferSize();

		ULONG controlLen = IsIpv6()
			? WSA_CMSG_SPACE(sizeof(IN6_PKTINFO))
			: WSA_CMSG_SPACE(sizeof(IN_PKTINFO));

		if (controlLen > assembly.GetControlBufferSize()) 
		{
			Err()  << "Control buffer size is too small for expected data." 
				<< std::endl;
			Err() << "\tneed : "
				<< controlLen
				<< " got : "
				<< assembly.GetControlBufferSize()
				<< std::endl;
			return nullptr;
		}

		WSAMSG msg{ 0 };
		msg.dwBufferCount = 1;
		msg.lpBuffers = &buffer;
		msg.Control.buf = assembly.GetControlBuffer();
		msg.Control.len = controlLen;
		msg.name = &remoteHost;
		msg.namelen = sizeof(sockaddr);

		DWORD messageLength = 0;

		// ************************************************************
		// Receive the message
		// ************************************************************
		int result = os->fnRcvMsg(
			os->Socket, &msg,
			&messageLength,
			nullptr, nullptr);

		if (result != 0) {
			LogSocketError("rcvmsg");
			return nullptr;
		}

		// ************************************************************
		// Assign results to the assembly object
		// ************************************************************
		assembly.SetDataSize((uint16_t)messageLength);
		assembly.SetBroadcast( (msg.dwFlags & MSG_BCAST) != 0);
		assembly.SetDestinationPort( GetSocketPort() );

		// Source IP + Port
		switch (remoteHost.sa_family) {
			case AF_INET:
			{
				sockaddr_in* inet4 = (sockaddr_in*)&remoteHost;
				assembly.SetSourcePort(ntohs(inet4->sin_port));
				assembly.SetSourceAddress(AddrToStr(&inet4->sin_addr));
			} break;
				
			case AF_INET6: 
			{
				sockaddr_in6* inet6 = (sockaddr_in6*)&remoteHost;
				assembly.SetSourcePort(ntohs(inet6->sin6_port));
				assembly.SetSourceAddress(AddrToStr(&inet6->sin6_addr));
			} break;
		}

		// ************************************************************
		// Set the destination address in the assembly object.
		// ************************************************************
		size_t ctrlIndex = 0;
		
		WSACMSGHDR* header = nullptr;
		while ((header = WSA_CMSG_NXTHDR(&msg, header)) != nullptr) 
		{
			if (header->cmsg_level == IPPROTO_IP 
				&& header->cmsg_type == IP_PKTINFO) 
			{
				IN_PKTINFO* pktInfo = (IN_PKTINFO*)WSA_CMSG_DATA(header);
				assembly.SetDestinationAddress(AddrToStr(&pktInfo->ipi_addr));
				break;
			}
			else if (header->cmsg_level == IPPROTO_IPV6
				&& header->cmsg_type == IPV6_PKTINFO)
			{
				IN6_PKTINFO* pktInfo = (IN6_PKTINFO*)WSA_CMSG_DATA(header);
				assembly.SetDestinationAddress(AddrToStr(&pktInfo->ipi6_addr));
				break;
			}
		}

		return assembly.Finalize();
	}

	bool
	UdpSocketWindows::Send(std::shared_ptr<tftplib::Datagram> datagram)
	{
		std::shared_ptr<OsSpecific> os = Os();
		if (!os) {
			return false;
		}

		WSABUF buffer {0};
		buffer.buf = datagram->GetDataBuffer();
		buffer.len = datagram->GetDataSize();

		DWORD sentBytes = 0;
		
		AddrInfoBox boxed {};
		int result = ParseAddress(datagram->GetDestAddress(), 
			datagram->GetDestPort(), 
			&boxed.addrInfo,
			AF_UNSPEC);

		if( result != 0 ) 
		{
			LogSocketError("Send::ParseAddress");
			return false;
		}

		result = WSASendTo(
			os->Socket,
			&buffer, 1,
			&sentBytes,
			0,
			boxed.addrInfo->ai_addr, 
			static_cast<int>(boxed.addrInfo->ai_addrlen),
			nullptr, nullptr
		);

		if (result != 0)
		{
			LogSocketError("Send::SendTo");
			return false;
		}

		return true;
	}

/***************************************************************************
 *	U D P _ S O C K E T _ W I N D O W S   P R I V A T E   A P I
 ***************************************************************************/
	std::shared_ptr<UdpSocketWindows::OsSpecific> 
	UdpSocketWindows::Os() const
	{
		ActivityGuard guard(_activityCounter);

		if (!IsBound()) {
			return nullptr;
		}

		return _osHandle.lock();
	}

	bool
	UdpSocketWindows::CreateSocket(OsSpecific* os, bool isIpv6)
	{
		ProviderList providers {};

		os->Socket = WSASocket(
			isIpv6 ? AF_INET6 : AF_INET,
			SOCK_DGRAM,
			IPPROTO_UDP,
			providers.FindProvider(isIpv6),
			0,
			WSA_FLAG_OVERLAPPED
		);

		if (os->Socket == INVALID_SOCKET) {
			LogSocketError("socket creation");
			return false;
		}

		return true;
	}

	bool
	UdpSocketWindows::SetSocketOptions(OsSpecific* os, bool isIpv6)
	{
		DWORD ipPktInfo = 0x1337; // viva them truthy values.
		int result = setsockopt(os->Socket,
			isIpv6 ? IPPROTO_IPV6 : IPPROTO_IP,
			isIpv6 ? IPV6_PKTINFO : IP_PKTINFO,
			(char*)&ipPktInfo,
			sizeof(ipPktInfo));

		// Let's not check the result - if anything's up this will only fail on bind lol
		(void)result;

		return true;
	}

	bool
	UdpSocketWindows::BindSocket(const char* hostname,
		uint16_t port,
		OsSpecific* os)
	{

		// Parse host/port parameters into a sockaddr struct
		AddrInfoBox requestAddr {};
		int result = ParseAddress(hostname, port, 
			&requestAddr.addrInfo, 
			AF_UNSPEC);

		if (result != 0) {
			LogSocketError("getaddrinfo");
			return false;
		}

		// Bind the socket with request parameters
		result = bind(os->Socket,
			requestAddr.addrInfo->ai_addr,
			(int)requestAddr.addrInfo->ai_addrlen);

		if (result == SOCKET_ERROR) {
			LogSocketError("bind");
			return false;
		}

		// Copy assigned ip/port tuple to the os struct
		int namelen = sizeof(sockaddr);
		result = getsockname(
			os->Socket,
			&os->LocalAddress,
			&namelen
		);

		if (result != 0) {
			LogSocketError("getsockname");
			return false;
		}

		return true;
	}

	bool
	UdpSocketWindows::InitRecvMsg(OsSpecific* os)
	{
		GUID GUID_RcvMsg = WSAID_WSARECVMSG;
		LPFN_WSARECVMSG lpfnRcvMsg = nullptr;
		DWORD bytesReturned = 0;

		int result = WSAIoctl(os->Socket,
			SIO_GET_EXTENSION_FUNCTION_POINTER,
			&GUID_RcvMsg, sizeof(GUID),
			&lpfnRcvMsg, sizeof(lpfnRcvMsg),
			&bytesReturned, nullptr, nullptr);

		if (result != 0) {
			LogSocketError("Acquiring WSARecvMsg entry point");
			return false;
		}

		os->fnRcvMsg = lpfnRcvMsg;
		return true;
	}

	void
	UdpSocketWindows::LogSocketError(const char* what) const
	{
		int errorCode = WSAGetLastError();
		if (errorCode == 0) {
			return;
		}

		const char* errorMessage = nullptr;
		FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER
			| FORMAT_MESSAGE_FROM_SYSTEM
			| FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			errorCode,
			0,
			(LPSTR)&errorMessage,
			0,
			nullptr
		);

		Err() << what << " failed : "
			<< (errorMessage ? errorMessage : "Unknown error")
			<< "(" << errorCode << ")"
			<< std::endl;

		LocalFree((HLOCAL)errorMessage);
	}
}