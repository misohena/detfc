#ifndef DETFC_BINARYIO_H_INCLUDED
#define DETFC_BINARYIO_H_INCLUDED

#include <istream>
#include <ostream>

namespace detfc{

// Binary I/O

template<typename T>
void writeBinary(std::ostream &os, const T &v)
{
	os.write(reinterpret_cast<const char *>(&v), sizeof(v));
}

template<typename T>
T readBinary(std::istream &is)
{
	T v = T();
	is.read(reinterpret_cast<char *>(&v), sizeof(v));
	return v;
}

void writeStringBinary(std::ostream &os, std::string &v)
{
	writeBinary(os, v.size());
	os.write(v.data(), v.size());
}

std::string readStringBinary(std::istream &is)
{
	typedef char CHAR;
	const std::size_t size = readBinary<std::size_t>(is);
	if (!is){
		return std::string();
	}
	if (size == 0){
		return std::string();
	}
	const std::size_t MAX_CHARS = 1024;
	if (size <= MAX_CHARS){
		CHAR buf[MAX_CHARS];
		is.read(buf, size);
		return std::string(buf, buf + size);
	}
	else{
		std::unique_ptr<CHAR[]> buf(new CHAR[size]);
		is.read(buf.get(), size);
		return std::string(buf.get(), buf.get() + size);
	}
}

}//namespace detfc
#endif
