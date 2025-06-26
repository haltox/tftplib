#include "pch.h"

#include "tftp_messages.h"

namespace tftplib {

	int strLenS(const char* str, uint16_t maxSz) {
		int sz = 0;
		while (*str++ && maxSz--) {
			sz++;
		}

		if (*(str - 1)) return -1;
		return sz;
	}

	mode::Mode mode::StrToEnum(const char* str) {
		if (str == nullptr) return mode::Mode::UNDEFINED;

		if (strcmp(str, mode::NETASCII) == 0)
		{
			return mode::Mode::NETASCII;
		}
		if (strcmp(str, mode::OCTET) == 0)
		{
			return mode::Mode::OCTET;
		}
		if (strcmp(str, mode::MAIL) == 0)
		{
			return mode::Mode::MAIL;
		}

		return mode::Mode::UNDEFINED;
	}

	const char* OpCodeToStr(OpCode op) {
		switch (op) {
		case OpCode::RRQ:	return "RRQ";
		case OpCode::WRQ:	return "WRQ";
		case OpCode::DATA:	return "DATA";
		case OpCode::ACK:	return "ACK";
		case OpCode::ERROR:	return "ERROR";
		case OpCode::OACK:	return "OACK";
		default:			return "[unknown]";
		}
	}

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

		size_t errorLen = strlen(errorMessage);
		size_t messageLength = sizeof(MessageError) + errorLen;
		MessageError* message = (MessageError*)allocator(messageLength);
		if (message == nullptr) {
			return nullptr;
		}

		message->opcode = OpCode::ERROR;
		message->errorCode = errorCode;
		strcpy_s(message->errorMessage, errorLen + 1, errorMessage);
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

	mode::Mode MessageRequest::getMode() const {
		return mode::StrToEnum(getModeStr());
	}

	const char* MessageRequest::getFilenameS(uint16_t messageSz) const {
		uint16_t dataSize = messageSz - sizeof(OpCode);
		if (strLenS(filenameAndMode, dataSize) == -1) {
			return nullptr;
		}

		return filenameAndMode;
	}

	const char* MessageRequest::getModeStrS(uint16_t messageSz) const {
		uint16_t dataSize = messageSz - sizeof(OpCode);
		int filenameLen = strLenS(filenameAndMode, dataSize);

		if (filenameLen == -1 || filenameLen + 2 > dataSize) {
			return nullptr;
		}

		dataSize -= (filenameLen + 1);
		const char* modeStr = filenameAndMode + (filenameLen + 1);
		if (strLenS(modeStr, dataSize) == -1) {
			return nullptr;
		}

		return modeStr;
	}

	mode::Mode MessageRequest::getModeS(uint16_t messageSz) const {
		return mode::StrToEnum(getModeStrS(messageSz));
	}

	size_t MessageRequest::Size() const {
		return sizeof(MessageRequest) + strlen(filenameAndMode)
			+ strlen(getModeStr());
	}

	bool MessageRequest::Validate(uint16_t messageSz) const {
		return getFilenameS(messageSz) != nullptr
			&& getModeStrS(messageSz) != nullptr
			&& getMode() != mode::Mode::MAIL
			&& getMode() != mode::Mode::UNDEFINED;
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
