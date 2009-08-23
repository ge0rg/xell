/* smc-ir.c - minimal IR receiver driver for XeLL
 *
 * Copyright (C) 2008 Georg Lukas <georg@boerde.de>
 *
 * This software is provided under the GNU GPL License v2.
 */
#include <cache.h>
#include <types.h>
#include <string.h>
#include <vsprintf.h>

volatile uint32_t *smc_base = (volatile uint32_t*)0x200ea001000LL;

int ignore_power = 3;

static inline unsigned int swap32(unsigned int v)
{
	return ((v&0xFF) << 24) | ((v&0xFF00)<<8) | ((v&0xFF0000)>>8) | ((v&0xFF000000)>>24);
}


int smc_hasdata() {
	return (swap32(smc_base[0x94/4]) & 4);
}

int smc_get(uint32_t *msg) {
	int pos;
	if (swap32(smc_base[0x94/4]) & 4) {
		smc_base[0x94/4] = swap32(4);
		for (pos=0; pos < 4; pos++)
			msg[pos] = smc_base[0x90/4];
		smc_base[0x94/4] = 0;
		return 1;
	}
	return 0;
}

void smc_send(uint32_t *msg) {
	int pos;
	while (!(swap32(smc_base[0x84/4]) & 4));

	smc_base[0x84/4] = swap32(4);
	for (pos=0; pos < 4; pos++)
		smc_base[0x80/4] = msg[pos];
	smc_base[0x84/4] = 0;
}

int smc_lastkey = -1;

int smc_getkey() {
	int ret = smc_lastkey;
	smc_lastkey = -1;
	return ret;
}

int smc_poll() {
	unsigned char msg[16], ans[16];
	memset(msg, 0, 16);

	if (smc_get((void*)&msg)) {
		if (ignore_power &&
		   ((msg[0] == 0x83 && msg[1] == 0x11 && msg[2] == 0x01)
#ifdef BLOCK_IR_REMOTE_POWER
		   || (msg[0] == 0x83 && msg[1] == 0x20 && msg[2] == 0x01)
#endif
		   )) {
			printf("PowerNAK");
			memset(ans, 0, 16);
			ans[0] = 0x82;
			ans[1] = 0x04;
			ans[2] = 0x33;
			smc_send((void*)ans);
			ignore_power--;
		} else
			printf("Received");
		int i;
		if (msg[0] == 0x83 && msg[1] == 0x23) {
			smc_lastkey = msg[3];
			printf(" IR code %d\n", smc_lastkey);
		} else
			for (i=0; i<16; i++)
				printf(" %02x", (uint32_t)msg[i]);
		printf("\n");
		return 1;
	}
	return 0;
}

