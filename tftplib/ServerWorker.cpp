#include "pch.h"
#include "ServerWorker.h"
#include "Server.h"
#include <thread>
#include <string>

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
		: _parent{parent}
		, _factory { factory }
	{
	}

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

			while(_activity == ActivityState::ACTIVE 
				&& _state != TransactionState::TERMINATING)
			{
				TickTransaction();
			}

			// Process messages or transactions here
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		_state = TransactionState::INACTIVE;
		_activity = ActivityState::INACTIVE;
	}

	void ServerWorker::TickTransaction()
	{
		
	}

	void ServerWorker::TryProcessMessage()
	{

		

	}

	void ServerWorker::ProcessTick()
	{
		
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
		
		_asciiMode = rwrq->getMode() == mode::Mode::NETASCII;

		/* **************************************************************
		 *  Lock file 
		 *  *************************************************************/
		auto locked = rwrq->getMessageCode() == OpCode::RRQ
			? _parent.FileSecurity().LockFileForRead(_filePath)
			: _parent.FileSecurity().LockFileForWrite(_filePath);

		if (!locked)
		{
			return MessageErrorCategory::FILE_LOCKED;
		}

		 /* **************************************************************
		  *  Everything went well - update state and ack.
		  *  *************************************************************/

		_state = rwrq->getMessageCode() == OpCode::RRQ ?
			TransactionState::WAITING_FOR_ACK  :
			TransactionState::WAITING_FOR_DATA ;
		if (rwrq->getMessageCode() == OpCode::WRQ) {
			if (!Ack(0))
			{
				MessageErrorCategory::CRITICAL_SERVER_ERROR;
			}
			_state = TransactionState::WAITING_FOR_DATA;
		}

		return MessageErrorCategory::NO_ERROR;
	}

	void 
	ServerWorker::RejectTransactionRequest(MessageErrorCategory errorReason)
	{
		// SUPER IMPORTANT TODO
	}

	bool
	ServerWorker::Ack(uint16_t ack)
	{
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
}
