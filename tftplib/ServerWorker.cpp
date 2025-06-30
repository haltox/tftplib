#include "pch.h"
#include "ServerWorker.h"
#include "Server.h"
#include <thread>
#include <string>
#include "HaloBuffer.h"
#include "FileReader.h"

namespace tftplib {
	class Finalizer {
		private:
			std::function<void()> _finale;
			bool _cancelled {false};

		public:
			Finalizer(std::function<void()> finale) 
			: _finale { finale }
			{}

			~Finalizer() {
				if( !_cancelled ) _finale();
			}

			void Abort() { _cancelled = true; }
	};

	ServerWorker::ServerWorker(Server& parent, 
		std::weak_ptr<DatagramFactory> factory)
		: _fw {nullptr}
		, _fr{ nullptr }
		, _parent{parent}
		, _fileBuffer { std::make_unique<HaloBuffer>(0x020000)  }
		, _factory { factory }
	{
	}

	ServerWorker::~ServerWorker(){}

	// Thread handling
	void ServerWorker::Start()
	{
		ActivityState expected = ActivityState::INACTIVE;
		ActivityState desired = ActivityState::ACTIVE;
		if ( !_activity.compare_exchange_strong(expected, desired) )
		{
			Err() << "ServerWorker::Start() called while worker is already running."
				<< std::endl;
			return;
		}

		_thread = std::thread(&ServerWorker::Run, this);
	}

	void ServerWorker::RequestStop()
	{
		ActivityState expected = ActivityState::ACTIVE;
		ActivityState desired = ActivityState::TERMINATING;
		if (_activity.compare_exchange_strong(expected, desired))
		{
			_signal.EmitSignal();
		}
	}

	void ServerWorker::Stop()
	{
		RequestStop();
		if( _thread.joinable() ) 
		{
			_thread.join();
		}

		_socket = nullptr;

		_clientHost = "";
		_clientTid = 0;
		_serverTid = 0;
		_lastAck = 0;
		_filePath = "";
		_asciiMode = false;
		_signal.Reset();
	}

	// Transaction handling
	void ServerWorker::AssignTransaction(
		std::shared_ptr<Datagram>& transactionRequest,
		std::shared_ptr<UdpSocketWindows> socket)
	{
		uint16_t clientTid = transactionRequest->GetSourcePort();
		uint16_t serverTid = socket->GetLocalPort();

		{
			TransactionState expected{ TransactionState::WAITING_FOR_REQUEST };
			TransactionState desired{ TransactionState::SETTING_UP_REQUEST };
			if (!_state.compare_exchange_strong(expected, desired))
			{
				Err() << "AssignTransaction(" 
					<< clientTid << "," << serverTid 
					<< ") called while : ";
				if (expected == TransactionState::INACTIVE)
				{
					Err() << "worker is inactive" << std::endl;
				}
				else if (expected == TransactionState::TERMINATING)
				{
					Err() << "worker is terminating" << std::endl;
				}
				else
				{
					Err() << "worker is handling transaction " 
						<< _clientTid << "/" << _serverTid
						<< std::endl;
				}
				return;
			}
		}

		_lastAck = 0;
		_clientHost = transactionRequest->GetSourceAddress();
		_clientTid = clientTid;
		_serverTid = serverTid;
		_socket = socket;
		
		TransactionState expected { TransactionState::SETTING_UP_REQUEST };
		TransactionState desired{ TransactionState::PROCESSING_REQUEST };
		if (!_state.compare_exchange_strong(expected, desired)) 
		{
			Err() << "Serious threading error in AssignTransaction" << std::endl;
		}

		ProcessRequestMessage(transactionRequest);
	}

	bool ServerWorker::IsBusy() const
	{
		return _state != TransactionState::WAITING_FOR_REQUEST;
	}

	std::ostream& ServerWorker::Out()
	{
		return _parent.Out();
	}

	std::ostream& ServerWorker::Err()
	{
		return _parent.Err();
	}

	void ServerWorker::Run()
	{
		_state = TransactionState::WAITING_FOR_REQUEST;
		
		while (_activity == ActivityState::ACTIVE)
		{
			while (_activity == ActivityState::ACTIVE 
				&& _state == TransactionState::WAITING_FOR_REQUEST )
			{
				_signal.WaitForSignal( std::chrono::seconds{1});
			}

			ProcessActivityStateChange();
			ProcessTransactionState();
		}

		_state = TransactionState::INACTIVE;
		_activity = ActivityState::INACTIVE;
	}

	void ServerWorker::ProcessActivityStateChange()
	{
		if (_activity == ActivityState::ACTIVE
			|| _activity == ActivityState::INACTIVE)
		{
			return;
		}

		Out() << "[TERMINATING] Processing request to stop" << std::endl;

		while (_state == TransactionState::SETTING_UP_REQUEST
			|| _state == TransactionState::PROCESSING_REQUEST)
		{
			Out() << "[TERMINATING] Waiting for request message to be handled..." 
				<< std::endl;
			_signal.WaitForSignal(std::chrono::milliseconds{ 50 });
		}

		if (_state != TransactionState::WAITING_FOR_REQUEST)
		{
			ErrorWithMessage(ErrorCode::UNDEFINED, "Server shut down");
		}

		_state = TransactionState::TERMINATING;
	}

	void ServerWorker::ProcessTransactionState()
	{
		switch (_state)
		{
			case TransactionState::INACTIVE:
			case TransactionState::WAITING_FOR_REQUEST:
				return;

			case TransactionState::SETTING_UP_REQUEST:
			case TransactionState::PROCESSING_REQUEST:
				_signal.WaitForSignal(std::chrono::milliseconds{ 20 });
				return;

			case TransactionState::WAITING_FOR_DATA:
				return ProcessWaitingForDataState();

			case TransactionState::WAITING_FOR_ACK:
				return ProcessWaitingForAckState();

			case TransactionState::TERMINATING:
				return;
		}
	}

	void 
	ServerWorker::ProcessWaitingForDataState()
	{
		using namespace std::chrono;
		using ms = milliseconds;

		auto beginning = steady_clock::now();
		auto now = beginning;

		auto factory = _factory.lock();
		if (!factory)
		{
			Err() << "[WaitingForData] Cannot grab factory. Shutting down?"
				<< std::endl;
			return;
		}

		while (duration_cast<ms>(now - beginning) < _transactionTimeout
			&& _activity == ActivityState::ACTIVE
			&& !_socket->Poll(50) )
		{
			now = steady_clock::now();
			Out() << "[WaitingForData] Elapsed: "
				<< duration_cast<ms>(now - beginning).count() << "ms"
				<< std::endl;
		}

		if (_activity != ActivityState::ACTIVE)
		{
			return;
		}

		auto errorResult = MessageErrorCategory::TIMEOUT;
		if (_socket->HasDatagram())
		{
			auto datagram = _socket->Receive(*factory);

			if (!datagram)
			{
				Err() << "[WaitingForData] could not allocate memory for message"
					<< std::endl;
			}
			errorResult = ProcessDataMessage(datagram);
		}

		if (errorResult != MessageErrorCategory::NO_ERROR)
		{
			Abort(errorResult);
		}
	}

	void 
	ServerWorker::ProcessWaitingForAckState()
	{
		std::shared_ptr<Datagram> datagram = 
			MakeMessageDatagram<MessageData>(_factory, _lastAck + 1, _dataBlockSize);

		MessageData* message = (MessageData*)datagram->GetData();
		size_t read = _fr->ReadBlock((uint8_t*)message->getDataBuffer(),
			_dataBlockSize);
		datagram->SetDataSize((uint16_t)read + MessageData::HeaderSize() );

		size_t attempt = 0;
		while( _activity == ActivityState::ACTIVE ) 
		{
			SendMessage(datagram);
			
			MessageErrorCategory result = WaitForAck(_lastAck + 1);
			switch (result)
			{
				case MessageErrorCategory::NO_ERROR:
					if (read < _dataBlockSize)
					{
						TerminateTransaction();
					}
					return;

				case MessageErrorCategory::TIMEOUT:
					if( attempt++ <= _retries)
					{
						// Try again. Otherwise, fall back to error out.
						continue;
					}

				default:
					Abort(result);
					return;
			}
		}
	}

	ServerWorker::MessageErrorCategory
	ServerWorker::WaitForAck(uint16_t ack)
	{
		using namespace std::chrono;
		using ms = milliseconds;

		auto beginning = steady_clock::now();
		auto now = beginning;

		auto factory = _factory.lock();
		if (!factory)
		{
			Err() << "[WaitForAck] Cannot grab factory. Shutting down?"
				<< std::endl;
			return ServerWorker::MessageErrorCategory::CRITICAL_SERVER_ERROR;
		}

		while (duration_cast<ms>(now - beginning) < _transactionTimeout
			&& _activity == ActivityState::ACTIVE
			&& !_socket->Poll(50))
		{
			now = steady_clock::now();
			Out() << "[WaitForAck] Elapsed: "
				<< duration_cast<ms>(now - beginning).count() << "ms"
				<< std::endl;
		}

		if (_activity != ActivityState::ACTIVE)
		{
			return ServerWorker::MessageErrorCategory::SHUTTING_DOWN;
		}

		auto errorResult = MessageErrorCategory::TIMEOUT;
		if (_socket->HasDatagram())
		{
			auto datagram = _socket->Receive(*factory);

			if (!datagram)
			{
				Err() << "[WaitForAck] could not allocate memory for message"
					<< std::endl;
			}

			errorResult = ProcessAckMessage(ack, datagram);
		}

		return errorResult;
	}

	ServerWorker::MessageErrorCategory
	ServerWorker::ProcessAckMessage( uint16_t expectedAck,
		const std::shared_ptr<Datagram>& dataMessage)
	{
		if (dataMessage->GetDataSize() < sizeof(OpCode))
		{
			return ServerWorker::MessageErrorCategory::INVALID_MESSAGE_FORMAT;
		}

		OpCode *opcode = (OpCode*)dataMessage->GetData();
		switch (*opcode)
		{
			case OpCode::ACK:
				// Happy path.
				break;

			case OpCode::ERROR:
				Err() << "[ProcessAckMessage] Rcv client error" << std::endl;
				return ProcessErrorMessage(dataMessage);

			default:
				return ServerWorker::MessageErrorCategory::INVALID_OPCODE;
		}

		if (dataMessage->GetDataSize() < sizeof(MessageAck))
		{
			return ServerWorker::MessageErrorCategory::INVALID_MESSAGE_FORMAT;
		}

		MessageAck* msg = (MessageAck*)dataMessage->GetData();
		if (msg->getBlockNumber() != expectedAck)
		{
			// Weird case - received ack late?
			// Let's just return timeout and trigger a resend. 
			// Maybe things will work out fine.
			return ServerWorker::MessageErrorCategory::TIMEOUT;
		}

		_lastAck = expectedAck;
		return ServerWorker::MessageErrorCategory::NO_ERROR;
	}

	ServerWorker::MessageErrorCategory
	ServerWorker::ProcessErrorMessage(const std::shared_ptr<Datagram>& errMessage)
	{
			
		return ServerWorker::MessageErrorCategory::CLIENT_ERROR;
	}

	void 
	ServerWorker::ProcessRequestMessage(
		std::shared_ptr<Datagram>& transactionRequest)
	{
		MessageErrorCategory error = MessageErrorCategory::NO_ERROR;

		/* **************************************************************
		 *  Initial message and state validation
		 *  *************************************************************/
		
		// Validate state
		if( error == MessageErrorCategory::NO_ERROR 
			&& _state != TransactionState::PROCESSING_REQUEST)
		{
			Err() << "ERROR! Invalid state for function " __FUNCTION__ << std::endl;
			error = MessageErrorCategory::INVALID_STATE;
		}

		// Validate message size
		if (error == MessageErrorCategory::NO_ERROR 
			&& transactionRequest->GetDataSize() < (sizeof(OpCode) + 2))
		{
			Err() << "Invalid message size for request." << std::endl;
			error = MessageErrorCategory::INVALID_MESSAGE_SIZE;
		}

		// Validate operation is actually a request
		const char* data = transactionRequest->GetData();
		OpCode requestType = *((OpCode*)data);

		if (error == MessageErrorCategory::NO_ERROR
			&& requestType != OpCode::RRQ
			&& requestType != OpCode::WRQ)
		{
			Err() << "Invalid message opcode. Expecting: RRQ/WRQ "
				<< "received: " << OpCodeToStr(requestType) << std::endl;
			error = MessageErrorCategory::INVALID_OPCODE;
		}

		/* **************************************************************
		 *  Validate and handle request
		 *  *************************************************************/
		const MessageRequest* rwrq = ((MessageRequest*)data);
		if (error == MessageErrorCategory::NO_ERROR
			&& !rwrq->Validate(transactionRequest->GetDataSize()))
		{
			error = MessageErrorCategory::INVALID_MESSAGE_FORMAT;
		}

		if (error == MessageErrorCategory::NO_ERROR)
		{
			error = ProcessRequestMessage(rwrq);
		}

		/* **************************************************************
		 *  Reject request in case of error.
		 *  *************************************************************/
		if (error != MessageErrorCategory::NO_ERROR) 
		{
			RejectTransactionRequest(error);
		}
	}

	ServerWorker::MessageErrorCategory
	ServerWorker::ProcessRequestMessage(const MessageRequest* rwrq)
	{
		MessageErrorCategory error = MessageErrorCategory::NO_ERROR;

		/* **************************************************************
		 *  File setup and validation
		 *  *************************************************************/
		_filePath = 
			_parent.FileSecurity().AbsoluteFromServerRoot(rwrq->getFilename());

		auto fileValidation = rwrq->getMessageCode() == OpCode::RRQ 
			? _parent.FileSecurity().IsFileValidForRead(_filePath)
			: _parent.FileSecurity().IsFileValidForWrite(_filePath);

		error = FileSecurityErrorToMessageError(fileValidation);
		if (error != MessageErrorCategory::NO_ERROR)
		{
			return error;
		}

		/* **************************************************************
		 *  Mode Setup and validation
		 *  *************************************************************/
		if (rwrq->getMode() == mode::Mode::MAIL)
		{
			return MessageErrorCategory::INVALID_MODE;
		}

		_currentOperation = rwrq->getMessageCode();
		_asciiMode = rwrq->getMode() == mode::Mode::NETASCII;

		/* **************************************************************
		 *  Lock file 
		 *  *************************************************************/
		_fileLocked = _currentOperation == OpCode::RRQ
			? _parent.FileSecurity().LockFileForRead(_filePath)
			: _parent.FileSecurity().LockFileForWrite(_filePath);

		if (!_fileLocked)
		{
			return MessageErrorCategory::FILE_LOCKED;
		}

		 /* **************************************************************
		  *  Everything went well - update state and ack.
		  *  *************************************************************/

		if (_currentOperation == OpCode::WRQ) {
			auto eolMode = _asciiMode
				? FileWriter::ForceNativeEOL::YES
				: FileWriter::ForceNativeEOL::NO;

			_fw.reset(new FileWriter(_filePath, _fileBuffer.get(), eolMode));
			_fr.reset(nullptr);
			_state = TransactionState::WAITING_FOR_DATA;
			if (!Ack(0))
			{
				MessageErrorCategory::CRITICAL_SERVER_ERROR;
			}
		}
		else 
		{
			auto eolMode = _asciiMode
				? FileReader::ForceNativeEOL::YES
				: FileReader::ForceNativeEOL::NO;

			_fw.reset(nullptr);
			_fr.reset(new FileReader( _filePath, _fileBuffer.get(), eolMode ));
			_state = TransactionState::WAITING_FOR_ACK;
		}


		_signal.EmitSignal();

		return MessageErrorCategory::NO_ERROR;
	}

	void 
	ServerWorker::RejectTransactionRequest(MessageErrorCategory errorReason)
	{
		Abort(errorReason);
	}

	ServerWorker::MessageErrorCategory
	ServerWorker::ProcessDataMessage(
			const std::shared_ptr<Datagram>& datagram)
	{
		/* ***************************************************
		 *  Validation of message and opcode
		 * ***************************************************/

		// msg size
		if (datagram->GetDataSize() < sizeof(MessageData))
		{
			Err() << "[ProcessDataMessage] INVALID_MESSAGE_SIZE" << std::endl;
			return MessageErrorCategory::INVALID_MESSAGE_SIZE;
		}

		// opcode
		OpCode* opCode = (OpCode*)datagram->GetData();
		if ( *opCode == OpCode::ERROR) {
			Err() << "[ProcessDataMessage] CLIENT_ERROR" << std::endl;
			return MessageErrorCategory::CLIENT_ERROR;
		}

		MessageData *msg = (MessageData * )datagram->GetData();
		uint16_t dataSize = datagram->GetDataSize() - msg->HeaderSize();
		bool isLastMessage = dataSize != _dataBlockSize;
		uint16_t expectedBlock = (_lastAck == 0xFFFF) ? 0 : (_lastAck + 1);
		
		Out() << "[ProcessDataMessage] Block=" << msg->getBlockNumber()
			<< ", expectedBlock=" << expectedBlock
			<< ", msgsize=" << datagram->GetDataSize()
			<< ", blocksize=" << dataSize
			<< std::endl;
		
		// block number
		if (msg->getBlockNumber() != expectedBlock)
		{
			return MessageErrorCategory::INVALID_BLOCK;
		}

		/* ***************************************************
		 *  Process the message
		 * ***************************************************/

		_fw->WriteBlock( (uint8_t*)msg->getData(), dataSize);
		Ack(msg->getBlockNumber());

		if (isLastMessage)
		{
			_fw->Finalize();
			TerminateTransaction();
		}

		return MessageErrorCategory::NO_ERROR;
	}

	bool
	ServerWorker::Ack(uint16_t ack)
	{
		Out() << "[Ack] block=" << ack << std::endl;

		std::shared_ptr<Datagram> datagram = 
			MakeMessageDatagram<MessageAck>(_factory, ack);

		bool result = SendMessage(datagram);
		if (result)
		{
			_lastAck = ack;
		}

		return result;
	}

	bool 
	ServerWorker::Error(ErrorCode errorCode)
	{
		std::shared_ptr<Datagram> datagram =
			MakeMessageDatagram<MessageError>(_factory, errorCode);

		return SendMessage(datagram);
	}

	bool 
	ServerWorker::Error(MessageErrorCategory errorCode)
	{
		const char* msg = nullptr;
		ErrorCode err = ErrorCode::UNDEFINED;

		switch (errorCode)
		{
			case MessageErrorCategory::NO_ERROR:
				return false;
			
			case MessageErrorCategory::INVALID_STATE:
			case MessageErrorCategory::INVALID_OPCODE:
				err = ErrorCode::ILLEGAL_OPERATION;
				break;

			case MessageErrorCategory::TIMEOUT:
				msg = "transaction timed out";
				break;

			case MessageErrorCategory::INVALID_MESSAGE_SIZE:
			case MessageErrorCategory::INVALID_MESSAGE_FORMAT:
			case MessageErrorCategory::INVALID_MODE:
				err = ErrorCode::ILLEGAL_OPERATION;
				break;

			case MessageErrorCategory::NO_SUCH_FILE:
				err = ErrorCode::FILE_NOT_FOUND;
				break;

			case MessageErrorCategory::ACCESS_FORBIDDEN:
				err = ErrorCode::ACCESS_VIOLATION;
				break;
			case MessageErrorCategory::FILE_LOCKED:
				msg = "temporarily unavailable";
				break;

			case MessageErrorCategory::UNSAFE_PATH:
				err = ErrorCode::ACCESS_VIOLATION;
				break;

			case MessageErrorCategory::CRITICAL_SERVER_ERROR:
				msg = "critical server error";
				break;
				
		}

		return ErrorWithMessage(err, msg);
	}


	bool
	ServerWorker::ErrorWithMessage(ErrorCode errorCode, const char* msg)
	{
		std::shared_ptr<Datagram> datagram =
			MakeMessageDatagram<MessageError>(_factory, errorCode, msg);

		return SendMessage(datagram);
	}

	bool
	ServerWorker::Abort(MessageErrorCategory error, bool sendErrorMsg)
	{
		Out() << "[Abort] error=" 
			<< MessageErrorCategoryToString(error) << std::endl;

		if (error == MessageErrorCategory::NO_ERROR)
		{
			return false;
		}
		
		if (sendErrorMsg)
		{
			Error(error);
		}

		return TerminateTransaction();
	}

	bool 
	ServerWorker::SendMessage(std::shared_ptr<Datagram>& datagram)
	{
		if (!datagram)
		{
			return false;
		}

		if (!_socket || !_socket->IsBound())
		{
			return false;
		}

		return _socket->Send(datagram);
	}

	bool 
	ServerWorker::TerminateTransaction()
	{
		Out() << "[TerminateTransaction]" << std::endl;

		_fw = nullptr;
		_fr = nullptr;
		_socket = nullptr;

		if (_fileLocked && _currentOperation != OpCode::UNDEF)
		{
			(_currentOperation == OpCode::RRQ)
				? _parent.FileSecurity().UnlockFileForRead(_filePath)
				: _parent.FileSecurity().UnlockFileForWrite(_filePath);
		}

		bool result = _parent.TerminateTransaction(_clientTid, _serverTid);
		_state = TransactionState::WAITING_FOR_REQUEST;
		
		return result;
	}

	ServerWorker::MessageErrorCategory
	ServerWorker::FileSecurityErrorToMessageError(
			FileSecurityHandler::ValidationResult fse) const
	{
		using FSEVR = FileSecurityHandler::ValidationResult;
		using MEC = MessageErrorCategory;
		switch (fse)
		{
			case FSEVR::VALID: return MEC::NO_ERROR;
			
			case FSEVR::INVALID_FORMAT: return MEC::NO_SUCH_FILE;
			case FSEVR::INVALID_ESCAPE_ROOT: return MEC::UNSAFE_PATH;
			case FSEVR::INVALID_CANT_CREATE_FILE: return MEC::ACCESS_FORBIDDEN;
			case FSEVR::INVALID_NO_SUCH_FILE: return MEC::NO_SUCH_FILE;
			case FSEVR::INVALID_IS_DIRECTORY:return MEC::NO_SUCH_FILE;
			case FSEVR::INVALID_ACCESS_FORBIDDEN: return MEC::ACCESS_FORBIDDEN;
			case FSEVR::INVALID_PERMISSIONS: return MEC::ACCESS_FORBIDDEN;

			default: return MEC::ACCESS_FORBIDDEN;
		}
	}

	const char* 
	ServerWorker::MessageErrorCategoryToString(MessageErrorCategory mec) const
	{
		switch (mec)
		{
			case MessageErrorCategory::NO_ERROR: 
				return "NO_ERROR";
			case MessageErrorCategory::INVALID_STATE: 
				return "INVALID_STATE";
			case MessageErrorCategory::INVALID_OPCODE:
				return "INVALID_OPCODE";
			case MessageErrorCategory::INVALID_BLOCK:
				return "INVALID_BLOCK";
			case MessageErrorCategory::TIMEOUT:
				return "TIMEOUT";
			case MessageErrorCategory::INVALID_MESSAGE_SIZE:
				return "INVALID_MESSAGE_SIZE";
			case MessageErrorCategory::INVALID_MESSAGE_FORMAT:
				return "INVALID_MESSAGE_FORMAT";
			case MessageErrorCategory::INVALID_MODE:
				return "INVALID_MODE";
			case MessageErrorCategory::NO_SUCH_FILE:
				return "NO_SUCH_FILE";
			case MessageErrorCategory::ACCESS_FORBIDDEN:
				return "ACCESS_FORBIDDEN";
			case MessageErrorCategory::FILE_LOCKED:
				return "FILE_LOCKED";
			case MessageErrorCategory::UNSAFE_PATH:
				return "UNSAFE_PATH";
			case MessageErrorCategory::CLIENT_ERROR:
				return "CLIENT_ERROR";
			case MessageErrorCategory::CRITICAL_SERVER_ERROR:
				return "CRITICAL_SERVER_ERROR";
			default:
				return "[UNKNOWN]";
		}
	}
}
