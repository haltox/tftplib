#include "pch.h"
#include "ServerWorker.h"
#include "Server.h"
#include <thread>
#include <string>

namespace tftplib {
	ServerWorker::ServerWorker(Server& parent)
		:_parent{parent}
		, _messages {16}
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
	}

	// Transaction handling
	void ServerWorker::AssignTransaction(uint16_t requestId)
	{
		{
			TransactionState expected{ TransactionState::WAITING_FOR_REQUEST };
			TransactionState desired{ TransactionState::SETTING_UP_REQUEST };
			if (!_state.compare_exchange_strong(expected, desired))
			{
				Err() << "AssignTransaction(" << requestId << ") called while : ";
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
						<< _transactionId 
						<< std::endl;
				}
				return;
			}
		}

		_lastAck = 0;
		_transactionId = requestId;
		
		TransactionState expected { TransactionState::SETTING_UP_REQUEST };
		TransactionState desired{ TransactionState::PROCESSING_REQUEST };
		if (!_state.compare_exchange_strong(expected, desired)) 
		{
			Err() << "Serious threading error in AssignTransaction" << std::endl;
		}

		_signal.EmitSignal();
	}

	bool ServerWorker::IsBusy() const
	{
		return _state != TransactionState::WAITING_FOR_REQUEST;
	}

	bool ServerWorker::DispatchMessage(const std::shared_ptr<Datagram>& message)
	{
		if (_messages.Write(message)) 
		{
			_signal.EmitSignal();
			return true;
		}

		Err() << "Failed to add message to queue for worker " 
			<< _thread.get_id()
			<< std::endl;

		return false;
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

				if (_messages.IsEmpty() )
				{
					_signal.WaitForSignal(std::chrono::milliseconds{ 50 });
				}
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
		if (_messages.IsEmpty())
		{
			return;
		}

		auto message = _messages.Read();

	}

	void ServerWorker::ProcessTick()
	{
		
	}
}