#pragma once

#include <atomic>
#include <stdexcept>

namespace tftplib 
{
	template <typename T>
	class RingBuffer 
	{

	public:
		RingBuffer(size_t size);
		~RingBuffer();
		RingBuffer(RingBuffer&&);
		RingBuffer& operator=(RingBuffer&&);

		RingBuffer(const RingBuffer&) = delete;
		RingBuffer& operator=(const RingBuffer&) = delete;
		
		bool IsEmpty() const {
			return _readHead == _writeHead;
		}

		bool IsFull() const {
			return (_writeHead + 1) % _size == _readHead;
		}

		bool Write(const T& v);

		T Read();
	
	private:
		std::atomic<size_t> _readHead{ 0 };
		std::atomic<size_t> _writeHead{ 0 };

		const size_t _size;
		T* _buffer{ nullptr };
	};


}

template <typename T>
tftplib::RingBuffer<T>::RingBuffer(size_t size)
	: _size{size}
{
	if (size < 4) {
		throw std::runtime_error("RingBuffer size must be greater than 4");
	}
	_buffer = new T[size];
	_readHead = 0;
	_writeHead = 0;
}

template <typename T>
tftplib::RingBuffer<T>::~RingBuffer()
{
	delete[] _buffer;
}

template <typename T>
tftplib::RingBuffer<T>::RingBuffer(RingBuffer&& rhs)
{
	*this = std::move(rhs);
}


template <typename T>
tftplib::RingBuffer<T>& 
tftplib::RingBuffer<T>::operator=(RingBuffer&& rhs)
{
	std::swap(_readHead, rhs._readHead);
	std::swap(_writeHead, rhs._writeHead);
	std::swap(_size, rhs._size);
	std::swap(_buffer, rhs._buffer);
	return *this;
}

template <typename T>
bool tftplib::RingBuffer<T>::Write(const T& v)
{
	if (IsFull()) {
		return false;
	}

	_buffer[_writeHead] = v;
	_writeHead = (_writeHead + 1) % _size;

	return true;
}

template <typename T>
T tftplib::RingBuffer<T>::Read()
{
	if (IsEmpty()) {
		throw std::runtime_error("RingBuffer is empty");
	}

	T value = _buffer[_readHead];
	_buffer[_readHead] = T{};
	_readHead = (_readHead + 1) % _size;
	return value;
}