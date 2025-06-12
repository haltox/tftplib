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

namespace tftplib
{
	std::string AddrToStr(IN_ADDR* addr)
	{
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, addr, ip, sizeof(ip));
		return std::string(ip);
	}

	std::string AddrToStr(IN6_ADDR* addr)
	{
		char ip[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, addr, ip, sizeof(ip));
		return std::string(ip);
	}

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

	struct UdpSocketWindows::OsSpecific {
		addrinfo* LocalAddress{ nullptr };
		SOCKET ListenSocket{};
		LPFN_WSARECVMSG fnRcvMsg{ nullptr };

		~OsSpecific() {
			if (LocalAddress) {
				freeaddrinfo(LocalAddress);
				LocalAddress = nullptr;
			}
			if (ListenSocket != INVALID_SOCKET) {
				closesocket(ListenSocket);
				ListenSocket = INVALID_SOCKET;
			}
		}
	};


	UdpSocketWindows::UdpSocketWindows()
		: _out{ nullptr }
		, _err{ nullptr }
		, _state{ Inactive }
		, _os{ std::make_unique<OsSpecific>() }
	{
		InitWSA();
	}

	UdpSocketWindows::~UdpSocketWindows() 
	{
		WSACleanup();
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

	bool
	UdpSocketWindows::Bind(const char* hostname, uint16_t port) 
	{
		if (GetState() != Inactive) {
			Out() << "Socket is already bound or listening." << std::endl;
			return false;
		}

		std::unique_ptr<UdpSocketWindows::OsSpecific> os{ std::make_unique<OsSpecific>() };

		if (!PrepareOSAddress(hostname, port, os.get())) {
			return false;
		}

		Out() << "UDP socket addr: "
			<< AddrToStr(os->LocalAddress->ai_addr)
			<< std::endl;

		if (!CreateSocket(os.get())) {
			return false;
		}

		if (!InitRecvMsg(os.get())) {
			return false;
		}

		if (!SetSocketOptions(os.get())) {
			return false;
		}

		if (!BindSocket(os.get())) {
			return false;
		}

		// ************************************************************
		// Socket is bound. Update state.
		// ************************************************************
		std::swap(_os, os);
		_state = Bound;

		return true;
	}

	uint16_t 
	UdpSocketWindows::GetSocketPort() const
	{
		if (!IsBound()) {
			Err() << "Socket is not bound." << std::endl;
			return 0;
		}

		switch (_os->LocalAddress->ai_addr->sa_family) 
		{
			case AF_INET: {
				sockaddr_in* inet4 = (sockaddr_in*)_os->LocalAddress->ai_addr;
				return ntohs(inet4->sin_port);
			}
			case AF_INET6: {
				sockaddr_in6* inet6 = (sockaddr_in6*)_os->LocalAddress->ai_addr;
				return ntohs(inet6->sin6_port);
			}
		}

		return 0;
	}

	bool
	UdpSocketWindows::HasDatagram() const
	{
		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(_os->ListenSocket, &readSet);

		timeval timeout{ 0 };

		return select(0, &readSet, nullptr, nullptr, &timeout);
	}

	std::shared_ptr<tftplib::Datagram>
	UdpSocketWindows::Receive(DatagramFactory& factory)
	{
		if (!IsBound()) {
			Err() << "Socket is not ready to receive messages." << std::endl;
			return nullptr;
		}
		
		DatagramAssembly assembly = factory.StartAssembly();

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
		int result = _os->fnRcvMsg(
			_os->ListenSocket, &msg,
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

	bool UdpSocketWindows::IsIpv6() const {
		if (!IsBound()) {
			Err() << "Socket is not bound." << std::endl;
			return false;
		}
		return _os->LocalAddress->ai_family == AF_INET6;
	}

	void UdpSocketWindows::InitWSA() {
		WSADATA wsaData;
		int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (result != 0) {
			Err() << "WSAStartup failed with error: "
				<< result
				<< std::endl;
			throw std::runtime_error("WSAStartup failed");
		}

		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
			WSACleanup();
			Err() << "Could not find a usable version of Winsock."
				<< std::endl;
			throw std::runtime_error("Could not find a usable version of Winsock.");
		}
	}

	bool
	UdpSocketWindows::PrepareOSAddress(const char* hostname,
			uint16_t port,
			OsSpecific* os)
	{
		addrinfo hints{};
		hints.ai_family = AF_INET;			// IPv4
		hints.ai_socktype = SOCK_DGRAM;		// UDP
		hints.ai_protocol = IPPROTO_UDP;	// UDP protocol
		hints.ai_flags = AI_PASSIVE;

		std::string portStr = std::to_string(port);

		int result = getaddrinfo(hostname,
			portStr.c_str(),
			&hints,
			&os->LocalAddress);

		if (result != 0) {
			LogSocketError("getaddrinfo");
			return false;
		}

		return true;
	}

	bool
	UdpSocketWindows::CreateSocket(OsSpecific* os)
	{
		static const TCHAR* MSAFD_TCPIP_PROVIDER = _T("{E70F1AA0-AB8B-11CF-8CA3-00805F48A192}");

		WSAPROTOCOL_INFO protocolInfo = { 0 };
		HRESULT result = CLSIDFromString(MSAFD_TCPIP_PROVIDER,
			&protocolInfo.ProviderId);

		if (result != S_OK) {
			Err() << "Critical: Failed to convert provider GUID into the proper format. "
				<< std::endl;

			return false;
		}

		os->ListenSocket = WSASocket(
			os->LocalAddress->ai_family,
			os->LocalAddress->ai_socktype,
			os->LocalAddress->ai_protocol,
			&protocolInfo,
			0,
			WSA_FLAG_OVERLAPPED
		);

		os->ListenSocket = socket(os->LocalAddress->ai_family,
			os->LocalAddress->ai_socktype,
			os->LocalAddress->ai_protocol);

		if (os->ListenSocket == INVALID_SOCKET) {
			LogSocketError("socket creation");
			return false;
		}

		return true;
	}

	bool
	UdpSocketWindows::SetSocketOptions(OsSpecific* os)
	{
		bool isIpv6 = os->LocalAddress->ai_family == AF_INET6;

		DWORD ipPktInfo = 0x1337; // viva them truthy values.
		int result = setsockopt(os->ListenSocket,
			isIpv6 ? IPPROTO_IPV6 : IPPROTO_IP,
			isIpv6 ? IPV6_PKTINFO : IP_PKTINFO,
			(char*)&ipPktInfo,
			sizeof(ipPktInfo));

		// Let's not check the result - if anything's up this will only fail on bind lol
		(void)result;

		return true;
	}

	bool
		UdpSocketWindows::BindSocket(OsSpecific* os)
	{
		int result = bind(os->ListenSocket,
			os->LocalAddress->ai_addr,
			(int)os->LocalAddress->ai_addrlen);

		if (result == SOCKET_ERROR) {
			LogSocketError("bind");
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

		int result = WSAIoctl(os->ListenSocket,
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