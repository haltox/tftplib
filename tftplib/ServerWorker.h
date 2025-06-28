#pragma once

#include "Datagram.h"
#include <memory>
#include <cstdint>
#include <thread>
#include <vector>
#include "RingBuffer.h"
#include "Signal.h"
#include <ostream>
#include "UdpSocketWindows.h"
#include "DatagramFactory.h"
#include <filesystem>
#include "tftp_messages.h"
#include "FileSecurityHandler.h"
#include "FileWriter.h"

namespace tftplib {
	class Server;
	class MessageRequest;
	class FileWriter;

	class ServerWorker
	{
	public:
		enum class ActivityState {
			INACTIVE,					// Worker thread is not running
			
			ACTIVE,						// Worker thread is running
			
			TERMINATING					// Worker thread is shutting down
										// Will transition to inactive.
		};

		enum class TransactionState {
			INACTIVE,					// Worker thread is not running

			WAITING_FOR_REQUEST,		// Worker thread is running
										// but not currently handling a transaction

			SETTING_UP_REQUEST,			// Setting up state for request handling

			PROCESSING_REQUEST,			// Worker thread is processing 
										// a transaction request

			WAITING_FOR_DATA,			// Worker thread is waiting for data 
										// from the client

			SENDING_DATA,				// Worker thread is waiting for ACK from 
										// the client
			
			WAITING_FOR_ACK,			// Worker thread is waiting for ACK from 
										// the client

			TERMINATING					// Worker thread is shutting down
										// Will transition to inactive.
		};


	public:
		
		ServerWorker(Server &parent, 
			std::weak_ptr<DatagramFactory> factory);

		~ServerWorker();

		// Thread handling
		void Start();

		void RequestStop();

		void Stop();

		// Transaction handling
		void AssignTransaction(std::shared_ptr<Datagram>& transactionRequest,
			std::shared_ptr<UdpSocketWindows> socket );

		bool IsBusy() const;

		std::ostream& Out();
		std::ostream& Err();

	private:
	
		enum class MessageErrorCategory
		{
			NO_ERROR,

			// State and operation sequencing errors
			INVALID_STATE,
			INVALID_OPCODE,
			INVALID_BLOCK,
			TIMEOUT,
			
			// Message structure errors
			INVALID_MESSAGE_SIZE,
			INVALID_MESSAGE_FORMAT,
			
			// Message error
			INVALID_MODE,

			// File errors
			NO_SUCH_FILE,
			ACCESS_FORBIDDEN,
			FILE_LOCKED,
			UNSAFE_PATH,

			// Received an error from the client - abort processing.
			CLIENT_ERROR,

			// Critical server error - abort processing.
			CRITICAL_SERVER_ERROR
		};

	private:
		void Run();

		void ProcessActivityStateChange();
		void ProcessTransactionState();

		void ProcessWaitingForDataState();
		void ProcessSendingDataState();
		void ProcessWaitingForAckState();

		/* ***************************************************
		 *  Message Processing : RRQ and WRQ
		 * ***************************************************/
		
		void ProcessRequestMessage(
			std::shared_ptr<Datagram>& transactionRequest );
		
		MessageErrorCategory ProcessRequestMessage(
			const MessageRequest* rwrq);

		void RejectTransactionRequest(MessageErrorCategory errorReason);

		/* ***************************************************
		 *  Message Processing : Data
		 * ***************************************************/

		MessageErrorCategory ProcessDataMessage(
			const std::shared_ptr<Datagram>& dataMessage );

		/* ***************************************************
		 *  General state and message handling
		 * ***************************************************/

		bool Ack(uint16_t ack);

		bool Error(ErrorCode errorCode);

		bool Error(MessageErrorCategory errorCode);

		bool ErrorWithMessage(ErrorCode errorCode, const char* msg);

		bool Abort(MessageErrorCategory error, bool sendErrorMsg = true);

		bool SendMessage(std::shared_ptr<Datagram> &datagram);

		bool TerminateTransaction();

		/* ***************************************************
		 *  General utility functions
		 * ***************************************************/

		MessageErrorCategory 
		FileSecurityErrorToMessageError(
			FileSecurityHandler::ValidationResult fse) const;

		const char* MessageErrorCategoryToString(MessageErrorCategory mec) const;

		template<typename T, typename... Args>
		std::shared_ptr<Datagram>
		MakeMessageDatagram(std::weak_ptr<DatagramFactory>& factoryHandle,
				Args... args);

	private:
		std::thread _thread {};
		Signal _signal{};

		// State handling
		std::atomic<ActivityState> _activity {ActivityState::INACTIVE};
		std::atomic<TransactionState> _state { TransactionState::INACTIVE };

		// Transaction resources and settings
		std::string _clientHost {""};
		uint16_t _clientTid {0};
		uint16_t _serverTid{ 0 };
		uint16_t _lastAck {0};

		OpCode _currentOperation { OpCode::UNDEF };
		bool _asciiMode {false};

		std::filesystem::path _filePath {""};
		bool _fileLocked {false};
		std::unique_ptr<FileWriter> _fw {nullptr};

		std::shared_ptr<UdpSocketWindows> _socket{nullptr};

		// General settings
		std::chrono::milliseconds _transactionTimeout {1000};
		uint16_t _dataBlockSize {512};

		// 
		Server &_parent;
		std::weak_ptr<DatagramFactory> _factory;
	};
}

namespace tftplib {
	template<typename T, typename... Args>
	std::shared_ptr<Datagram>
	ServerWorker::MakeMessageDatagram(std::weak_ptr<DatagramFactory>& factoryHandle,
		Args... args)
	{
		auto factory = factoryHandle.lock();
		if (!factory) return nullptr;

		auto assembly = factory->StartAssembly()
			.SetDestinationAddress(_clientHost)
			.SetDestinationPort(_clientTid);

		T* message = T::create(args...,
			[&assembly](size_t sz) {
				assembly.SetDataSize(static_cast<uint16_t>(sz));
				return assembly.GetDataBuffer();
			});

		if (message == nullptr)
		{
			return nullptr;
		}

		return assembly.Finalize();
	}
}

