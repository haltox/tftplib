#include "pch.h"
#include "HaloBuffer.h"
#include <stdexcept>

#include <windows.h>
#include <WinBase.h>

namespace tftplib {
	class HaloBuffer::Os 
	{
	public:
		static Os* Make(size_t size);
		static size_t GetPageSize();

	public:

		~Os();
		void* Buffer() const;
		size_t Size() const;

	private:
		Os();
		void Free();

		bool Allocate(size_t size);
		size_t Align(size_t size);
	

	private:
		void* _actualBuffer{ nullptr };
	
		HANDLE _physicalPage{ INVALID_HANDLE_VALUE };
		void* _view1{ nullptr };
		void* _view2{ nullptr };
		void* _firstSegment{ nullptr };
		void* _secondSegment{ nullptr };

		size_t _physicalMemorySize {0};

	};

	HaloBuffer::Os* HaloBuffer::Os::Make(size_t size)
	{
		HaloBuffer::Os *os = new HaloBuffer::Os{};
		if( !os->Allocate(size) )
		{
			delete os;
			os = nullptr;
		}

		return os;
	}

	size_t HaloBuffer::Os::GetPageSize()
	{
		static size_t pageSize = 0;

		if (pageSize == 0)
		{
			SYSTEM_INFO sys{};
			GetSystemInfo(&sys);
			pageSize = sys.dwPageSize;
		}

		return pageSize;
	}

	HaloBuffer::Os::~Os()
	{
		Free();
	}

	void* HaloBuffer::Os::Buffer() const
	{
		return _actualBuffer;
	}

	size_t HaloBuffer::Os::Size() const
	{
		return _physicalMemorySize;
	}

	HaloBuffer::Os::Os() {};

	void HaloBuffer::Os::Free()
	{
		UnmapViewOfFile(_view1);
		UnmapViewOfFile(_view2);
		CloseHandle(_physicalPage);
		VirtualFree(_secondSegment, 0, MEM_RELEASE);
		VirtualFree(_actualBuffer, 0, MEM_RELEASE);
	
		_actualBuffer	= nullptr;
		_firstSegment	= nullptr;
		_secondSegment	= nullptr;
		_physicalPage	= nullptr;
		_view1			= nullptr;
		_view2			= nullptr;
	}

	bool HaloBuffer::Os::Allocate(size_t size)
	{
		_physicalMemorySize = Align(size);

		try {
			_actualBuffer = VirtualAlloc2(
				nullptr, // Same process
				nullptr, // No starting address, get a new block
				2 * _physicalMemorySize,
				MEM_RESERVE | MEM_RESERVE_PLACEHOLDER,
				PAGE_NOACCESS,
				nullptr, 0 // No additional parameters
			);

			if (_actualBuffer == nullptr)
			{
				throw std::runtime_error{"can't alloc buf"};
			}
	#pragma warning( push )
	#pragma warning( disable : 6333 )
	#pragma warning( disable : 28160 )
			if (!VirtualFree(_actualBuffer, 
				_physicalMemorySize,
				MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER))
			{
				throw std::runtime_error{ "couldn't segment buffer" };
			}
	#pragma warning( pop )

			_firstSegment = _actualBuffer;
			_secondSegment = (char*)_actualBuffer + _physicalMemorySize;

			DWORD high = (_physicalMemorySize >> 32) & 0xFFFFFFFF;
			DWORD low = (_physicalMemorySize >> 0) & 0xFFFFFFFF;
			_physicalPage = CreateFileMapping(
				INVALID_HANDLE_VALUE,	// Create file mapping backed by a paging file
				nullptr,				// no inherit
				PAGE_READWRITE,			// rw access
				high,					// high order bytes of size
				low,					// Low-order bytes of size
				nullptr					// anonymous region
			);

			if (_physicalPage == NULL) {
				throw std::runtime_error{ "couldn't allocate file mapping" };
			}

			_view1 = (char*)MapViewOfFile3(
				_physicalPage,
				nullptr,
				_firstSegment,
				high,
				low,
				MEM_REPLACE_PLACEHOLDER,
				PAGE_READWRITE,
				nullptr, 0
			);

			_view2 = (char*)MapViewOfFile3(
				_physicalPage,
				nullptr,
				_secondSegment,
				high,
				low,
				MEM_REPLACE_PLACEHOLDER,
				PAGE_READWRITE,
				nullptr, 0
			);

			if (_view1 == nullptr || _view2 == nullptr)
			{
				throw std::runtime_error{ "view mapping failed" };
			}
		}
		catch (std::runtime_error err)
		{
			Free();
			return false;
		}

		return true;
	}

	size_t HaloBuffer::Os::Align(size_t size)
	{
		size_t align = HaloBuffer::Os::GetPageSize();
		size_t keepAligned = ~(align - 1);
	
		return (size & keepAligned) == size
			? size
			: (size & keepAligned) + align;
	}

	HaloBuffer::HaloBuffer(size_t size)
	{
		_os.reset(Os::Make(size));
		if (_os == nullptr)
		{
			throw std::runtime_error {"Could not allocate buffer"};
		}
	}

	HaloBuffer::~HaloBuffer(){}

	size_t HaloBuffer::Size() const
	{
		return _os->Size();
	}

	void* HaloBuffer::Get() const
	{
		return _os->Buffer();
	}
}