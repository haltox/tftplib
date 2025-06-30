#include "pch.h"
#include "FileWriter.h"
#include "File.h"
#include <Windows.h>
#include "HaloBuffer.h"

namespace tftplib {

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
	 * OS Specific functions definition
	 * *********************************************************************/
	std::filesystem::path
	FileWriter::Os::MakeTempFileName() const
	{
		GetTempPath2W(MAX_PATH + 1, _bufferDir);
		GetTempFileNameW(_bufferDir, L"grm", 9, _bufferFileName);
		
		return _bufferFileName;
	}

	std::unique_ptr<File>
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

		return std::unique_ptr<File>(new File{h});
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
	FileWriter::FileWriter(std::filesystem::path file, 
		HaloBuffer* buffer, 
		ForceNativeEOL opt)
		: _os { new FileWriter::Os }
		, _pathTempFile { _os->MakeTempFileName() }
		, _pathEndFile{file}
		, _eolOpt { opt }
		, _buffer{ buffer }
		, _file { _os->OpenFile(_pathTempFile)}
	{
		_file->DeleteOnClose();
	}

	FileWriter::~FileWriter()
	{
	}

	void 
	FileWriter::WriteBlock(const uint8_t* buffer, size_t bufferSize)
	{
		if (_eolOpt == ForceNativeEOL::YES) 
		{
			BufferBlock(buffer, bufferSize);
			BufferOut(_bytesWritten == 0 ? (bufferSize >> 1) : bufferSize );
		}
		else 
		{
			Write(buffer, bufferSize);
		}
	}

	void 
	FileWriter::Finalize()
	{
		if (_bytesBuffered > _bytesWritten)
		{
			BufferOut(_bytesBuffered - _bytesWritten);
		}

		_file->Commit();
		_os->OverwriteFile(_pathEndFile, _pathTempFile);
	}

	void 
	FileWriter::BufferBlock(const uint8_t* buffer, size_t bufferSize)
	{
		uint8_t*  target = 
			_buffer->GetAt<uint8_t>(_bytesBuffered % _buffer->Size());
		memcpy(target, buffer, bufferSize);
		_bytesBuffered += bufferSize;
	}

	void
	FileWriter::BufferOut(size_t bufferSize)
	{
		const uint8_t* data = _buffer->Get<uint8_t>();
		const uint8_t* EOL = (const uint8_t*)"\r\n";

		size_t cursor = _bytesWritten % _buffer->Size();
		size_t beginning = cursor;
		size_t processed = 0;

		while (processed++ < bufferSize)
		{
			// CRLF
			if (data[cursor] == '\r'
				&& processed < _bytesBuffered
				&& data[cursor + 1] == '\n')
			{
				WriteBufferedSegment(beginning, cursor);
				Write(EOL, 2);

				processed += 1, cursor += 2, beginning = cursor;
			}
			// UNIX line ending
			else if (data[cursor] == '\n')
			{
				WriteBufferedSegment(beginning, cursor);
				Write(EOL, 2);
				
				_bytesWritten -= 1, cursor += 1, beginning = cursor;
			}
			else
			{
				cursor += 1;
			}
		}
		
		WriteBufferedSegment(beginning, cursor);
	}

	void
	FileWriter::Write(const uint8_t* buffer, size_t bufferSize)
	{
		_file->Write(buffer, bufferSize);
		_bytesWritten += bufferSize;
	}
	
	void
	FileWriter::WriteBufferedSegment(size_t beginning, size_t end)
	{
		size_t segmentSize = end - beginning;
		if (segmentSize != 0)
		{
			Write(_buffer->GetAt<uint8_t>(beginning), segmentSize);
		}
	}
}