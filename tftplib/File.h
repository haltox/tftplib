#pragma once

#include <cstdint>
#include <filesystem>

namespace tftplib
{
	class File
	{
	public:
		using Handle = void*;

		struct OpenForRead_t {};
		static const OpenForRead_t OpenForRead;

		struct OpenForWrite_t {};
		static const OpenForWrite_t OpenForWrite;

		struct OpenForDelete_t {};
		static const OpenForDelete_t OpenForDelete;

	public:
		static File* Open(const std::filesystem::path& path, OpenForRead_t read);
		static File* Open(const std::filesystem::path& path, 
			OpenForRead_t read, 
			OpenForDelete_t del);
		static File* Open(const std::filesystem::path& path, OpenForWrite_t write);
		static File* Open(const std::filesystem::path& path, 
			OpenForWrite_t write,
			OpenForDelete_t del);

		File() {}
		File(Handle h);
		~File();
		File(File&& h) noexcept;
		File& operator=(File&& h) noexcept;

		File(const File& h) = delete;
		File& operator=(const File& h) = delete;

		void DeleteOnClose();
		void Commit();

		void Write(const uint8_t* buffer, size_t sz);
		size_t Read(uint8_t*  buffer, size_t bufSz);

	private:
		Handle _handle{ 0 };
	};
}