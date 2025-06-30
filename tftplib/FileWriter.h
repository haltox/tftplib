#pragma once

#include <filesystem>
#include <cstdint>

namespace tftplib {

class File;
class HaloBuffer;

class FileWriter
{
public:
	enum class ForceNativeEOL
	{
		NO,
		YES
	};

private:
	class Os;

public:
	FileWriter(std::filesystem::path file,
		HaloBuffer* buffer,
		ForceNativeEOL eolOpt = ForceNativeEOL::NO);
	~FileWriter();

	FileWriter(FileWriter&& rhs) = delete;
	FileWriter& operator=(FileWriter&& rhs) = delete;
	FileWriter(const FileWriter& rhs) = delete;
	FileWriter& operator=(const FileWriter& rhs) = delete;

	void WriteBlock(const uint8_t *buffer, size_t bufferSize);
	void Finalize();

private:
	void BufferBlock(const uint8_t* buffer, size_t bufferSize);
	void BufferOut(size_t bufferSize);
	void Write(const uint8_t* buffer, size_t bufferSize);

	void WriteBufferedSegment(size_t beginning, size_t end);

private:
	std::unique_ptr<Os> _os;

	std::filesystem::path _pathTempFile {};
	std::filesystem::path _pathEndFile;
	ForceNativeEOL _eolOpt;

	// Buffer state
	size_t _bytesBuffered{0};
	size_t _bytesWritten{0};

	HaloBuffer* _buffer;
	std::unique_ptr<File> _file;
};

}