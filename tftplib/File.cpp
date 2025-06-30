#include "pch.h"
#include "File.h"

#include <Windows.h>
#include <utility>

namespace tftplib {

	const File::OpenForRead_t File::OpenForRead{};
	const File::OpenForWrite_t File::OpenForWrite{};
	const File::OpenForDelete_t File::OpenForDelete{};

	File *OpenFile(const std::filesystem::path& path, DWORD accessFlags,
		DWORD shareFlags, DWORD openFlags )
	{
		HANDLE h = CreateFileW(
			path.wstring().c_str(),
			accessFlags,
			shareFlags,	// No sharing
			nullptr, // No security attributes
			openFlags,
			FILE_ATTRIBUTE_NORMAL,
			nullptr //  no file template
		);

		if( h == 0 ) return nullptr;
		return new File{ h };
	}

	File* File::Open(const std::filesystem::path& path,
		File::OpenForRead_t read)
	{
		return OpenFile(path, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING);
	}

	File* File::Open(const std::filesystem::path& path,
		File::OpenForRead_t read,
		File::OpenForDelete_t del)
	{
		return OpenFile(path, GENERIC_READ, 
			FILE_SHARE_READ | DELETE,
			OPEN_EXISTING);
	}

	File* File::Open(const std::filesystem::path& path,
		File::OpenForWrite_t write)
	{
		return OpenFile(path, GENERIC_WRITE, 0, CREATE_ALWAYS);
	}

	File* File::Open(const std::filesystem::path& path,
		File::OpenForWrite_t write,
		File::OpenForDelete_t del)
	{
		return OpenFile(path, GENERIC_WRITE | DELETE, 0, CREATE_ALWAYS);
	}

	File::File(File::Handle h)
		: _handle{ h }
	{
	}

	File::~File()
	{
		CloseHandle(_handle);
	}

	File::File(File&& h) noexcept
	{
		*this = std::move(h);
	}

	File&
	File::operator=(File&& h) noexcept
	{
		std::swap(_handle, h._handle);
		return *this;
	}

	void File::DeleteOnClose()
	{
		FILE_DISPOSITION_INFO info;
		info.DeleteFileW = true;
		SetFileInformationByHandle(_handle, FileDispositionInfo,
			&info, sizeof(info));
	}

	void File::Commit()
	{
		// Remove delete on close flag.
		FILE_DISPOSITION_INFO info;
		info.DeleteFileW = false;
		SetFileInformationByHandle(_handle, FileDispositionInfo,
			&info, sizeof(info));

		FlushFileBuffers(_handle);
		CloseHandle(_handle);

		_handle = 0;
	}

	void File::Write(const uint8_t* buffer, size_t sz)
	{
		WriteFile(_handle, buffer, sz, nullptr, nullptr);
	}

	size_t File::Read(uint8_t* buffer, size_t bufSz)
	{
		DWORD read{0};
		if( !ReadFile(_handle, buffer, bufSz, &read, nullptr) ){
			return -1;
		}

		return read;
	}
}