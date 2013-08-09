#include "x00fs.hpp"

////////////////////////////////////////////////////////////////////////////////
//
// P00 file format implementation
//
/* TODO: Move as P00 format to PI, recode.
bool P00_reset(char *s)
{
	byte i;

	// Check filetype, and skip past 26 bytes
	// Actually, compare here for string 'C64File' not only 'C64'.
	if((fatFgetc() == 'C') and (fatFgetc() == '6') and (fatFgetc() == '4')) {

		for(i = 0; i < 23; i++)
			fatFgetc();

		if(!fatFeof())
			// All is ok
			return true;
	}

	return false;
} // P00_reset
*/

/* TODO: Move as P00 format to PI, recode.
bool close_to_fat(void)
{
	// Close and back to fat
	m_interfaceState = IS_NATIVE;
	fatFclose();
	return true;
} // close_to_fat
*/


x00FS::x00FS()
{
} // ctor


bool x00FS::putc(char c)
{
	Q_UNUSED(c);
	// not supported, as of now.
	return false;
}
