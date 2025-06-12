#include "pch.h"
#include "Server.h"


namespace tftplib {
	
	Server::Server() 
		: _out{ nullptr }
		, _err{ nullptr }
		, _noopOstream{ &_noopBuf }
	{

	}

	Server::~Server() {
		
	}



	void Server::Start() {
		if (!ValidateConfiguration()) {
			throw std::runtime_error("Invalid server configuration");
		}


	}

	void Server::Stop() {

	}

	// Setters for server configuration
	Server& Server::SetPort(uint16_t port) {
		_port = port;
		return *this;
	}

	Server& Server::SetHost(const std::string& host) {
		_host = host;
		return *this;
	}

	Server& Server::SetRootDirectory(const std::filesystem::path& root) {
		_rootDirectory = root;
		return* this;
	}

	Server& Server::SetTimeout(uint32_t timeoutMs) {
		_timeoutMs = timeoutMs;
		return *this;
	}

	Server& Server::SetThreadCount(uint32_t max) {
		_threadCount = max;
		return *this;
	}

	Server& Server::SetOutStream(std::ostream* os) {
		_out = os;
		return *this;
	}

	Server& Server::SetErrStream(std::ostream* os) {
		_err = os;
		return *this;
	}

	bool Server::ValidateConfiguration() const {
		if (_rootDirectory.empty()) {
			Err() << "Root directory is not set." << std::endl;
			return false;
		}

		if (!std::filesystem::exists(_rootDirectory)) {
			Err() << "Root directory '" << _rootDirectory << "' does not exist." << std::endl;
			return false;
		}
			
		if(!std::filesystem::is_directory(_rootDirectory)) {
			Err() << "Root directory '" << _rootDirectory << "' is not a directory." << std::endl;
			return false;
		}

		return true;
	}

	std::ostream& Server::Out() const {
		return _out != nullptr ? *_out : _noopOstream;
	}

	std::ostream& Server::Err() const {
		return _err != nullptr ? *_err : _noopOstream;
	}
};