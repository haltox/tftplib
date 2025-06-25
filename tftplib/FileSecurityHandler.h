#pragma once

#include <filesystem>
#include <optional>
#include "RWInterlock.h"
#include <mutex>
#include <unordered_map>

namespace tftplib 
{
	class FileSecurityHandler
	{

	public:
		enum class OverwritePolicy {
			DISALLOW,
			ALLOW
		};

		enum class FileCreationPolicy {
			DISALLOW,
			ALLOW
		};

		enum class ValidationResult {
			VALID,

			INVALID_FORMAT,

			INVALID_ESCAPE_ROOT,
			INVALID_CANT_CREATE_FILE,
			INVALID_NO_SUCH_FILE,
			INVALID_IS_DIRECTORY,
			INVALID_ACCESS_FORBIDDEN,

			INVALID_PERMISSIONS // OS LEVEL PERMISSIONS
		};

	public:
			
		/* ***************************************************
		 *	Setup API
		 *		Not thread safe.
		 *		Setup should be done _before_ FileSecurityHandler
		 *		is available to other threads.
		 * ***************************************************/

		FileSecurityHandler& SetRootDirectory(std::filesystem::path root);
		FileSecurityHandler& SetOverwritePolicy(OverwritePolicy policy);
		FileSecurityHandler& SetFileCreationPolicy(FileCreationPolicy policy);
		FileSecurityHandler& Reset();

		/* ***************************************************
		 *	Security handling API
		 *		Thread safety considerations : reentrant and safe
		 *		under the assumption that setup API isn't called
		 * ***************************************************/

		 std::filesystem::path
		 AbsoluteFromServerRoot(std::filesystem::path file) const;

		 ValidationResult IsFilePathValid(std::filesystem::path) const;
		 ValidationResult IsFileValidForWrite(std::filesystem::path) const;
		 ValidationResult IsFileValidForRead(std::filesystem::path) const;

		 /* ***************************************************
		  *	File locking API
		  *		Thread safety considerations : reentrant and safe
		  *		under the assumption that setup API isn't called
		  * ***************************************************/
		 bool LockFileForRead(std::filesystem::path file);
		 bool LockFileForWrite(std::filesystem::path file);
		 bool UnlockFileForRead(std::filesystem::path file);
		 bool UnlockFileForWrite(std::filesystem::path file);

		
	private:

		/* ***************************************************
		 *	Configuration
		 * ***************************************************/
		 
		 bool _canCreateFile { false };
		 bool _canOverwriteFile { false };
		 
		 bool _canCreateDirectories {false};
		 bool _canReadFile{ true };

		 std::filesystem::path _rootDirectory {""};

		 mutable std::mutex _mutex;
		 std::unordered_map< std::filesystem::path, RWInterlock> _locks;
	};
}
