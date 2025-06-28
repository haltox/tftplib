#include "pch.h"
#include "FileWriter.h"
#include <Windows.h>

namespace tftplib {

	/* *********************************************************************
	 * File class declaration
	 * *********************************************************************/
	class FileWriter::File
	{
	public:
		File() {}
		File(HANDLE h);
		~File();
		File(File&& h) noexcept;
		File& operator=(File&& h) noexcept;

		File(const File& h) = delete;
		File& operator=(const File& h) = delete;

		void DeleteOnClose();
		void Commit();

		void Write(uint8_t *buffer, size_t sz);

	private:
		HANDLE _handle{ 0 };
	};

	/* *********************************************************************
	 * OS Specific class declaration
	 * *********************************************************************/
	class FileWriter::Os
	{

	public:
		std::filesystem::path MakeTempFileName() const;
		std::unique_ptr<File> OpenFile(const std::filesystem::path &path);
		void OverwriteFile(const std::filesystem::path& replaced, 
			const std::filesystem::path& replacement);

	private:
		
		mutable TCHAR _bufferDir[MAX_PATH + 1];
		mutable TCHAR _bufferFileName[MAX_PATH + 1];
	};

	/* *********************************************************************
	 * File class functions definition
	 * *********************************************************************/
	FileWriter::File::File(HANDLE h)
		: _handle{ h }
	{}

	FileWriter::File::~File()
	{
		CloseHandle(_handle);
	}

	FileWriter::File::File(File&& h) noexcept
	{
		*this = std::move(h);
	}

	FileWriter::File&
	FileWriter::File::operator=(File&& h) noexcept
	{
		std::swap(_handle, h._handle);
		return *this;
	}

	void FileWriter::File::DeleteOnClose()
	{
		FILE_DISPOSITION_INFO info;
		info.DeleteFileW = true;
		SetFileInformationByHandle(_handle, FileDispositionInfo,
			&info, sizeof(info));
	}

	void FileWriter::File::Commit()
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

	void FileWriter::File::Write(uint8_t* buffer, size_t sz)
	{
		WriteFile(_handle, buffer, sz, nullptr, nullptr);
	}

	/* *********************************************************************
	 * OS Specific functions definition
	 * *********************************************************************/
	std::filesystem::path
	FileWriter::Os::MakeTempFileName() const
	{
		GetTempPath2W(MAX_PATH + 1, _bufferDir);
		GetTempFileNameW(_bufferDir, L"grm", 9, _bufferFileName);
		
		return _bufferFileName;
	}

	std::unique_ptr<FileWriter::File>
	FileWriter::Os::OpenFile(const std::filesystem::path& path)
	{
		HANDLE h = CreateFileW(
			path.wstring().c_str(),
			GENERIC_WRITE | DELETE,
			0,	// No sharing
			nullptr, // No security attributes
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			nullptr //  no file template
		);

		return std::unique_ptr<FileWriter::File>(new File{h});
	}

	void 
	FileWriter::Os::OverwriteFile(const std::filesystem::path& replaced,
		const std::filesystem::path& replacement)
	{
		HANDLE hReplaced = CreateFileW(
			replaced.wstring().c_str(),
			GENERIC_WRITE,
			0,	// No sharing
			nullptr, // No security attributes
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			nullptr //  no file template
		);

		CloseHandle(hReplaced);

		ReplaceFileW( replaced.wstring().c_str(), 
			replacement.wstring().c_str(),
			nullptr, 0, 0, 0);
	}

	/* *********************************************************************
	 * FileWriter functions definition
	 * *********************************************************************/
	FileWriter::FileWriter(std::filesystem::path file, ForceNativeEOL opt)
		: _os { new FileWriter::Os }
		, _pathTempFile { _os->MakeTempFileName() }
		, _pathEndFile{file}
		, _eolOpt { opt }
		, _file { _os->OpenFile(_pathTempFile)}
	{
		_file->DeleteOnClose();
	}

	FileWriter::~FileWriter()
	{
	}

	void 
	FileWriter::WriteBlock(uint8_t* buffer, size_t bufferSize)
	{
		// TODO EOL override
		_file->Write(buffer, bufferSize);
	}

	void 
	FileWriter::Finalize()
	{
		_file->Commit();
		_os->OverwriteFile(_pathEndFile, _pathTempFile);
	}
}