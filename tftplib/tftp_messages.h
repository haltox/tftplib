#pragma once

#include <cstdint>

namespace tftplib
{
	namespace defaults {
		const uint16_t ServerPort = 69;	// Default TFTP port
	};

	enum class OpCode : uint16_t {
		RRQ		= 1,	// Read Request
		WRQ		= 2,	// Write Request
		DATA	= 3,	// Data
		ACK		= 4,	// Acknowledgment
		ERROR	= 5,	// Error
		OACK	= 6		// Option Acknowledgment
	};

	namespace mode {
		static const char* NETASCII = "netascii";	// Text mode
		static const char* OCTET = "octet";		// Binary mode
		static const char* MAIL = "mail";		// Mail mode			// Obsolete

		static const uint8_t cNETASCII = 0x01;			// Text mode
		static const uint8_t cOCTET = 0x02;			// Binary mode
		static const uint8_t cMAIL = 0x03;			// Mail mode			// Obsolete
	};

	enum class ErrorCode : uint16_t {
		UNDEFINED			= 0,	// Not defined, see error message (RFC 1350)
		FILE_NOT_FOUND		= 1,	// File not found (RFC 1350)
		ACCESS_VIOLATION	= 2,	// Access violation (RFC 1350)
		DISK_FULL			= 3,	// Disk full or allocation exceeded (RFC 1350)
		ILLEGAL_OPERATION	= 4,	// Illegal TFTP operation (RFC 1350)
		UNKNOWN_TRANSFER_ID	= 5,	// Unknown transfer ID (RFC 1350)
		FILE_EXISTS			= 6,	// File already exists (RFC 1350)
		NO_SUCH_USER		= 7		// No such user. (RFC 1350)
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

};