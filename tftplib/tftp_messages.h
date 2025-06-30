#pragma once

#include <cstdint>
#include <bit>
#include <functional>

namespace tftplib
{
	constexpr bool IS_BIG_ENDIAN =
		(std::endian::native == std::endian::big);

	namespace defaults {
		const uint16_t ServerPort = 69;	// Default TFTP port
		const uint16_t BlockSize = 512;
	};

	enum class OpCode : uint16_t {
		RRQ		= IS_BIG_ENDIAN ? 0x0001 : 0x0100,	// Read Request
		WRQ		= IS_BIG_ENDIAN ? 0x0002 : 0x0200,	// Write Request
		DATA	= IS_BIG_ENDIAN ? 0x0003 : 0x0300,	// Data
		ACK		= IS_BIG_ENDIAN ? 0x0004 : 0x0400,	// Acknowledgment
		ERROR	= IS_BIG_ENDIAN ? 0x0005 : 0x0500,	// Error
		OACK	= IS_BIG_ENDIAN ? 0x0006 : 0x0600,	// Option Acknowledgment


		UNDEF	= 0xFFFF
	};

	const char* OpCodeToStr(OpCode op);

	namespace mode {
		static const char* NETASCII = "netascii";	// Text mode
		static const char* OCTET = "octet";			// Binary mode
		static const char* MAIL = "mail";			// Mail mode (obsolete)

		static const char* ModeStrings[] = {
			nullptr,								// 0
			NETASCII,								// 1
			OCTET,									// 2
			MAIL									// 3
		};

		enum class Mode : uint8_t
		{
			UNDEFINED = 0x00,						// Not defined
	
			NETASCII = 0x01,						// Text mode
			OCTET = 0x02,							// Binary mode
			MAIL = 0x03,							// Mail mode (obsolete)

			LAST = MAIL								// Last valid mode
		};

		Mode StrToEnum(const char* str);
	};

	enum class ErrorCode : uint16_t {
		UNDEFINED			= IS_BIG_ENDIAN ? 0x0000 : 0x0000,	// Not defined, see error message (RFC 1350)
		FILE_NOT_FOUND		= IS_BIG_ENDIAN ? 0x0001 : 0x0100,	// File not found (RFC 1350)
		ACCESS_VIOLATION	= IS_BIG_ENDIAN ? 0x0002 : 0x0200,	// Access violation (RFC 1350)
		DISK_FULL			= IS_BIG_ENDIAN ? 0x0003 : 0x0300,	// Disk full or allocation exceeded (RFC 1350)
		ILLEGAL_OPERATION	= IS_BIG_ENDIAN ? 0x0004 : 0x0400,	// Illegal TFTP operation (RFC 1350)
		UNKNOWN_TRANSFER_ID	= IS_BIG_ENDIAN ? 0x0005 : 0x0500,	// Unknown transfer ID (RFC 1350)
		FILE_EXISTS			= IS_BIG_ENDIAN ? 0x0006 : 0x0600,	// File already exists (RFC 1350)
		NO_SUCH_USER		= IS_BIG_ENDIAN ? 0x0007 : 0x0700,	// No such user. (RFC 1350)

		LAST = NO_SUCH_USER
	};

	static const char* DefaultErrorMessages[] = {
		nullptr,								// UNDEFINED
		"File not found",						// FILE_NOT_FOUND
		"Access violation",						// ACCESS_VIOLATION
		"Disk full or allocation exceeded",		// DISK_FULL
		"Illegal TFTP operation",				// ILLEGAL_OPERATION
		"Unknown transfer ID",					// UNKNOWN_TRANSFER_ID
		"File already exists",					// FILE_EXISTS
		"No such user"							// NO_SUCH_USER
	};

#pragma pack (push, 1)

	class MessageHeader {
		OpCode opcode;

	public:
		OpCode getMessageCode() const {
			return opcode;
		}
	};

	class MessageError {
		OpCode opcode;
		ErrorCode errorCode;
		char errorMessage[1];

	public:
		static MessageError* create(ErrorCode errorCode, 
			std::function<void*(size_t)> allocator,
			const char* customErrorMessage = nullptr);

		static MessageError* create(ErrorCode errorCode,
			const char* customErrorMessage,
			std::function<void* (size_t)> allocator );

		OpCode getMessageCode() const {
			return opcode;
		}

		ErrorCode getErrorCode() const {
			return errorCode;
		}

		const char* getErrorMessage() const {
			return errorMessage;
		}

		size_t Size() const {
			return sizeof(MessageError) + strlen(errorMessage);
		}
	};

	class MessageRequest {
		OpCode opcode;
		char filenameAndMode[2];

	private:
		static MessageRequest* createRequest(OpCode opCode,
			const char* filename,
			mode::Mode mode,
			std::function<void* (size_t)> allocator);
	public:
		static MessageRequest* createReadRequest( const char* filename,
			mode::Mode mode,
			std::function<void* (size_t)> allocator);

		static MessageRequest* createWriteRequest(const char* filename,
			mode::Mode mode,
			std::function<void* (size_t)> allocator);

		OpCode getMessageCode() const {
			return opcode;
		}
		
		const char* getFilename() const {
			return filenameAndMode;
		}

		const char* getModeStr() const {
			return filenameAndMode + strlen(filenameAndMode) + 1;
		}

		mode::Mode getMode() const;

		// 'Safe' variant of the accessors.
		// Those functions make sure they do not read past the end of the buffer.
		// Those functions and those functions only should be called on 
		// incoming requests that haven't been validated.
		const char* getFilenameS(uint16_t messageSz) const;
		const char* getModeStrS(uint16_t messageSz) const;

		mode::Mode getModeS(uint16_t messageSz) const;

		// Return the buffer size required to acommodate this request.
		size_t Size() const;

		// Validate the request.
		//	Possible failure scenarios : 
		//		- Malformed request
		//			i.e. filename and/or mode aren't properly formatted within 
		//			buffer
		//		- Mode is undefined, unknown or could not be parsed.
		//		- Mode is MAIl. RFC1350 tells us not to support this mode of operation.
		bool Validate(uint16_t messageSz) const;
	};

	class MessageData {
		OpCode opcode;
		uint16_t blockNumber;
		char data[1];

		friend class MessageAck;

	public:
		static MessageData* create(uint16_t number,
			uint16_t blockSize,
			std::function<void* (size_t)> allocator);

		uint16_t getBlockNumber() const ;

		const char* getData() const {
			return data;
		}

		char* getDataBuffer() {
			return data;
		}

		static uint16_t HeaderSize() { 
			return sizeof(OpCode) + sizeof(uint16_t); 
		}
	};

	class MessageAck {
		OpCode opcode;
		uint16_t blockNumber;

	public:
		static MessageAck* create(uint16_t number,
			std::function<void* (size_t)> allocator);

		static MessageAck* createFor(MessageData* og,
			std::function<void* (size_t)> allocator);

		uint16_t getBlockNumber() const;

		size_t Size() const {
			return sizeof(MessageAck);
		}
	};

#pragma pack (pop)

}