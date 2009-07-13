
/* 
	this xenos initialization is based on 
	reverse engineering the mode setting code and some ana dumps. 
	at the moment, it can only do 640x480p (over VGA).
	
*/

#include "xenon_smc.h"
#include "xenon_gpio.h"

#include <vsprintf.h>
#include <time.h>
#include <string.h>
#define assert(x)

#define require(x, label) if (!(x)) { printf("%s:%d [%s]\n", __FILE__, __LINE__, #x); goto label; }

static void xenos_write32(int reg, uint32_t val)
{
	*(volatile uint32_t*)(0x80000200ec800000 + reg) = val;
}

static uint32_t xenos_read32(int reg)
{
	return *(volatile uint32_t*)(0x80000200ec800000 + reg);
}

static void xenos_ana_write(int addr, uint32_t reg)
{
	xenos_write32(0x7950, addr);
	xenos_read32(0x7950);
	xenos_write32(0x7954, reg);
	while (xenos_read32(0x7950) == addr) printf(".");
}

uint32_t ana_640x480p [] = {
0x0000004f, 0x00000003, 0x00000000, 0x00000000, 0x00000000, 0xffffffff, 0x00000000, 0x00000000, 
0x00060001, 0x00000000, 0x00000000, 0xffffffff, 0xac0000d0, 0x00000009, 0x1c0ffc00, 0x00000000, 
0x24900000, 0x00087400, 0xf8461778, 0x48280320, 0x002c1061, 0x4601120d, 0x00000000, 0x02064f2e, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xc0000000, 0x00000000, 
0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x81612c88, 0x00000000, 0x10b3c8c3, 
0x00000000, 0x00080c0b, 0x00000000, 0x07e7d5dc, 0x00000000, 0x07bfb3d4, 0x00000000, 0x0014180e, 
0x00000000, 0x0000fe00, 0x0000fffd, 0xfd000000, 0x81612c88, 0x00000000, 0x10b3c8c3, 0x00000000, 
0x00080c0b, 0x00000000, 0x07e7d5dc, 0x00000000, 0x07bfb3d4, 0x00000000, 0x0014180e, 0x00000000, 
0x0000fe00, 0x0000fffd, 0xfd000000, 0x40000000, 0x00000000, 0x00000000, 0x00000000, 0x40000000, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xdeadbeef, 0x00000000, 0x00000000, 
0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x5f86605e, 0x00385b53, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0x00000000, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 

0xffffffff, 0xffffffff, 0x00000000, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 

0x000562de, 0x001162de, 0x002cfe1f, 0x00205218, 0x00115a17, 0x00000060, 0x00000060, 0x00000000, 
0x0000000f, 0x0001ee60, 0x00000002, 0x00000003, 0x000042aa, 0xffffffff, 0xffffffff, 0xffffffff, 

0x00000000, 0x00000000, 0x00000000, 0x8436f666, 0x0000001b, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 

0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 
0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x00000001, 0x00000000, 
};

void xenos_init_ana_new(uint32_t *mode_ana)
{
	uint32_t tmp;
	
	require(!xenon_smc_ana_read(0xfe, &tmp), ana_error);
	
	require(!xenon_smc_ana_read(0xD9, &tmp), ana_error);
	tmp &= ~(1<<18);
	require(!xenon_smc_ana_write(0xD9, tmp), ana_error);

	int addr_0[] =    {0,   0xD5, 0xD0,    0xD1,     0xD6, 0xD8, 0xD, 0xC};
	uint32_t val[] =  {0x0, 0x60, 0x55a9d, 0x1f4a9f, 0x60, 0xb,  1,   0xd8000000};
	
	xenon_smc_ana_write(0, 0);
	int i;
	for (i = 1; i < 8; ++i)
	{
		uint32_t old;
		xenon_smc_ana_read(addr_0[i], &old);
//		require(!xenon_smc_ana_write(addr_0[i], val[i]), ana_error);
		require(!xenon_smc_ana_write(addr_0[i], mode_ana[addr_0[i]]), ana_error);
		xenon_smc_ana_read(addr_0[i], &tmp);
	}
	
	require(!xenon_smc_ana_write(0, 0x60), ana_error);
	
	uint32_t old;
	xenos_write32(0x7938, (old = xenos_read32(0x7938)) & ~1);
	xenos_write32(0x7910, 1);
	xenos_write32(0x7900, 1);

	int addr_2[] = {2, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76};
	
	for (i = 0; i < 0x11; ++i)
		xenos_ana_write(addr_2[i], mode_ana[addr_2[i]]);

	for (i = 0x26; i <= 0x34; ++i)
		xenos_ana_write(i, mode_ana[i]);

	for (i = 0x35; i <= 0x43; ++i)
		xenos_ana_write(i, mode_ana[i]);

	for (i = 0x44; i <= 0x52; ++i)
		xenos_ana_write(i, mode_ana[i]);
	
	for (i = 0x53; i <= 0x54; ++i)
		xenos_ana_write(i, mode_ana[i]);
	
	for (i = 0x55; i <= 0x57; ++i)
		xenos_ana_write(i, mode_ana[i]);

	int addr_8[] = {3, 6, 7, 8, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};

	for (i = 0; i < 0x11; ++i)
		xenos_ana_write(addr_8[i], mode_ana[addr_8[i]]);

	xenos_write32(0x7938, old);

	xenon_smc_ana_write(0, mode_ana[0]);

	return;
	
ana_error:
	printf("error reading/writing ana\n");
}



void xenos_init_phase0(void)
{
	xenos_write32(0xe0, 0x10000000);
	xenos_write32(0xec, 0xffffffff);
	if (xenos_read32(0xec) != 0xffff)
		printf("xenos fail! (%08x)\n", xenos_read32(0xec));

//	#include "dump_wald.h"

	xenos_write32(0x1724, 0);
	int i;
	for (i = 0; i < 8; ++i)
	{
		int addr[] = {0x8000, 0x8c60, 0x8008, 0x8004, 0x800c, 0x8010, 0x8014, 0x880c, 0x8800, 0x8434, 0x8820, 0x8804, 0x8824, 0x8828, 0x882c, 0x8420, 0x841c, 0x841c, 0x8418, 0x8414, 0x8c78, 0x8c7c, 0x8c80};
		int j;
		for (j = 0; j < 23; ++j)
			xenos_write32(addr[j] + i * 0x1000, 0);
	}
}

void xenos_init_phase1(void)
{
	uint32_t v;
	xenos_write32(0x0244, 0x20000000); // 
	v = 0x200c0011;
	xenos_write32(0x0244, v);
	udelay(1000);
	v &=~ 0x20000000;
	xenos_write32(0x0244, v);
	udelay(1000);
	
	assert(xenos_read32(0x0244) == 0xc0011);

	xenos_write32(0x0218, 0);
	xenos_write32(0x3c04, 0xe);
	xenos_write32(0x00f4, 4);
	
	xenos_write32(0x0204, 0x400300); // 4400380 for jasper
	xenos_write32(0x0208, 0x180002);
	
	xenos_write32(0x01a8, 0);
	xenos_write32(0x0e6c, 0x0c0f0000);
	xenos_write32(0x3400, 0x40401);
	udelay(1000);
	xenos_write32(0x3400, 0x40400);
	xenos_write32(0x3300, 0x3a22);
	xenos_write32(0x340c, 0x1003f1f);
	xenos_write32(0x00f4, 0x1e);
	
	
	xenos_write32(0x2800, 0);
	xenos_write32(0x2804, 0x20000000);
	xenos_write32(0x2808, 0x20000000);
	xenos_write32(0x280c, 0);
	xenos_write32(0x2810, 0x20000000);
	xenos_write32(0x2814, 0);

	udelay(1000);	
	xenos_write32(0x6548, 0);
		
	xenos_write32(0x04f0, (xenos_read32(0x04f0) &~ 0xf0000) | 0x40100);
}

static void f1(void)
{
	uint32_t tmp;
	xenon_smc_ana_read(0, &tmp);
	tmp &= ~0x1e;
	xenon_smc_ana_write(0, tmp);
}

void xenos_ana_preinit(void)
{
	f1();
	uint32_t v;
	xenos_write32(0x6028, v = (xenos_read32(0x6028) | 0x01000000));
	while (!(xenos_read32(0x281c) & 2)) printf(".");
	xenon_smc_ana_write(0, 0);
	xenon_smc_ana_write(0xd7, 0xff);
	xenos_write32(0x6028, v & ~0x301);
	xenos_write32(0x7900, 0);
	xenon_smc_ana_read(0xd9, &v);
	xenon_smc_ana_write(0xd9, v | 0x40000);
}

int total_width = 800, total_height = 525, hsync_offset = 0x56, real_active_width = 640, active_height = 480, vsync_offset = 0x23, is_progressive = 1, width = 640;

void xenos_set_mode_f1(void)
{
	xenos_write32(0x60e8, 1);
	xenos_write32(0x6cbc, 3);
	xenos_write32(0x60ec, 0);
	xenos_write32(0x6020, 0);

	int interlace_factor = is_progressive ? 1 : 2;
	
	xenos_write32(0x6000, total_width - 1);
	xenos_write32(0x6010, total_height - 1);
	xenos_write32(0x6004, (hsync_offset << 16) | (real_active_width + hsync_offset));
	xenos_write32(0x6014, (vsync_offset << 16) | (active_height * interlace_factor + vsync_offset));
	xenos_write32(0x6008, 0x100000);
	xenos_write32(0x6018, 0x60000);
	xenos_write32(0x6030, is_progressive ? 0 : 1);
	xenos_write32(0x600c, 0);
	xenos_write32(0x601c, 0);

	xenos_write32(0x604c, 0);
	xenos_write32(0x6050, 0);
	xenos_write32(0x6044, 0);
	xenos_write32(0x6048, 0);

	xenos_write32(0x60e8, 0);
}

void xenos_set_mode_f2(void)
{
	int interlace_factor = is_progressive ? 1 : 2;
	
	xenos_write32(0x6144, 1);
	xenos_write32(0x6120, width);
	xenos_write32(0x6104, 2);
	xenos_write32(0x6108, 0);
	xenos_write32(0x6124, 0);
	xenos_write32(0x6128, 0);
	xenos_write32(0x612c, 0);
	xenos_write32(0x6130, 0);
	xenos_write32(0x6134, width);
	xenos_write32(0x6138, active_height * interlace_factor);
	xenos_write32(0x6110, 0x1f920000);
	xenos_write32(0x6120, 640);
	xenos_write32(0x6100, 1);
	xenos_write32(0x6144, 0);
	
	xenos_write32(0x65cc, 1);
	xenos_write32(0x6590, 0);
	xenos_write32(0x2840, 0x1f920000);
	xenos_write32(0x2844, width);
	xenos_write32(0x2848, 0x80000);
	
	xenos_write32(0x6580, 0);
	xenos_write32(0x6584, (width << 16) | (active_height * interlace_factor));
	xenos_write32(0x65e8, (width >> 2) - 1);
	xenos_write32(0x6528, is_progressive ? 0 : 1);
	xenos_write32(0x6550, 0xff);
	xenos_write32(0x6524, 0x300);
	xenos_write32(0x65d0, 0x100);
	xenos_write32(0x6148, 1);
	
	xenos_write32(0x6594, 0x905);
	xenos_write32(0x6584, 0x028001e0);
	xenos_write32(0x65e8, 0x9f);
	xenos_write32(0x6580, 0);
	xenos_write32(0x612c, 0);
	xenos_write32(0x6130, 0);
	xenos_write32(0x6434, 640);
	xenos_write32(0x6138, 480);
	xenos_write32(0x6044, 0);
	xenos_write32(0x6048, 0);
	xenos_write32(0x604c, 0);
	xenos_write32(0x6050, 0);
	xenos_write32(0x65a0, 0);
	xenos_write32(0x65b4, 0x01000000);
	xenos_write32(0x65c4, 0x01000000);
	xenos_write32(0x65b0, 0);
	xenos_write32(0x65c0, 0x01000000);
	xenos_write32(0x65b8, 0x00060000);
	xenos_write32(0x65c8, 0x00040000);
	xenos_write32(0x65dc, 0);
	
	xenos_write32(0x65cc, 0);
	
	xenos_write32(0x64C0, 0);
	xenos_write32(0x6488, 0);
	xenos_write32(0x6484, 0);
	xenos_write32(0x649C, 7);
	xenos_write32(0x64a0, 1);
	while (!(xenos_read32(0x64a0) & 2));
}

void xenos_set_mode(void)
{
	xenos_write32(0x7938, xenos_read32(0x7938) | 1);
	xenos_write32(0x06ac, 1);
	
	xenos_ana_preinit();
	xenos_write32(0x04a0, 0x100);
	xenos_init_ana_new(ana_640x480p);
//	xenos_init_ana();
	xenos_write32(0x7900, 1); 

	xenos_set_mode_f1();
	xenos_set_mode_f2();

	xenos_write32(0x6028, 0x10001);
	xenos_write32(0x6038, 0x04010040);
	xenos_write32(0x603c, 0);
	xenos_write32(0x6040, 0x04010040);
	xenos_write32(0x793c, 0);
	xenos_write32(0x7938, 0x700);

	
	xenos_write32(0x6024, 0x04010040);
	xenos_write32(0x6054, 0x00010002);
	xenos_write32(0x6058, 0xec02414a);

	xenos_write32(0x6030, 0);
	xenos_write32(0x6034, 0);
	
	xenos_write32(0x6060, 0x000000ec);
	xenos_write32(0x6064, 0x014a00ec);
	xenos_write32(0x6068, 0x00d4014a);
	xenos_write32(0x6110, 0x1f920000);

		/* color matrix */	
	xenos_write32(0x6380, 0x00000001);
	xenos_write32(0x6384, 0x00db4000);
	xenos_write32(0x6388, 0x00000000);
	xenos_write32(0x638c, 0x00000000);
	xenos_write32(0x6390, 0x00408000);
	xenos_write32(0x6394, 0x00000000);
	xenos_write32(0x6398, 0x00db4000);
	xenos_write32(0x639c, 0x00000000);
	xenos_write32(0x63a0, 0x00408000);
	xenos_write32(0x63a4, 0x00000000);
	xenos_write32(0x63a8, 0x00000000);
	xenos_write32(0x63ac, 0x00db4000);
	xenos_write32(0x63b0, 0x00408000);
	xenos_write32(0x63b4, 0x00000000);
}

void xenos_lowlevel_init(void)
{
	printf(" * Xenos init: p0");
	xenos_init_phase0();
	printf(",p1");
	xenos_init_phase1();
	
	printf(",gpio");
	
	xenon_gpio_set(0, 0x2300);
	xenon_gpio_set_oe(0, 0x2300);

	printf(",mode");
	xenos_set_mode();
	printf(",ana");
	xenon_smc_ana_write(0xdf, 0); 
	printf("\n");
}
