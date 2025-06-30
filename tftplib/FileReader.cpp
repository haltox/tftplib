#include "pch.h"
#include "FileReader.h"
#include "File.h"

namespace tftplib {

	class FileReader::Os {

	};

	FileReader::FileReader(std::filesystem::path file,
		HaloBuffer* buffer,
		ForceNativeEOL eolOpt)
		: _os {std::make_unique<FileReader::Os>()}
		, _path {file}
		, _eolOpt { eolOpt }
		, _buffer {buffer}
		, _file { File::Open(_path, File::OpenForRead) }
	{
	}

	FileReader::~FileReader() { }

	size_t FileReader::ReadBlock(uint8_t* buffer, size_t bufferSize)
	{
		return _file->Read(buffer, bufferSize);
	}
}