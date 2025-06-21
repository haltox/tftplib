#include "pch.h"

#include "tftp_messages.h"

namespace tftplib {

	MessageError* MessageError::create(ErrorCode errorCode,
		std::function<void* (size_t)> allocator,
		const char* customErrorMessage)
	{
		if (errorCode > ErrorCode::LAST) {
			return nullptr;
		}

		const char* errorMessage = customErrorMessage;
		if (errorMessage == nullptr) {
			uint8_t* bytes = (uint8_t*)&errorCode;
			uint8_t index = bytes[1];
			errorMessage = DefaultErrorMessages[index];
		}

		if (errorMessage == nullptr) {
			errorMessage = "";
		}

		size_t messageLength = sizeof(MessageError) + strlen(errorMessage);
		MessageError* message = (MessageError*)allocator(messageLength);
		if (message == nullptr) {
			return nullptr;
		}

		message->opcode = OpCode::ERROR;
		message->errorCode = errorCode;
		strcpy_s(message->errorMessage, errorMessage);
		return message;
	}

	MessageRequest* MessageRequest::createRequest(OpCode opCode,
		const char* filename,
		mode::Mode mode,
		std::function<void* (size_t)> allocator)
	{
		if (mode == mode::Mode::UNDEFINED || mode > mode::Mode::LAST) {
			return nullptr;
		}

		size_t filenameLength = strlen(filename);
		size_t modeLength = strlen(mode::NETASCII);
		size_t messageLength = sizeof(MessageRequest)
			+ filenameLength + 1
			+ modeLength + 1;

		MessageRequest* message = (MessageRequest*)allocator(messageLength);
		if( message == nullptr) {
			return nullptr;
		}

		message->opcode = opCode;
		strcpy_s(message->filenameAndMode, filenameLength + 1, filename);

		char* modePtr = message->filenameAndMode + filenameLength + 1;
		const char* modeStr = mode::ModeStrings[static_cast<uint8_t>(mode)];
		strcpy_s(modePtr, modeLength + 1, modeStr);
		return message;
	}

	MessageRequest* MessageRequest::createReadRequest(const char* filename,
		mode::Mode mode,
		std::function<void* (size_t)> allocator)
	{
		return createRequest(OpCode::RRQ, filename, mode, allocator);
	}

	MessageRequest* MessageRequest::createWriteRequest(const char* filename,
		mode::Mode mode,
		std::function<void* (size_t)> allocator)
	{
		return createRequest(OpCode::WRQ, filename, mode, allocator);
	}

	MessageData* MessageData::create(uint16_t number,
		uint16_t blockSize,
		std::function<void* (size_t)> allocator)
	{
		size_t messageLength = sizeof(MessageData) - 1 + blockSize;
		MessageData* message = (MessageData*)allocator(messageLength);
		if (message == nullptr) {
			return nullptr;
		}
		message->opcode = OpCode::DATA;

		uint8_t* bytes = (uint8_t*)&message->blockNumber;
		bytes[1] = (number >> 0) & 0xFF;
		bytes[0] = (number >> 8) & 0xFF;
		return message;
	}

	uint16_t MessageData::getBlockNumber() const
	{
		uint8_t* bytes = (uint8_t*)&blockNumber;
		return (bytes[1] << 0) | (bytes[0] << 8);
	}

	MessageAck* MessageAck::create(uint16_t number,
		std::function<void* (size_t)> allocator)
	{
		size_t messageLength = sizeof(MessageAck);
		MessageAck* message = (MessageAck*)allocator(messageLength);
		if (message == nullptr) {
			return nullptr;
		}

		message->opcode = OpCode::ACK;
		uint8_t* bytes = (uint8_t*)&message->blockNumber;
		bytes[1] = (number >> 0) & 0xFF;
		bytes[0] = (number >> 8) & 0xFF;

		return message;
	}

	MessageAck* MessageAck::createFor(MessageData* og,
		std::function<void* (size_t)> allocator)
	{
		size_t messageLength = sizeof(MessageAck);
		MessageAck* message = (MessageAck*)allocator(messageLength);
		if (message == nullptr) {
			return nullptr;
		}

		message->opcode = OpCode::ACK;
		message->blockNumber = og->blockNumber;

		return message;
	}

	uint16_t MessageAck::getBlockNumber() const
	{
		uint8_t* bytes = (uint8_t*)&blockNumber;
		return (bytes[1] << 0) | (bytes[0] << 8);
	}
}
