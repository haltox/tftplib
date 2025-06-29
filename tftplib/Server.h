﻿#pragma once

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
#include "FileSecurityHandler.h"


namespace tftplib
{
	class ServerWorker;
	class Allocator;

	class Server
	{

	private:
		struct TransactionRecord {
			size_t socketId {0};

			uint16_t clientTID {0};
			uint16_t serverTID {0};

			std::atomic<bool> isActive {false};
		};
		
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

		FileSecurityHandler &FileSecurity();

	private:
		bool ValidateConfiguration() const;

		void MainServerThread();

		bool IsHandlingMaxTransactions() const;

		void ProcessNewTransactionRequest(
			std::shared_ptr<Datagram> &transactionRequest);

		std::shared_ptr<ServerWorker> AssignWorkerToTransaction(
			std::shared_ptr<Datagram>& transactionRequest);

		TransactionRecord* FindTransactionRecord(
			const std::function<bool(TransactionRecord*)>& filter) const;

		TransactionRecord* FindTransactionRecord( uint16_t ctid, uint16_t stid) const;

		TransactionRecord* FindFreeTransactionRecord() const;

		bool TerminateTransaction( uint16_t clientTid, uint16_t serverTid );

	private:
		// Reply functions
		void ReplyRejectTransactionNoWorkerAvailable(const Datagram& transactionRequest);

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
		
		FileSecurityHandler _fileSecurity;

		UdpSocketWindows _controlSocket {};
		std::vector< std::shared_ptr<UdpSocketWindows>> _transactionSockets {};

		std::thread _dispatchThread {};
		std::vector<std::shared_ptr<ServerWorker>> _workers;
		
		TransactionRecord *_transactions;


		std::atomic<bool> _running{ false };
		std::atomic<bool> _starting{ false };
		std::atomic<bool> _stopping{ false };

		friend class ServerWorker;
	};
}

