#include "pch.h"
#include "Server.h"
#include <thread>

#include "ServerWorker.h"
#include "tftp_messages.h"
#include "Allocator.h"

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

		_fileSecurity.Reset()
			.SetFileCreationPolicy(FileSecurityHandler::FileCreationPolicy::ALLOW)
			.SetOverwritePolicy(FileSecurityHandler::OverwritePolicy::ALLOW)
			.SetRootDirectory(_rootDirectory);

		_factory = DatagramFactory::Instantiate(_threadCount * 8);
		_alloc = std::make_shared<Allocator>(_messagePoolSize);
		_controlSocket.Bind(_host.c_str(), _port);

		_transactions = new TransactionRecord[_threadCount];

		for (uint32_t i = 0; i < _threadCount; i++)
		{
			auto socket = std::make_shared<UdpSocketWindows>();
			_transactionSockets.push_back(socket);

			auto worker = std::make_shared<ServerWorker>(*this, _factory);
			worker->Start();
			_workers.push_back(worker);
		}

		_dispatchThread = std::thread(&Server::MainServerThread, this);

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

			for (auto& socket : _transactionSockets) {
				socket->Unbind();
			}

			_controlSocket.Unbind();
			_alloc = nullptr;
			_factory = nullptr;

			_workers.clear();
			_transactionSockets.clear();
			delete[] _transactions;
		}

		// Stopping is complete - release lock
		_stopping = false;
	}

	void Server::MainServerThread()
	{
		_running = true;

		while (!_stopping)
		{
			while (!_controlSocket.Poll(100) && !_stopping) ;

			if (_stopping) {
				break;
			}

			std::shared_ptr<Datagram> datagram = _controlSocket.Receive(*_factory);
			if (datagram == nullptr || !datagram->IsValid()) {
				Err() << "Received invalid datagram or out of buffers. Waiting a bit." << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			if (datagram->GetDataSize() < sizeof(OpCode)) 
			{
				Err() << "Received invalid message. Ignoring." << std::endl;
				continue;
			}

			OpCode *op = (OpCode *)datagram->GetData();
			Out() << "Control socket receive: " << OpCodeToStr(*op) << " from "
				<< datagram->GetSourcePort() 
				<< ":" << datagram->GetSourcePort()
				<< std::endl;

			switch (*op)
			{
				case OpCode::RRQ:
				case OpCode::WRQ:
					Out() << "Received request message" << std::endl;
					ProcessNewTransactionRequest(datagram);
					break;

				case OpCode::ACK:
					if (datagram->GetDataSize() < sizeof(MessageAck))
					{
						Out() << "Ignoring malformed ACK" << std::endl;
					}
					else 
					{
						MessageAck *ack = (MessageAck*)datagram->GetData();
						Out() << "Ignoring ACK " << ack->getBlockNumber() << std::endl;
					}
					break;

				default:
					Err() << "Ignoring unexpected message" << std::endl;
					break;
			}
		}

		_running = false;
	}

	void 
	Server::ProcessNewTransactionRequest(
		std::shared_ptr<Datagram> &transactionRequest)
	{
		Out() << "Received message on control port from "
			<< transactionRequest->GetSourceAddress()
			<< ":"  << transactionRequest->GetSourcePort()
			<< std::endl;

		// Check if we are handling max transactions
		if (IsHandlingMaxTransactions()) {
			Err() << "Rejecting transaction from "
				<< transactionRequest->GetSourceAddress() 
				<< ":" 
				<< transactionRequest->GetSourcePort()
				<< " due to max transactions limit." 
				<< std::endl;
			
			ReplyRejectTransactionNoWorkerAvailable(*transactionRequest);
		}
		else {
			auto worker = AssignWorkerToTransaction(transactionRequest);

			if (worker == nullptr) 
			{
				ReplyRejectTransactionNoWorkerAvailable(*transactionRequest);
			}
		}
	}

	bool Server::IsHandlingMaxTransactions() const
	{
		return FindFreeTransactionRecord() == nullptr;
	}

	Server::TransactionRecord*
	Server::FindTransactionRecord(
		const std::function<bool(Server::TransactionRecord*)>& filter) const
	{
		for (unsigned int i = 0; i < _threadCount; i++)
		{
			if (filter(&_transactions[i]))
			{
				return &_transactions[i];
			}
		}
		return nullptr;
	}

	Server::TransactionRecord*
	Server::FindTransactionRecord(uint16_t ctid, uint16_t stid) const
	{
		return FindTransactionRecord([ctid, stid](TransactionRecord* tr) {
			return tr->clientTID == ctid
				&& tr->serverTID == stid;
		});
	}

	Server::TransactionRecord* Server::FindFreeTransactionRecord() const
	{
		return FindTransactionRecord([](TransactionRecord* tr) {
			return !tr->isActive;
		});
	}

	bool 
	Server::TerminateTransaction(uint16_t clientTid, uint16_t serverTid)
	{
		auto it = FindTransactionRecord(clientTid, serverTid);
		if( it == nullptr ) 
		{
			Err() << "[Server] cannot find transaction " 
				<< clientTid << "/" << " for termination."
				<< std::endl;
			return false;
		}

		auto socket = _transactionSockets[it->socketId];
		socket->Unbind();

		it->isActive = false;
		return true;
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
			static_cast<uint16_t>(message->Size()),
			transactionRequest);

		_controlSocket.Send(response);
	}

	std::shared_ptr<ServerWorker> 
	Server::AssignWorkerToTransaction(
		std::shared_ptr<Datagram>& transactionRequest)
	{
		size_t freeSocket = 0;
		std::shared_ptr<UdpSocketWindows> socket = nullptr;
		for( ; freeSocket < _transactionSockets.size(); freeSocket++ )
		{
			socket = _transactionSockets[freeSocket];
			if (!socket->IsBound())
			{
				socket->Bind(_host.c_str(), 0);
				break;
			}
		}

		if (socket == nullptr) 
		{
			Err() << "Couldn't find free socket for incoming transaction!" << std::endl;
			return nullptr;
		}

		uint16_t clientTid = transactionRequest->GetSourcePort();
		uint16_t serverTid = socket->GetLocalPort();
		auto * record = FindFreeTransactionRecord();
		if (record == nullptr)
		{
			Err() << "[Server] Couldn't find free record for transaction" << std::endl;
			return nullptr;
		}

		for (size_t i = 0; i < _workers.size(); ++i)
		{
			if ( !_workers[i]->IsBusy()) 
			{
				record->socketId = freeSocket;
				record->clientTID = clientTid;
				record->serverTID = serverTid;
				record->isActive = true;
				
				_workers[i]->AssignTransaction(transactionRequest, socket);

				return _workers[i];
			}
		}

		// Shouldn't happen.
		Err()  << "No available worker to handle transaction : "
			<< clientTid << "/" << serverTid
			<< std::endl;

		return nullptr;
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

	FileSecurityHandler& Server::FileSecurity()
	{
		return _fileSecurity;
	}
};