#include <vsprintf.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define LOADER_RAW         0x8000000004000000ULL
#define LOADER_MAXSIZE     0x1000000

#define ATAPI_ERROR 1
#define ATAPI_DATA 0
#define ATAPI_DRIVE_SELECT 6
#define ATAPI_BYTE_CNT_L 4
#define ATAPI_BYTE_CNT_H 5
#define ATAPI_FEATURES 1
#define ATAPI_COMMAND 7
#define ATAPI_STATUS 7

#define ATAPI_STATUS_DRQ 8
#define ATAPI_STATUS_ERR 1

extern char getchar(void);

void ide_out8(int reg, unsigned char d)
{
	*(volatile unsigned char*)(0x80000200ea001200ULL + reg) = d;
}

void ide_out16(int reg, unsigned short d)
{
	*(volatile unsigned short*)(0x80000200ea001200ULL + reg) = d;
}

void ide_out32(int reg, unsigned int d)
{
	*(volatile unsigned int*)(0x80000200ea001200ULL + reg) = d;
}

unsigned char ide_in8(int reg)
{
	return *(volatile unsigned char*)(0x80000200ea001200ULL + reg);
}

unsigned int ide_in32(int reg)
{
	return *(volatile unsigned int*)(0x80000200ea001200ULL + reg);
}

int ide_waitready()
{
	while (1)
	{
		int status=ide_in8(ATAPI_STATUS);
		if ((status&0xC0)==0x40) // RDY and /BUSY
			break;
		if (status == 0xFF)
			return -1;
	}
	return 0;
}

int atapi_packet(unsigned char *packet, unsigned char *data, int data_len)
{
	ide_out8(ATAPI_DRIVE_SELECT, 0xA0);

	data_len /= 2;

	ide_out8(ATAPI_BYTE_CNT_L, data_len & 0xFF);
	ide_out8(ATAPI_BYTE_CNT_H, data_len >> 8);

	ide_out8(ATAPI_FEATURES, 0);
	ide_out8(ATAPI_COMMAND, 0xA0);
	
	if (ide_waitready())
	{
		printf(" ! waitready after command failed!\n");
		return -2;
	}
	
	if (ide_in8(ATAPI_STATUS) & ATAPI_STATUS_DRQ)
	{
		ide_out32(ATAPI_DATA, *(unsigned int*)(packet + 0));
		ide_out32(ATAPI_DATA, *(unsigned int*)(packet + 4));
		ide_out32(ATAPI_DATA, *(unsigned int*)(packet + 8));
	} else if (ide_in8(ATAPI_STATUS) & ATAPI_STATUS_ERR)
	{
		printf(" ! error before sending cdb\n");
		return -3; 
	} else
	{
		printf(" ! DRQ not set\n");
		return -4;
	}
	
	if (ide_waitready())
	{
		printf(" ! waitready after CDB failed\n");
		return -5;
	}
	
	while (data_len)
	{
		unsigned char x = ide_in8(ATAPI_STATUS);
		if (x & 0x80) // busy
			continue;

		if (x & 1) // err
		{
			printf(" ! ERROR after CDB [%02x], err=%02x.\n", data[0], ide_in8(ATAPI_ERROR));
			break;
		}

		*(unsigned int*)data = ide_in32(ATAPI_DATA);
		data_len -= 2;
		data += 4;
	}
	
	if (ide_in8(ATAPI_STATUS) & ATAPI_STATUS_ERR)
	{
		printf(" ! error after reading response\n");
		return -6;
	}
	
	return 0;
}

int request_sense(void)
{
	unsigned char request_sense[12] = {0x03, 0, 0, 0, 24, 0, 0, 0, 0, 0, 0, 0};
	unsigned char sense[24];
	if (atapi_packet(request_sense, sense, 24))
	{
		printf(" ! REQUEST SENSE failed\n");
		return -1;
	}
	return (sense[2] << 16) | (sense[12] << 8) | sense[13];
}

int read_sector(unsigned char *sector, int lba, int num)
{
	int maxretries = 10;
retry:
	if (!maxretries--)
	{
		printf(" ! sorry.\n");
		return -1;
	}
	unsigned char read[12] = {0x28, 0, lba >> 24, lba >> 16, lba >> 8, lba, 0, num >> 8, num, 0, 0, 0};
	if (atapi_packet(read, sector, 0x800 * num))
	{
		printf(" ! read sector failed!\n");
		int sense = request_sense();

		printf(" ! sense: %06x\n", sense);
		
		if ((sense >> 16) == 6)
		{
			printf(" * UNIT ATTENTION -> just retry\n");
			goto retry;
		}
		
		if (sense == 0x020401)
		{
			printf(" ! not ready -> retry\n");
			delay(1);
			goto retry;
		}
		
		return -1;
	}
	return 0;
}

void set_modeb(void)
{
	unsigned char res[2048];
	
	unsigned char modeb[12] = {0xE7, 0x48, 0x49, 0x54, 0x30, 0x90, 0x90, 0xd0, 0x01};
	printf("atapi_packet result: %d\n", atapi_packet(modeb, res, 0x30));
}

struct pvd_s
{
	char id[8];
	char system_id[32];
	char volume_id[32];
	char zero[8];
	unsigned int total_sector_le, total_sect_be;
	char zero2[32];
	unsigned int volume_set_size, volume_seq_nr;
	unsigned short sector_size_le, sector_size_be;
	unsigned int path_table_len_le, path_table_len_be;
	unsigned int path_table_le, path_table_2nd_le;
	unsigned int path_table_be, path_table_2nd_be;
	unsigned char root_direntry[34];
	// ...
}  __attribute__((packed));

static unsigned char sector_buffer[0x800];

	/* really simple. */
int memicmp(void *a, void *b, int len)
{
	unsigned char *_a = a, *_b = b;
	while (len--)
		if (toupper(*_a++) != toupper(*_b++))
			return 1;
	return 0;
}

int read_direntry(unsigned char *direntry, char *filename, int *lba, int *size)
{
	int nrb = *direntry++;
	++direntry;
	
	direntry += 4;
	
	*lba  = (*direntry++) << 24;
	*lba |= (*direntry++) << 16;
	*lba |= (*direntry++) << 8;
	*lba |= (*direntry++);	
	
	direntry +=4;
	*size  = (*direntry++) << 24;
	*size |= (*direntry++) << 16;
	*size |= (*direntry++) << 8;
	*size |= (*direntry++);
	
	direntry += 7; // skip date
	
	int flags = *direntry++;
	++direntry; ++direntry; direntry+=4;
	
	int nl = *direntry++;

	int fnl = strlen(filename);
	if (!fnl)
		fnl++;
	
	/* printf("compare <%s> (%d) with <%s> (%d) [%02x %02x] vs [%02x %02x]\n", filename, fnl, direntry, nl, direntry[0], direntry[1], filename[0], filename[1]); */
	
	if ((nl != fnl) || (memicmp(direntry, filename, nl)))
	{
		printf("no\n");
		*lba = 0;
		*size = 0;
	}
	
	return nrb;
}

int read_directory(int sector, int len, char *filename, int *lba, int *size)
{
	if (read_sector(sector_buffer, sector, 1))
	{
		printf("read_directory: read sector failed\n");
		return -1;
	}
	
	int ptr = 0;
	
	for (ptr =0 ; ptr < 0x40; ++ptr)
	{
		printf("%02x ", sector_buffer[ptr]);
		if ((ptr&15)==15)
			printf("\n");
	}
	ptr = 0;
	
	while (len > 0)
	{
		printf("At %08x\n", ptr);
		ptr += read_direntry(sector_buffer + ptr, filename, lba, size);
		
		if (*lba || *size)
			return 0;
		
		if (!sector_buffer[ptr])
		{
			len -= 2048;
			read_sector(sector_buffer, ++sector, 1);
			ptr = 0;
		}
	}
	printf("file not found!\n");
	return -2;
}

extern int execute_elf_at(void *dst, int len, const char *args);

int iso9660_load_file(char *filename, void* addr)
{
	int sector, size, file_sector, file_size;
	char fnamebuf[258];
	struct pvd_s *pvd = 0;
	
	printf(" * reading CD/DVD...\n");
	
	if (read_sector(sector_buffer, 16, 1))
	{
		printf(" ! failed to read PVD\n");
		return -1;
	}

	pvd = (void*)sector_buffer;
	if (memcmp(sector_buffer, "\1CD001\1", 8))
	{
		printf(" ! no iso9660!\n");
		return -1;
	}
	
	printf("root_direntry offset: %d\n", (int)(&((struct pvd_s *)0)->root_direntry));
	
	read_direntry(pvd->root_direntry, "", &sector, &size);
	
	if (!sector)
	{
		printf(" ! root direntry not found\n");
		return -1;
	}
	
	printf(" ! root at lba=%02x, size=%d\n", sector, size);

	strcpy(fnamebuf, filename);
	strcat(fnamebuf, ";1");
	if (read_directory(sector, size, fnamebuf, &file_sector, &file_size))
	{
		strcpy(fnamebuf, filename);
		strcat(fnamebuf, ".;1");
		if (read_directory(sector, size, fnamebuf, &file_sector, &file_size))
		{
			printf(" ! '%s' not found\n", filename);
			return -1;
		}
	}
	
	printf(" ! found '%s' at lba=%d, size=%d\n", fnamebuf, file_sector, file_size);
	
	printf(" ! loading file...\n");

	int s = file_size;
	
	while (1)
	{
		printf("\r * %08x -> %p... ", file_sector, addr);
		
		int num = (s + 0x7ff) / 0x800;
		if (num > 64)
			num = 64;
		
		if (read_sector(addr, file_sector, num))
		{
			printf("\n ! read sector failed!\n");
			break;
		}
		
		addr += num * 0x800; file_sector+=num; s -= num * 0x800;
		if (s <= 0)
		{
			printf("\n * done!\n");
			return file_size;
		}
	}
	return -1;
}

void try_boot_cdrom(char *filename)
{
	int size;
	set_modeb();
	
	if (!filename)
		filename = "vmlinux";
	if ((size = iso9660_load_file(filename, (void*)LOADER_RAW)) >= 0) {
		execute_elf_at((void*)LOADER_RAW, size, "");
	}
}
