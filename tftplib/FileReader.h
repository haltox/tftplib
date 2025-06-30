#pragma once
#include <filesystem>

namespace tftplib {

	class HaloBuffer;
	class File;

	class FileReader
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
		FileReader(std::filesystem::path file,
			HaloBuffer* buffer,
			ForceNativeEOL eolOpt = ForceNativeEOL::NO);
		~FileReader();

		FileReader(FileReader&& rhs) = delete;
		FileReader& operator=(FileReader&& rhs) = delete;
		FileReader(const FileReader& rhs) = delete;
		FileReader& operator=(const FileReader& rhs) = delete;

		size_t ReadBlock(uint8_t* buffer, size_t bufferSize);

	private:
		std::unique_ptr<Os> _os;
		std::filesystem::path _path;
		ForceNativeEOL _eolOpt;

		size_t _buffered{0};

		HaloBuffer* _buffer;
		std::unique_ptr<File> _file;
	};

}


