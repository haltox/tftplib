#pragma once

#include <memory>

namespace tftplib {

	// Totally not a ring buffer
	class HaloBuffer
	{
	private:
		class Os;

	public:
		HaloBuffer(size_t size);
		~HaloBuffer();

		size_t Size() const;
		void* Get() const;

		template <typename T>
		T* Get() const;

		template <typename T>
		T* GetAt(size_t at) const;

	private:
		std::unique_ptr<Os> _os;

	};

	template <typename T>
	T* HaloBuffer::Get() const
	{
		return (T*)Get();
	}

	template <typename T>
	T* HaloBuffer::GetAt(size_t at) const
	{
		T* buf = Get<T>();
		return &buf[at];
	}
}