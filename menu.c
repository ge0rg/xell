#include <vsprintf.h>
#include <string.h>
#include <network.h>
#include <lwip/inet.h>
#include <lwip/netif.h>
#include <processor.h>
#include <time.h>
#include <xenon_smc.h>

extern int kbhit(void);
extern int getchar(void);
extern int iso9660_load_file(char *filename, void* addr);
extern void try_boot_cdrom(char *);

/* from network.c: */
extern struct ip_addr ipaddr, netmask, gw;
extern struct netif netif;


/* from fat.c: */
extern int fat_open(const char *name);
extern int fat_read(void *data, u32 len);

extern void try_boot_fat(char *filename);

int ignore_power;
/*----------------------------------------------------------------------*/

#define MAXOPTS 32

enum source {
	SRC_DEFAULT,
	SRC_TFTP,
	SRC_CD,
	SRC_USB,
	SRC_HDD,
	SRC_MAX
};

char *sourcenames[SRC_MAX] = {
	"same source as XeLL.cfg",
	"TFTP",
	"CD-ROM",
	"USB stick",
	"Hard Disk"
};

struct file {
};

struct boot {
	enum source src;
	char *path;
	char *title;
};

void print_bootopt(struct boot *b) {
	printf("Boot config \"%s\"\n", b->title);
	printf("	Kernel: \"%s\" %i\n", b->path, b->src);
}

#define isspace(c) ((c)==' ' || (c)=='\r' || (c)=='\n' || (c)=='\t')

/* skip leading whitespace in string at ptr */
#define SKIP_WS(ptr) while (isspace(*ptr)) ptr++;

/* macro to parse a command and execute according code
 *
 * @ptr: pointer to command line start (will be changed)
 * @opt: command name (i.e. "timeout")
 * @optlen: strlen(opt) (i.e. 7)
 * @code: what to execute when the command is matched
 **/
#define IFOPT(ptr, opt, optlen, code) \
	if ((0 == strncmp(ptr, opt, optlen)) && isspace(ptr[optlen])) { \
		ptr += optlen + 1; \
		SKIP_WS(ptr); \
		code; \
	}

/* global boot config vars:
 * boot_optc: number of boot entries
 * boot_opt: boot menu entries
 * boot_opt[0] = default fallback entries, no real boot menu
 **/
int boot_optc;
struct boot boot_opt[MAXOPTS];

int boot_timeout;
struct in_addr boot_ip, boot_netmask, boot_server;
char *tftp_server;

void parse_source(enum source src, char *data) {
	if (boot_optc == MAXOPTS) {
		printf("Maximum number of menu entries reached!\n");
		return;
	}
	char *tmp = strpbrk(data, " \t");
	if (tmp) {
		*tmp++ = '\0';
		SKIP_WS(tmp);
		boot_opt[boot_optc].src = src;
		boot_opt[boot_optc].path = data;
		boot_opt[boot_optc].title = tmp;
		boot_optc++;
	} else {
		printf("Missing whitespace in source line: \"%s\"\n", data);
	}
}

void parse_input_line(char *data) {
	char *tmp;

	SKIP_WS(data);
	IFOPT(data, "ip", 2, 
		tmp = strchr(data, '/');
		if (tmp) {
			struct ip_addr ipaddr;
			struct ip_addr netmask;
			*tmp = '\0';
			tmp++;
			inet_aton(data, (struct in_addr*)&ipaddr.addr);
			inet_aton(tmp, (struct in_addr*)&netmask.addr);
			netif_set_addr(&netif, &ipaddr, &netmask, &netif.gw);
			print_network_config();
		} else printf("Error: missing netmask");
	) else
	IFOPT(data, "server", 6, 
		inet_aton(data, &boot_server);
		tftp_server = data;
		//printf("TFTP: %08x\n", boot_server.s_addr);
	) else
	IFOPT(data, "timeout", 7, 
		boot_timeout = skip_atoi(&data);
	) else
	IFOPT(data, "tftp", 4,
		parse_source(SRC_TFTP, data);
	) else
	IFOPT(data, "cd", 2,
		parse_source(SRC_CD, data);
	) else
	IFOPT(data, "usb", 3,
		parse_source(SRC_USB, data);
	) else
	IFOPT(data, "hdd", 3,
		parse_source(SRC_HDD, data);
	) else {
		printf("Ignoring unknown command: \"%s\"\n", data); 
	}
}

/* parse a zero terminated config file string */
void parse_config(char *data) {
	char *next, *eol;

	boot_optc = 0;
	boot_timeout = 30;

	while (data && *data) {

		/* find EOL and replace with NUL byte */
		eol = strpbrk(data, "\r\n");
		if (eol == NULL) {
			next = NULL;
		} else {
			*eol = '\0';
			next = eol + 1;
		}
		//printf("%p param %s\n", data, data);

		parse_input_line(data);
		data = next;

		if (data)
			SKIP_WS(data);
	}
}

#define POWER_BTN -3

/** @brief presents a prompt on screen and waits until a choice is taken
 *
 * @param defaultchoice what to return on timeout
 * @param max maximum allowed choice
 * @param timeout number of seconds to wait
 *
 * @return choice (0 <= choice <= max)
 */
int user_prompt(int defaultchoice, int max, int timeout) {
	int redraw = 1;
	int min = 1;
	int delta, olddelta = -1;
	int ignore_power_last = ignore_power;
	tb_t start, now;

	mftb(&start);
	delta = 0;
	while (delta <= timeout) {
		/* measure seconds since menu start */
		mftb(&now);
		delta = tb_diff_sec(&now, &start);
		/* test if prompt update is needed */
		if (delta != olddelta) {
			olddelta = delta;
			redraw = 1;
		}
		/* redraw prompt - clear line, then print prompt */
		if (redraw) {
			printf("                                                           \r[%02d] > ",
				timeout - delta);
			redraw = 0;
		}

		if (0 == xenon_smc_poll()) {
			int ch = xenon_smc_get_ir();
			if (ch >= min && ch <= max)
				return ch;
			redraw = 1;
			/* check for Power button */
			if (ignore_power_last != ignore_power)
				return POWER_BTN;
		}

		if (kbhit()) {
			char ch = getchar();
			int num = ch - '0';
			printf("\nyou pressed '%c'\n", ch);
			if (num >= min && num <= max)
				return num;
			redraw = 1;
		}

		network_poll();
	}
	printf("Timeout.\n");
	return defaultchoice;
}

void main_menu(char *xell_config, int xell_config_size) {
	int defchoice = 1;

	/* ensure null termination */
	xell_config[xell_config_size] = '\0';
	parse_config(xell_config);

	/* Allow power button for menu selection */
	ignore_power = boot_optc - 1;

	while (1) {
		int opt;
		printf("XeLL Main Menu. Please choose from:\n\n");
		for (opt = 1; opt <= boot_optc; opt++) {
			printf("  <%i> %s - %s%s\n", opt, boot_opt[opt-1].title,
				sourcenames[boot_opt[opt-1].src],
				defchoice == opt?" (default)":"");
		}
		int choice = user_prompt(defchoice, boot_optc, boot_timeout);
		if (choice == POWER_BTN) {
			defchoice++;
			boot_timeout = 5;
			continue;
		}

		struct boot *b = &boot_opt[choice-1];
		printf("[###] > Booting %s: %s from %s\n", b->title, b->path, sourcenames[b->src]);
		switch (b->src) {
		case SRC_TFTP:
			boot_tftp(tftp_server, b->path);
			break;
		case SRC_CD:
			try_boot_cdrom(b->path);
			break;
		case SRC_USB:
			try_boot_fat(b->path);
			break;
		default:
			printf("Not yet implemented, aborting menu!\n");
			//usleep(1000);
			return;
		}
	}
}

