#include <types.h>
#include <cache.h>
#include <vsprintf.h>
#include <string.h>
#include <network.h>
#include <processor.h>
#include <time.h>
#include "version.h"
#include <diskio.h>
#include "flash.h"

#define MENU
/* cdrom.c: */
extern int iso9660_load_file(char *filename, void* addr);
extern void try_boot_cdrom(char *);
/* xenos.c: */
extern void xenos_init();
extern void xenos_putch(const char c);

extern char *network_boot_file_name();
extern char *network_boot_server_name();

static inline uint32_t bswap_32(uint32_t t)
{
	return ((t & 0xFF) << 24) | ((t & 0xFF00) << 8) | ((t & 0xFF0000) >> 8) | ((t & 0xFF000000) >> 24);
}

#include "elf_abi.h"

static void putch(unsigned char c)
{
	while (!((*(volatile uint32_t*)0x80000200ea001018)&0x02000000));
	*(volatile uint32_t*)0x80000200ea001014 = (c << 24) & 0xFF000000;
}

static int kbhit(void)
{
	uint32_t status;
	
	do
		status = *(volatile uint32_t*)0x80000200ea001018;
	while (status & ~0x03000000);
	
	return !!(status & 0x01000000);
}

int getchar(void)
{
	while (!kbhit());
	return (*(volatile uint32_t*)0x80000200ea001010) >> 24;
}

int putchar(int c)
{
	if (c == '\n')
		putch('\r');
	putch(c);
	xenos_putch(c);
	return 0;
}

void putstring(const char *c)
{
	while (*c)
		putchar(*c++);
}

int puts(const char *c)
{
	putstring(c);
	putstring("\n");
	return 0;
}

/* e_ident */
#define IS_ELF(ehdr) ((ehdr).e_ident[EI_MAG0] == ELFMAG0 && \
                      (ehdr).e_ident[EI_MAG1] == ELFMAG1 && \
                      (ehdr).e_ident[EI_MAG2] == ELFMAG2 && \
                      (ehdr).e_ident[EI_MAG3] == ELFMAG3)

unsigned long load_elf_image(void *addr)
{
	Elf32_Ehdr *ehdr;
	Elf32_Shdr *shdr;
	unsigned char *strtab = 0;
	int i;

	ehdr = (Elf32_Ehdr *) addr;
	
	shdr = (Elf32_Shdr *) (addr + ehdr->e_shoff + (ehdr->e_shstrndx * sizeof (Elf32_Shdr)));

	if (shdr->sh_type == SHT_STRTAB)
		strtab = (unsigned char *) (addr + shdr->sh_offset);

	for (i = 0; i < ehdr->e_shnum; ++i)
	{
		shdr = (Elf32_Shdr *) (addr + ehdr->e_shoff +
				(i * sizeof (Elf32_Shdr)));

		if (!(shdr->sh_flags & SHF_ALLOC) || shdr->sh_size == 0)
			continue;

		if (strtab) {
			printf("0x%08x 0x%08x, %sing %s...",
				(int) shdr->sh_addr,
				(int) shdr->sh_size,
				(shdr->sh_type == SHT_NOBITS) ?
					"Clear" : "Load",
				&strtab[shdr->sh_name]);
		}

		void *target = (void*)(((unsigned long)0x8000000000000000UL) | shdr->sh_addr);

		if (shdr->sh_type == SHT_NOBITS) {
			memset (target, 0, shdr->sh_size);
		} else {
			memcpy ((void *) target, 
				(unsigned char *) addr + shdr->sh_offset,
				shdr->sh_size);
		}
		flush_code (target, shdr->sh_size);
		puts("done");
	}
	
	return ehdr->e_entry;
}

extern void jump(unsigned long dtc, unsigned long kernel_base, unsigned long null, unsigned long reladdr, unsigned long hrmor);
extern void boot_tftp(const char *server, const char *file);

extern char bss_start[], bss_end[], dt_blob_start[];

volatile unsigned long secondary_hold_addr = 1;

void enet_quiesce(void);

void execute_elf_at(void *addr)
{
	printf(" * Loading ELF file...\n");
	void *entry = (void*)load_elf_image(addr);
	
	printf(" * Stop ethernet...\n");
	enet_quiesce();
	printf(" * GO (entrypoint: %p)\n", entry);
	printf(" * Please wait a moment while the kernel loads...\n");

	secondary_hold_addr = ((long)entry) | 0x8000000000000060UL;
	
	jump(((long)dt_blob_start)&0x7fffffffffffffffULL, (long)entry, 0, (long)entry, 0);
}

volatile int processors_online[6] = {1};

int get_online_processors(void)
{
	int i;
	int res = 0;
	for (i=0; i<6; ++i)
		if (processors_online[i])
			res |= 1<<i;
	return res;
}

void place_jump(void *addr, void *_target)
{
	unsigned long target = (unsigned long)_target;
	dcache_flush(addr - 0x80, 0x100);
	*(volatile uint32_t*)(addr - 0x18 + 0) = 0x3c600000 | ((target >> 48) & 0xFFFF);
	*(volatile uint32_t*)(addr - 0x18 + 4) = 0x786307c6;
	*(volatile uint32_t*)(addr - 0x18 + 8) = 0x64630000 | ((target >> 16) & 0xFFFF);
	*(volatile uint32_t*)(addr - 0x18 + 0xc) = 0x60630000 | (target & 0xFFFF);
	*(volatile uint32_t*)(addr - 0x18 + 0x10) = 0x7c6803a6;
	*(volatile uint32_t*)(addr - 0x18 + 0x14) = 0x4e800021;
	flush_code(addr-0x18, 0x18);
	*(volatile uint32_t*)(addr + 0) = 0x4bffffe8;
	flush_code(addr, 0x80);
}

extern char __start_other[], __exception[];

#define HRMOR (0x10000000000ULL)

#define LOADER_RAW         0x8000000004000000ULL
#define LOADER_MAXSIZE     0x1000000

void syscall();
void fix_hrmor();

int start(int pir, unsigned long hrmor, unsigned long pvr, void *r31)
{
	secondary_hold_addr = 0;

#ifndef TARGET_xell
	int exc[]={0x100, 0x200, 0x300, 0x380, 0x400, 0x480, 0x500, 0x600, 0x700, 0x800, 0x900, 0x980, 0xC00, 0xD00, 0xF00, 0xF20, 0x1300, 0x1600, 0x1700, 0x1800};
#endif

	int i;

	/* initialize BSS first. DO NOT INSERT CODE BEFORE THIS! */
	unsigned char *p = (unsigned char*)bss_start;
	memset(p, 0, bss_end - bss_start);

	printf("\nXeLL - Xenon linux loader " LONGVERSION "\n");

#ifdef TARGET_xell
	printf(" * WARNING: Bootstrapped XeLL not catching CPUs...\n");
#else
	printf(" * Attempting to catch all CPUs...\n");

#if 1
	for (i=0; i<sizeof(exc)/sizeof(*exc); ++i)
		place_jump((void*)HRMOR + exc[i], __start_other);
#endif

	place_jump((void*)0x8000000000000700, __start_other);
	
	while (get_online_processors() != 0x3f)
	{
		printf("CPUs online: %02x..\n", get_online_processors()); mdelay(10);
//		if ((get_online_processors() & 0x15) == 0x15)
		{
//			for (i=0; i<sizeof(exc)/sizeof(*exc); ++i)
//				place_jump((void*)0x8000000000000000ULL + exc[i], __start_other);
			
			for (i=1; i<6; ++i)
			{
				*(volatile uint64_t*)(0x8000020000050070ULL + i * 0x1000) = 0x7c;
				*(volatile uint64_t*)(0x8000020000050068ULL + i * 0x1000) = 0;
				(void)*(volatile uint64_t*)(0x8000020000050008ULL + i * 0x1000);
				while (*(volatile uint64_t*)(0x8000020000050050ULL + i * 0x1000) != 0x7C);
			}
			
			*(uint64_t*)(0x8000020000052010ULL) = 0x3e0078;
		}
	}
	
	printf("CPUs online: %02x..\n", get_online_processors());
	printf(" * success.\n");
	
	xenon_smc_start_bootanim();
	
	fix_hrmor();
#endif

			/* re-reset interrupt controllers. especially, remove their pending IPI IRQs. */
	for (i=1; i<6; ++i)
	{
		*(uint64_t*)(0x8000020000050068ULL + i * 0x1000) = 0x74;
		while (*(volatile uint64_t*)(0x8000020000050050ULL + i * 0x1000) != 0x7C);
	}

	printf(" * xenos init\n");
	xenos_init();

		/* remove any characters left from bootup */
	printf(" * remove input\n");
	while (kbhit())
		getchar();
		
	printf(" * network init\n");
	network_init();

		/* display some cpu info */
	printf(" * CPU PVR: %08lx\n", mfspr(287));

#if 1
	printf(" * FUSES - write them down and keep them safe:\n");
	for (i=0; i<12; ++i)
		printf("fuseset %02d: %016lx\n", i, *(unsigned long*)(0x8000020000020000 + (i * 0x200)));
#endif

	if (get_online_processors() != 0x3f)
		printf("WARNING: not all processors could be woken up.\n");

	kmem_init();
	usb_init();
	printf(" * Waiting for USB...\n");
	
	tb_t s, e;
	mftb(&s);
	struct bdev *f;
	do {
		usb_do_poll();
		network_poll();
		f = bdev_open("uda");
		if (f)
			break;
		mftb(&e);
	} while (tb_diff_sec(&e, &s) < 5);

	if (f)
	{
		extern u32 fat_file_size;
		
		if (fat_init(f))
			printf(" * FAT init failed\n");
		else if (fat_open("/xenon.elf"))
			printf("fat open of /xenon.elf failed\n");
		else
		{
			printf(" * fat open okay, loading file...\n");
			int r = fat_read(LOADER_RAW, LOADER_MAXSIZE);
			printf(" * executing...\n");
			execute_elf_at((void*)LOADER_RAW);
		}
		
#if 1
		if (!fat_open("/updxell.bin"))
		{
			printf(" * found XeLL update. press power NOW if you don't want to update.\n");
			delay(15);
			fat_read(LOADER_RAW, LOADER_MAXSIZE);
			printf(" * flashing @1MB...\n");

			sfcx_writereg(0, sfcx_readreg(0) &~ (4|8|0x3c0));
			if (sfcx_readreg(0) != 0x01198010)
			{
				printf(" * unknown flash config %08x, refuse to flash.\n", sfcx_readreg(0));
				goto fail;
			}
			
			unsigned char hdr[0x210];
			readsector(hdr, 0, 0);
			if (memcmp(hdr + 0x10, "zeropair image, version=00, ", 0x1c))
			{
				printf(" * unknown hackimage version.\n");
				printf("%s\n", hdr + 0x20);
				goto fail;
			}
			
			const unsigned char elfhdr[] = {0x7f, 'E', 'L', 'F'};
			if (!memcmp((void*)LOADER_RAW, elfhdr, 4))
			{
				printf(" * really, we don't need an elf.\n");
				goto fail;
			}

			int eraseblock_size = 16384; // FIXME for largeblock

			int addr;
#define OFFSET 1024*1024
			
			for (addr = 0; addr < 0x40000; addr += eraseblock_size)
			{
				int flash_addr = addr + OFFSET;
				unsigned char block[0x210];
				printf("%08x\r", flash_addr);

				readsector(block, flash_addr, 0);

				u32 phys_pos = sfcx_readreg(6); /* physical addr */

				if (!(phys_pos & 0x04000000)) /* shouldn't happen, unless the existing image is broken. just assume the sector is okay. */
				{
					printf(" * Uh, oh, don't know. Reading at %08x failed.\n", i);
					phys_pos = flash_addr;
				}
				phys_pos &= 0x3fffe00;
		
				if (phys_pos != flash_addr)
					printf(" * relocating sector %08x to %08x...\n", flash_addr, phys_pos);
				
				flash_erase(phys_pos);
				int j;
				for (j = 0; j < (eraseblock_size / 0x200); ++j)
				{
					memset(block, 0xff, 0x200);
					if (fat_file_size > addr + j * 0x200)
						memcpy(block, (void*)LOADER_RAW + addr + j * 0x200, 0x200);
					memset(block + 0x200, 0, 0x10);

					*(int*)(block + 0x200) = bswap_32(phys_pos / eraseblock_size);
					block[0x205] = 0xFF;
					calcecc(block);
					write_page(phys_pos + j * 0x200, block);
					
					readsector(block, phys_pos + j * 0x200, 0);
				}
			}
			printf(" * update done\n");

fail:
			while (1);
		}
#endif
	}
	
	printf(" * try booting tftp\n");
	boot_tftp(network_boot_server_name(), network_boot_file_name());
	printf(" * try booting from CDROM\n");
	try_boot_cdrom("vmlinux");
	printf(" * HTTP listen\n");
	print_network_config();
	while (1) network_poll();

	return 0;
}
