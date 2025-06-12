#pragma once

#include <streambuf>

class streambuf_noop : public std::basic_streambuf<char, std::char_traits<char> >
{
public:
	streambuf_noop() = default;
	virtual ~streambuf_noop() override = default;

protected:


	virtual int_type overflow(int_type c = traits_type::eof()) override
	{
		return traits_type::to_int_type(c);
	}

	virtual std::streamsize xsputn(const char* s, std::streamsize n) override {
		return std::min(strlen(s), (std::size_t)n);
	}
	
};

