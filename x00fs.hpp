#ifndef X00FS_HPP
#define X00FS_HPP

#include "nativefs.hpp"

class x00FS : public NativeFS
{
public:
	x00FS();
	bool putc(char c);
};

#endif // X00FS_HPP
