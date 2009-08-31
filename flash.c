#include "include/types.h"
#include <vsprintf.h>

#define STATUS  (1)
#define COMMAND (2)
#define ADDRESS (3)
#define DATA    (4)

static inline uint32_t bswap_32(uint32_t t)
{
	return ((t & 0xFF) << 24) | ((t & 0xFF00) << 8) | ((t & 0xFF0000) >> 8) | ((t & 0xFF000000) >> 24);
}

void sfcx_writereg(int addr, unsigned long data)
{	
	*(volatile unsigned int*)(0x200ea00c000ULL|(addr*4)) = bswap_32(data);
}

unsigned long sfcx_readreg(int addr)
{
	return bswap_32(*(volatile unsigned int*)(0x200ea00c000ULL|(addr*4)));
}

void readsector(unsigned char *data, int sector, int raw)
{
	int status;
	sfcx_writereg(STATUS, sfcx_readreg(STATUS));
	sfcx_writereg(ADDRESS, sector);	
	sfcx_writereg(COMMAND, raw ? 3 : 2);

	while ((status = sfcx_readreg(STATUS))&1);
 
	if (status != 0x200)
	{
		if (status & 0x40)
			printf(" * Bad block found at %08x\n", sector);
		else if (status & 0x1c)
			printf(" * (corrected) ECC error %08x: %08x\n", sector, status);
		else if (!raw && (status & 0x800))
			printf(" * illegal logical block %08x (status: %08x)\n", sector, status);
		else
			printf(" * Unknown error at %08x: %08x. Please worry.\n", sector, status);
	}

	sfcx_writereg(ADDRESS, 0);

	int i;
	for (i = 0; i < 0x210; i+=4)
	{
		sfcx_writereg(COMMAND, 0);
		*(int*)(data + i) = bswap_32(sfcx_readreg(DATA));
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
	while (sfcx_readreg(STATUS) & 1);
	int status = sfcx_readreg(STATUS);
	if (status != 0x200)
		printf("[%08x]", status);
	sfcx_writereg(STATUS, 0xFF);
	sfcx_writereg(0, sfcx_readreg(0) & ~8);
}

void write_page(int address, unsigned char *data)
{
	sfcx_writereg(STATUS, 0xFF);
	sfcx_writereg(0, sfcx_readreg(0) | 8);

	sfcx_writereg(ADDRESS, 0);

	int i;

	for (i = 0; i < 0x210; i+=4)
	{
		sfcx_writereg(DATA, bswap_32(*(int*)(data + i)));
		sfcx_writereg(COMMAND, 1);
	}

	sfcx_writereg(ADDRESS, address);
	sfcx_writereg(COMMAND, 0x55);
	while (sfcx_readreg(STATUS) & 1);
	sfcx_writereg(COMMAND, 0xAA);
	while (sfcx_readreg(STATUS) & 1);
	sfcx_writereg(COMMAND, 0x4);
	while (sfcx_readreg(STATUS) & 1);
	int status = sfcx_readreg(STATUS);
	if (status != 0x200)
		printf("[%08x]", status);
	sfcx_writereg(0, sfcx_readreg(0) & ~8);
}


void calcecc(unsigned int *data)
{
	unsigned int i=0, val=0;
	unsigned char *edc = ((unsigned char*)data) + 0x200;
	
	unsigned int v;
	
	for (i = 0; i < 0x1066; i++)
	{
		if (!(i & 31))
			v = ~bswap_32(*data++);
		val ^= v & 1;
		v>>=1;
		if (val & 1)
			val ^= 0x6954559;
		val >>= 1;
	}
	
	val = ~val;
	
	
	edc[0xC] |= (val << 6) & 0xC0;
	edc[0xD] = (val >> 2) & 0xFF;
	edc[0xE] = (val >> 10) & 0xFF;
	edc[0xF] = (val >> 18) & 0xFF;
}
