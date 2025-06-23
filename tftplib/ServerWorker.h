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

namespace tftplib {
	class Server;
	
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

			WAITING_FOR_ACK,			// Worker thread is waiting for ACK from 
			// the client

			TERMINATING					// Worker thread is shutting down
										// Will transition to inactive.
		};


	public:
		
		ServerWorker(Server &parent);

		// Thread handling
		void Start();

		void RequestStop();

		void Stop();

		// Transaction handling
		void AssignTransaction(uint16_t clientTid, 
			uint16_t serverTid, 
			std::shared_ptr<UdpSocketWindows> socket );

		bool IsBusy() const;

		bool DispatchMessage(const std::shared_ptr<Datagram> &message);

		std::ostream& Out();
		std::ostream& Err();

	private:
		void Run();

		void TickTransaction();

		void TryProcessMessage();
		void ProcessTick();

	private:
		std::thread _thread {};
		Signal _signal{};

		RingBuffer<std::shared_ptr<Datagram>> _messages;

		// Transaction state handling
		std::atomic<ActivityState> _activity {ActivityState::INACTIVE};
		std::atomic<TransactionState> _state { TransactionState::INACTIVE };
		uint16_t _clientTid {0};
		uint16_t _serverTid{ 0 };
		uint16_t _lastAck {0};

		Server &_parent;
	};
}


