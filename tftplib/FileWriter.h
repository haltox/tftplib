#pragma once

#include <filesystem>
#include <cstdint>

namespace tftplib {

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
	class File;

public:
	FileWriter(std::filesystem::path file, 
		ForceNativeEOL eolOpt = ForceNativeEOL::NO);
	~FileWriter();

	FileWriter(FileWriter&& rhs) = delete;
	FileWriter& operator=(FileWriter&& rhs) = delete;
	FileWriter(const FileWriter& rhs) = delete;
	FileWriter& operator=(const FileWriter& rhs) = delete;

	void WriteBlock(uint8_t *buffer, size_t bufferSize);
	void Finalize();

private:
	

private:
	std::unique_ptr<Os> _os;

	std::filesystem::path _pathTempFile {};
	std::filesystem::path _pathEndFile;
	ForceNativeEOL _eolOpt;

	std::unique_ptr<File> _file;
};

}