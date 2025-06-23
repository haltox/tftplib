#pragma once

#include "pch.h"

#include "tftp_messages.h"
#include <memory>
#include <string>

#include <filesystem>
#include "streambuf_noop.h"

#include "UdpSocketWindows.h"
#include <thread>
#include "DatagramFactory.h"
#include <unordered_map>

#include <mutex>

namespace tftplib
{
	class ServerWorker;
	class Allocator;

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

		void MainServerThread();

		bool IsHandlingMaxTransactions() const;

		void ProcessNewTransactionRequest(
			std::shared_ptr<Datagram> transactionRequest);

		std::shared_ptr<ServerWorker> AssignWorkerToTransaction(
			const std::string &host,
			uint16_t requestId);

		void DispatchMessageToWorker(
			std::shared_ptr<Datagram> message, 
			ServerWorker& worker);

		std::string MakeTransactionKey(
			const std::string& host,
			uint16_t requestId) const;

		std::string MakeTransactionKey(const Datagram& request) const;
	private:
		// Reply functions
		void ReplyRejectTransactionNoWorkerAvailable(const Datagram& transactionRequest);

	private:
		struct TransactionRecord {
			size_t socketId;
			
			uint16_t clientTID;
			uint16_t serverTID;
		};

	private:

		mutable std::ostream* _out{ nullptr };
		mutable std::ostream* _err{ nullptr };

		streambuf_noop _noopBuf;
		mutable std::ostream _noopOstream;

		// Server settings
		uint16_t _port{ tftplib::defaults::ServerPort }; // Default TFTP port
		std::string _host{ "0.0.0.0" };
		std::filesystem::path _rootDirectory{};
		uint32_t _timeoutMs{1000};
		uint32_t _threadCount{1};
		uint16_t _blockSize { tftplib::defaults::BlockSize };
		uint32_t _messagePoolSize { 64000 };

		// Server state
		std::unique_ptr<UdpSocketWindows::GlobalOsContext> _osContext;
		std::shared_ptr<Allocator> _alloc;
		std::shared_ptr<DatagramFactory> _factory {nullptr};

		UdpSocketWindows _controlSocket {};
		std::vector< std::shared_ptr<UdpSocketWindows>> _transactionSockets {};

		std::thread _dispatchThread {};
		std::vector<std::shared_ptr<ServerWorker>> _workers;
		std::unordered_map<uint64_t, TransactionRecord> _transactions {};
		std::atomic<bool> _running{ false };
		std::atomic<bool> _starting{ false };
		std::atomic<bool> _stopping{ false };

		friend class ServerWorker;
	};
}

