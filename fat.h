#ifndef __fat_h
#define __fat_h

typedef struct
{
	int count;
	uint32_t cluster[512];
	int is_dir[512];
	char name[8][512];
	
} DIR;

void free_dir( DIR *dir );

#endif

