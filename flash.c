#include "include/types.h"
#include <vsprintf.h>

#define STATUS  (1*4)
#define COMMAND (2*4)
#define ADDRESS (3*4)
#define DATA    (4*4)

static inline uint32_t bswap32(uint32_t t)
{
	return ((t & 0xFF) << 24) | ((t & 0xFF00) << 8) | ((t & 0xFF0000) >> 8) | ((t & 0xFF000000) >> 24);
}

void sfcx_writereg(int addr, unsigned long data)
{	
	*(volatile unsigned int*)(0x200ea00c000ULL|addr) = data;
}

unsigned long sfcx_readreg(int addr)
{
	return *(volatile unsigned int*)(0x200ea00c000ULL|addr);
}

void readsector(unsigned char *data, int sector, int raw)
{
	int status;
	sfcx_writereg(STATUS, sfcx_readreg(STATUS));
	sfcx_writereg(ADDRESS, sector);	
	sfcx_writereg(COMMAND, raw ? 3 : 2);

	while ((status = sfcx_readreg(STATUS))&1);
 
	if (status != 0x200)	 
		printf("error reading %08x: %08x\n", sector, status);

	sfcx_writereg(ADDRESS, 0);

	int i;
	for (i = 0; i < 0x210; i+=4)
	{
		sfcx_writereg(ADDRESS, i);
		*(int*)(data + i) = bswap32(sfcx_readreg(DATA));
	}
	
}

void flash_erase(int address)
{
	sfcx_writereg(0, sfcx_readreg(0) | 8);
	sfcx_writereg(STATUS, 0xFF);
	sfcx_writereg(ADDRESS, address);
	while (sfcx_readreg(STATUS) & 1);
	sfcx_writereg(COMMAND, 0xAA);
	sfcx_writereg(COMMAND, 0x55);
	while (sfcx_readreg(STATUS) & 1);
	sfcx_writereg(COMMAND, 0x5);
	while (sfcx_readreg(STATUS) & 1) { printf(".");  }
	printf("[%08x]", sfcx_readreg(STATUS));
	sfcx_writereg(STATUS, 0xFF);
	sfcx_writereg(0, sfcx_readreg(0) & ~8);
}

void write_page(int address, unsigned char *data)
{
	sfcx_writereg(0, sfcx_readreg(0) | 8);
	sfcx_writereg(STATUS, 0xFF);
	sfcx_writereg(ADDRESS, 0);

	int i;

	for (i = 0; i < 0x210; i+=4)
		sfcx_writereg(DATA, bswap32(*(int*)(data + i)));

	sfcx_writereg(ADDRESS, address);
	sfcx_writereg(COMMAND, 0x55);
	while (sfcx_readreg(STATUS) & 1);
	sfcx_writereg(COMMAND, 0xAA);
	while (sfcx_readreg(STATUS) & 1);
	sfcx_writereg(COMMAND, 0x4);
	while (sfcx_readreg(STATUS) & 1) { printf("."); }
	printf("[%08x]", sfcx_readreg(STATUS));
	sfcx_writereg(0, sfcx_readreg(0) & ~8);
}

void calcecc(unsigned long *data)
{
	int i=0, val=0;
	
	unsigned long v;
	
	for (i = 0; i < 0x1066; i++)
	{
		if (!(i & 31))
			v = ~*data++; /* byte order: LE */
		val ^= v & 1;
		v>>=1;
		if (val & 1)
			val ^= 0x6954559;
		val >>= 1;
	}
	
	val = ~val;
	
	data[0x20C] |= (val << 6) & 0xC0;
	data[0x20D] = (val >> 2) & 0xFF;
	data[0x20E] = (val >> 10) & 0xFF;
	data[0x20F] = (val >> 18) & 0xFF;
}
