#include "pch.h"
#include "FileSecurityHandler.h"

namespace tftplib {

	/* ***************************************************
	 *	Setup API
	 * ***************************************************/
	FileSecurityHandler& 
	FileSecurityHandler::SetRootDirectory(std::filesystem::path root)
	{
		_rootDirectory = root;
		return *this;
	}

	FileSecurityHandler& 
	FileSecurityHandler::SetOverwritePolicy(OverwritePolicy policy)
	{
		_canOverwriteFile = policy == OverwritePolicy::ALLOW;
		return *this;
	}

	FileSecurityHandler& 
	FileSecurityHandler::SetFileCreationPolicy(FileCreationPolicy policy)
	{
		_canCreateFile = policy == FileCreationPolicy::ALLOW;
		return *this;
	}

	FileSecurityHandler& FileSecurityHandler::Reset()
	{
		_canCreateFile = false;
		_canOverwriteFile = false;
		_rootDirectory = "";
		_locks.clear();

		return *this;
	}

	/* ***************************************************
	 *	Security handling API
	 * ***************************************************/

	std::filesystem::path
	FileSecurityHandler::AbsoluteFromServerRoot(std::filesystem::path file) const
	{
		std::filesystem::path full = _rootDirectory / file;
		std::filesystem::path absolute = std::filesystem::absolute(full);

		return absolute;
	}

	FileSecurityHandler::ValidationResult
	FileSecurityHandler::IsFilePathValid(std::filesystem::path path) const
	{
		if (!path.is_absolute())
		{
			return ValidationResult::INVALID_FORMAT;
		}

		std::filesystem::path validation = path;
		while (validation.has_relative_path())
		{
			validation = validation.parent_path();
			if (validation == _rootDirectory) 
			{
				return ValidationResult::VALID;
			}
		}

		return ValidationResult::INVALID_ESCAPE_ROOT;
	}

	FileSecurityHandler::ValidationResult 
	FileSecurityHandler::IsFileValidForWrite(std::filesystem::path path) const
	{
		if (!std::filesystem::exists(path) && !_canCreateFile )
		{ 
			return ValidationResult::INVALID_CANT_CREATE_FILE;
		}

		if (std::filesystem::exists(path) &&
			!std::filesystem::is_regular_file(path) )
		{
			return ValidationResult::INVALID_IS_DIRECTORY;
		}

		if (std::filesystem::exists(path) && !_canOverwriteFile)
		{
			return ValidationResult::INVALID_ACCESS_FORBIDDEN;
		}

		return IsFilePathValid(path);
	}

	FileSecurityHandler::ValidationResult 
	FileSecurityHandler::IsFileValidForRead(std::filesystem::path path) const
	{
		if (!_canReadFile)
		{
			return ValidationResult::INVALID_ACCESS_FORBIDDEN;
		}

		if (!std::filesystem::exists(path))
		{
			return ValidationResult::INVALID_NO_SUCH_FILE;
		}

		if (!std::filesystem::is_regular_file(path))
		{
			return ValidationResult::INVALID_IS_DIRECTORY;
		}

		return IsFilePathValid(path);
	}

	/* ***************************************************
	 *	File locking API
	 * ***************************************************/
	bool 
	FileSecurityHandler::LockFileForRead(std::filesystem::path file)
	{
		std::lock_guard lock{_mutex};

		return _locks[file].TryLockRead();
	}
	
	bool 
	FileSecurityHandler::LockFileForWrite(std::filesystem::path file)
	{
		std::lock_guard lock{ _mutex };

		return _locks[file].TryLockWrite();
	}

	bool 
	FileSecurityHandler::UnlockFileForRead(std::filesystem::path file)
	{
		std::lock_guard lock{ _mutex };

		auto it = _locks.find(file);
		if( it == _locks.end() ) return false; // wtf

		it->second.UnlockRead();
		if (it->second.IsFree())
		{
			_locks.erase(it);
		}

		return true;
	}
	
	bool 
	FileSecurityHandler::UnlockFileForWrite(std::filesystem::path file)
	{
		std::lock_guard lock{ _mutex };

		auto it = _locks.find(file);
		if (it == _locks.end()) return false; // wtf

		it->second.UnlockWrite();
		if (it->second.IsFree())
		{
			_locks.erase(it);
		}

		return true;
	}
}