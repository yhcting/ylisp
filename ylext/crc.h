#ifndef __CRc_h__
#define __CRc_h__

/**
 * Assumption : sizeof(unsigned short) == 16
 * @crc     : prevvious crc value
 * @buffer  : data pointer
 * @len     : number of bytes in the buffer.
 */
unsigned short
crc16(unsigned short crc, const unsigned char* data, unsigned int len);


/**
 * Assumption : sizeof(unsigned int) == 32
 * See above 'ycrc16' for details.
 */
unsigned int
crc32(unsigned int crc, const unsigned char* data, unsigned int len);


#endif /* __CRc_h__ */
