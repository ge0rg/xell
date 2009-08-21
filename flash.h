#ifndef __flash_h
#define __flash_h

void sfcx_writereg(int addr, unsigned long data);
unsigned long sfcx_readreg(int addr);
void readsector(unsigned char *data, int sector, int raw);
void flash_erase(int address);
void write_page(int address, unsigned char *data);
void calcecc(unsigned char *data);

#endif
