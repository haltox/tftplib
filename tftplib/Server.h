#pragma once

#include "pch.h"

#include "tftp_messages.h"
#include <memory>
#include <string>

#include <filesystem>
#include "streambuf_noop.h"


namespace tftplib
{
	class Server
	{
		
	public:
		Server();
		~Server();
		Server(const Server&) = delete;
		Server& operator=(const Server&) = delete;
		Server(Server&&) = delete;

		void Start();
		void Stop();

		// To set before starting server
		Server& SetPort(uint16_t port);
		Server& SetHost(const std::string& host);
		Server& SetRootDirectory(const std::filesystem::path &root);
		Server& SetTimeout(uint32_t timeoutMs);
		Server& SetThreadCount(uint32_t max);

		Server& SetOutStream(std::ostream *os);
		Server& SetErrStream(std::ostream* os);

		std::ostream& Out() const;
		std::ostream& Err() const;
	private:
		bool ValidateConfiguration() const;


	private:

		mutable std::ostream* _out{ nullptr };
		mutable std::ostream* _err{ nullptr };

		streambuf_noop _noopBuf;
		mutable std::ostream _noopOstream;

		uint16_t _port{ tftplib::defaults::ServerPort }; // Default TFTP port
		std::string _host{};
		std::filesystem::path _rootDirectory{};
		uint32_t _timeoutMs{1000};
		uint32_t _threadCount{1};
	};
}

