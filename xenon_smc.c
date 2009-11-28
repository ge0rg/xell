#include <types.h>
#include <string.h>

#define SMC_BASE 0x80000200ea001000

inline uint32_t bswap32(uint32_t t)
{
	return ((t & 0xFF) << 24) | ((t & 0xFF00) << 8) | ((t & 0xFF0000) >> 8) | ((t & 0xFF000000) >> 24);
}

static inline uint32_t read32(long addr)
{
	return bswap32(*(volatile uint32_t*)addr);
}

static inline uint32_t read32n(long addr)
{
	return *(volatile uint32_t*)addr;
}

static inline void write32(long addr, uint32_t val)
{
	*(volatile uint32_t*)addr = bswap32(val);
}

static inline void write32n(long addr, uint32_t val)
{
	*(volatile uint32_t*)addr = val;
}

void xenon_smc_send_message(unsigned char *msg)
{
/*	printf("SEND: ");
	int i;
	for (i = 0; i < 16; ++i)
		printf("%02x ", msg[i]);
	printf("\n");
*/
	while (!(read32(SMC_BASE + 0x84) & 4));
	write32(SMC_BASE + 0x84, 4);
	write32n(SMC_BASE + 0x80, *(uint32_t*)(msg + 0));
	write32n(SMC_BASE + 0x80, *(uint32_t*)(msg + 4));
	write32n(SMC_BASE + 0x80, *(uint32_t*)(msg + 8));
	write32n(SMC_BASE + 0x80, *(uint32_t*)(msg + 12));
	write32(SMC_BASE + 0x84, 0);
}

int xenon_smc_receive_message(unsigned char *msg)
{
	if (read32(SMC_BASE + 0x94) & 4)
	{
		uint32_t *msgl = (uint32_t*)msg;
		int i;
		write32(SMC_BASE + 0x94, 4);
		for (i = 0; i < 4; ++i)
			*msgl++ = read32n(SMC_BASE + 0x90);
		write32(SMC_BASE + 0x94, 0);
		return 0;
	}
	return -1;
}

void xenon_smc_handle_bulk(unsigned char *msg)
{
	switch (msg[1])
	{
	case 0x11:
	case 0x20:
		printf("SMC power message\n");
		break;
	case 0x23:
		printf("IR RX [%02x %02x]\n", msg[2], msg[3]);
		break;
	case 0x60 ... 0x65:
		printf("DVD cover state: %02x\n", msg[1]);
		break;
	default:
		printf("unknown SMC bulk msg\n");
		break;
	}
}

int xenon_smc_receive_response(unsigned char *msg)
{
	while (1)
	{
		if (xenon_smc_receive_message(msg))
			continue;

/*		printf("REC: ");
		int i;
		for (i = 0; i < 16; ++i)
			printf("%02x ", msg[i]);
		printf("\n");
*/
		if (msg[0] == 0x83)
		{
			xenon_smc_handle_bulk(msg);
			continue;
		}
		return 0;
	}
}

int xenon_smc_ana_write(uint8_t addr, uint32_t val)
{
	uint8_t buf[16];
	
	memset(buf, 0, 16);
	
	buf[0] = 0x11;
	buf[1] = 0x60;
	buf[3] = 0x80 | 0x70;
	
	buf[6] = addr;
	
	buf[8] = val;
	buf[9] = val >> 8;
	buf[10] = val >> 16;
	buf[11] = val >> 24;

	xenon_smc_send_message(buf);

	xenon_smc_receive_response(buf);
	if (buf[1] != 0)
	{
		printf("xenon_smc_ana_write failed, addr=%02x, err=%d\n", addr, buf[1]);
		return -1;
	}
	
	return 0;
}
int xenon_smc_ana_read(uint8_t addr, uint32_t *val)
{
	uint8_t buf[16];
	memset(buf, 0, 16);

	buf[0] = 0x11;
	buf[1] = 0x10;
	buf[2] = 5;
	buf[3] = 0x80 | 0x70;
	buf[5] = 0xF0;
	buf[6] = addr;
	
	xenon_smc_send_message(buf);
	xenon_smc_receive_response(buf);
	if (buf[1] != 0)
	{
		printf("xenon_smc_ana_read failed, addr=%02x, err=%d\n", addr, buf[1]);
		return -1;
	}
	*val = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
	return 0;
}

void xenon_smc_set_led(int override, int value)
{
	uint8_t buf[16];
	memset(buf, 0, 16);
	
	buf[0] = 0x99;
	buf[1] = override;
	buf[2] = value;
	
	xenon_smc_send_message(buf);
}

void xenon_smc_power_shutdown(void)
{
	uint8_t buf[16] = {0x82, 0x11, 0x01};
	xenon_smc_send_message(buf);
}

void xenon_smc_start_bootanim(void)
{
	uint8_t buf[16] = {0x8c, 0x03, 0x01};
	xenon_smc_send_message(buf);
}


void xenon_gpio_set_oe(uint32_t clear, uint32_t set)
{
	write32(SMC_BASE + 0x20, (read32(SMC_BASE + 0x20) &~ clear) | set);
}

void xenon_gpio_set(uint32_t clear, uint32_t set)
{
	write32(SMC_BASE + 0x34, (read32(SMC_BASE + 0x34) &~ clear) | set);
}

int xenon_smc_read_avpack(void)
{
	uint8_t buf[16] = {0x0f};
	xenon_smc_send_message(buf);
	xenon_smc_receive_response(buf);
	return buf[1];
}
