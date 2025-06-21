#include "pch.h"
#include "Server.h"
#include <thread>

#include "ServerWorker.h"
#include "tftp_messages.h"
#include "Allocator.h"

#define SUPER_TEMP_DEBUGGING 1

namespace tftplib {
	
	template<typename T, typename... Args>
	std::unique_ptr<T, std::function<void(void*)>>
		MakeManagedMessage(std::shared_ptr<Allocator>& alloc, Args... args)
	{
		return std::unique_ptr<T, std::function<void(void*)>>{
			T::create(args...,
				[alloc](size_t size) { return alloc->allocate(size); }),
				[alloc](void* ptr) { alloc->free(ptr); }};
	}

	Server::Server() 
		: _out{ nullptr }
		, _err{ nullptr }
		, _noopOstream{ &_noopBuf }
		, _osContext{ UdpSocketWindows::InitGlobalOsContext() }
	{
	}

	Server::~Server() 
	{
		Stop();
	}

	void Server::Start() {
		if (_running || _starting) {
			Out() << "Server is already running." << std::endl;
			return;
		}

		while (_stopping) {
			Out() << "Server is stopping, waiting for it to finish." << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		if (_starting.exchange(true)) {
			Out() << "Server is already starting." << std::endl;
			return;
		}

		if (_stopping) {
			Out() << "Server is stopping, cannot start." << std::endl;
			_starting = false;
			return;
		}

		if (!ValidateConfiguration()) {
			throw std::runtime_error("Invalid server configuration");
		}

		_factory = DatagramFactory::Instantiate(_threadCount * 8);
		_alloc = std::make_shared<Allocator>(_messagePoolSize);
		_socket.Bind(_host.c_str(), _port);

		for (uint32_t i = 0; i < _threadCount; i++)
		{
			auto worker = std::make_shared<ServerWorker>(*this);
			worker->Start();
			_workers.push_back(worker);
		}

		_dispatchThread = std::thread(&Server::DispatchJob, this);

		_starting = false;
	}

	void Server::Stop() {
		if (!_running) {
			return;
		}

		// Lock stopping routine and indicates to dispatch thread to stop
		if (_stopping.exchange(true)) {
			Out() << "Server is already stopping." << std::endl;
			return;
		}

		while (_starting) {
			Out() << "Server is stopping, waiting for it to finish." << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		// Cleanup block
		{
			_dispatchThread.join();

			for (auto& worker : _workers) {
				worker->RequestStop();
			}

			for (auto& worker : _workers) {
				worker->Stop();
			}

			_socket.Unbind();
			_alloc = nullptr;
			_factory = nullptr;

			_workers.clear();
			_transactions.clear();
		}

		// Stopping is complete - release lock
		_stopping = false;
	}

	void Server::DispatchJob()
	{
		_running = true;

		while (!_stopping)
		{
			while (!_socket.Poll(100) && !_stopping) ;

			if (_stopping) {
				break;
			}

			std::shared_ptr<Datagram> datagram = _socket.Receive(*_factory);
			if (datagram == nullptr || !datagram->IsValid()) {
				Err() << "Received invalid datagram or out of buffers. Waiting a bit." << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			std::string transactionKey {MakeTransactionKey(*datagram)};
			auto it = _transactions.find(transactionKey);
			if (it == _transactions.end() ) {
				ProcessNewTransactionRequest(datagram);
			}
			else {
				auto worker = _workers[it->second];
				DispatchMessageToWorker(datagram, *worker);
			}
		}

		_running = false;
	}

	void 
	Server::ProcessNewTransactionRequest(
		std::shared_ptr<Datagram> transactionRequest)
	{
		uint16_t port = transactionRequest->GetSourcePort();
		std::string host = transactionRequest->GetSourceAddress();

		// Check if we are handling max transactions
		if (IsHandlingMaxTransactions()) {
			Err() << "Rejecting transaction from "
				<< transactionRequest->GetSourceAddress() 
				<< ":" 
				<< port
				<< " due to max transactions limit." 
				<< std::endl;
			
			ReplyRejectTransactionNoWorkerAvailable(*transactionRequest);
		}
		else {
			auto worker = AssignWorkerToTransaction(host, port);

			if (worker == nullptr) 
			{
				ReplyRejectTransactionNoWorkerAvailable(*transactionRequest);
			}
			else
			{
				DispatchMessageToWorker(transactionRequest, *worker);
			}
		}
	}

	bool Server::IsHandlingMaxTransactions() const
	{
		return _transactions.size() >= _threadCount;
	}

	std::string 
	Server::MakeTransactionKey(
		const std::string& host,
		uint16_t requestId) const
	{
		return host + ":" + std::to_string(requestId);
	}

	std::string 
	Server::MakeTransactionKey( const Datagram& request ) const
	{
		return MakeTransactionKey(
			request.GetSourceAddress(),
			request.GetSourcePort());
	}

	void 
	Server::ReplyRejectTransactionNoWorkerAvailable(
		const Datagram& transactionRequest)
	{
		auto message = MakeManagedMessage<MessageError>(
			_alloc,
			tftplib::ErrorCode::DISK_FULL);

		if(message == nullptr)
		{
			return;
		}

		auto response = _factory->BuildResponse(
			(const uint8_t*)message.get(),
			message->Size(),
			transactionRequest);

		_socket.Send(response);
	}

	std::shared_ptr<ServerWorker> 
	Server::AssignWorkerToTransaction(const std::string& host, uint16_t requestId)
	{
		std::lock_guard<std::mutex> lock{ _mutex };

		std::string transactionKey{ host, requestId };
		for (size_t i = 0; i < _workers.size(); ++i)
		{
			if ( !_workers[i]->IsBusy()) {
				_workers[i]->AssignTransaction(requestId);
				_transactions[transactionKey] = i;
				return _workers[i];
			}
		}

		// Shouldn't happen.
		Err()  << "No available worker to handle transaction : "
			<< transactionKey
			<< std::endl;

		return nullptr;
	}

	void 
	Server::DispatchMessageToWorker(
		std::shared_ptr<Datagram> message, 
		ServerWorker& worker)
	{
		worker.DispatchMessage(message);
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

		#ifdef SUPER_TEMP_DEBUGGING
		return true;
		#endif

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