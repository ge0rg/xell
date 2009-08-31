#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "fat.h"

#define FALSE 0
#define TRUE 1
#define BUFSIZE	2000

typedef struct {
	int len;
	char buf[BUFSIZE];
} TEL_TXST;

static TEL_TXST tel_st;
static int close_session;
static struct tcp_pcb *tel_pcb;

//static void flush_queue(void);
static void telnet_send(struct tcp_pcb *pcb, TEL_TXST *st);
static err_t telnet_accept(void *arg, struct tcp_pcb *pcb, err_t err);
static err_t telnet_sent(void *arg, struct tcp_pcb *pcb, u16_t len);

void tel_init(void)
{	
err_t err;


	tel_pcb = tcp_new();									/* new tcp pcb */

	if(tel_pcb != NULL)
	{
		close_session = FALSE;								/* reset session */
		err = tcp_bind(tel_pcb, IP_ADDR_ANY, 23);		/* bind to port */

		if(err == ERR_OK)
		{
			tel_pcb = tcp_listen(tel_pcb);					/* set listerning */
			tcp_accept(tel_pcb, telnet_accept);				/* register callback */
		}
		else
		{

		}
	}
	else
	{

	}

	printf(" * Telnet server ok\n");
}

static void telnet_close(struct tcp_pcb *pcb)
{

	close_session = FALSE;
	tcp_arg(pcb, NULL);									/* clear arg/callbacks */
	tcp_err(pcb, NULL);
	tcp_sent(pcb, NULL);
	tcp_recv(pcb, NULL);
	tcp_close(pcb);
}

void tel_close(void)
{
	close_session = TRUE;								/* flag close session */
}

void tel_tx_str( char *buf, int bc)
{
	TEL_TXST *st;


	st = &tel_st;									
	memcpy(st->buf + st->len, buf, strlen(buf));					/* copy data */
	st->len += strlen(buf);							/* inc length */
	//printf("tel -> tel_tx_str - TxPut (%i)\n", st->len);
}

static check_tx_que(void)
{
int tx_bc;
TEL_TXST *st;

	
	st = &tel_st;

	if(st->len > 0)
	{
		//printf("tel -> check_tx_que - st->len (%i)\n", st->len);
		tcp_sent(tel_pcb, telnet_sent);				/* register callback */
		telnet_send(tel_pcb, st);				/* send telnet data */
	}
}

static void telnet_send(struct tcp_pcb *pcb, TEL_TXST *st)
{
err_t err;
u16_t len;


	if(tcp_sndbuf(pcb) < st->len)
	{
		len = tcp_sndbuf(pcb);								
	}
	else
	{ 
		len = st->len;
	}

	do 
	{
		err = tcp_write(pcb, st->buf, len, 0);		

		//printf("tel -> telnet_send - tcp_write (%i)\n", len);

		if(err == ERR_MEM)
		{ 
			len /= 2;
		}
	} 
	while(err == ERR_MEM && len > 1);  
  
	if(err == ERR_OK)
	{ 
		st->len -= len;

		//printf("tel -> telnet_send - tcp_write (%i)\n", st->len);
	}
}

static err_t telnet_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
TEL_TXST *st;

	
	st = arg;
 
	if(st->len > 0)			
	{ 
		//printf ("tel -> telnet_sent - telnet_send (%i)\n", st->len);

		telnet_send(pcb, st);
	}

	return ERR_OK;
}

char * strstr(const char * s1,const char * s2)
{
	int l1, l2;

	l2 = strlen(s2);
	if (!l2)
		return (char *) s1;
	l1 = strlen(s1);
	while (l1 >= l2) {
		l1--;
		if (!memcmp(s1,s2,l2))
			return (char *) s1;
		s1++;
	}
	return NULL;
}


int path_now = 0;

uint32_t cluster[512];
DIR *root_dir = NULL;	

void _get_dir( )
{
	if ( root_dir != NULL )
		free_dir( root_dir );

	root_dir = get_dir( cluster[path_now] );
}

void ls_cmd( )
{
	int i;

	for ( i = 0; i < root_dir->count; i++ )
	{
		if ( root_dir->is_dir[i] )
			tel_tx_str("\x1B[36m", 0 );
		else
			tel_tx_str("\x1b[0m", 0 );

		tel_tx_str( root_dir->name[i], 0  );
		tel_tx_str("\n", 1);
	}
	tel_tx_str("\x1b[0m", 0 );
}

#define LOADER_RAW         0x8000000004000000ULL
#define LOADER_MAXSIZE     0x1000000

void ld_cmd(char *exec )
{
	int i;

	char exec_ask[512];
	memset( exec_ask, 0, 512) ;
	
	for ( i = 5; i < strlen( exec ) - 2 ; i++ )
	{
		exec_ask[i-5] = exec[i];
	}

	int len = strlen ( exec_ask );

	printf("fat open : %s (len=%i)\n", exec_ask, len );

	if ( fat_open_cluster( exec_ask, cluster[path_now] ) )
		printf( "fat open of %s failed\n", exec_ask );
	else
	{
		close_session = TRUE;
		telnet_close(tel_pcb);

		printf(" * fat open okay, loading file...\n");
		int r = fat_read(LOADER_RAW, LOADER_MAXSIZE);
		printf(" * executing...\n");
		execute_elf_at((void*)LOADER_RAW);
	}
}

void cd_cmd( char *folder )
{
	int i;

	char folder_ask[512];
	memset(folder_ask, 0, 512);
	
	for ( i = 3; i < strlen( folder ) ; i++ )
	{
		folder_ask[i-3] = folder[i];
	}

	for ( i = 0; i < root_dir->count; i++ )
	{
		if ( strncmp ( folder_ask, "/", 1 )   == 0 )
		{
			path_now = 0;
			_get_dir();
			//printf("reseting cluster\n" );
			break;
		}

		if ( strncmp ( folder_ask, "..", 2 )  == 0 )
		{
			if ( path_now > 0 )
			{
				path_now--;
				_get_dir();
				//printf("cluster = %i\n", root_dir->cluster[i] );
			}
			break;
		}

		if ( ! root_dir->is_dir[i] )
			continue;

		//printf("compare %s|%s\n", folder_ask, root_dir->name[i] );
		if ( strncmp ( folder_ask, root_dir->name[i], strlen( root_dir->name[i] ) ) == 0 )
		{
			path_now++;
			cluster[path_now] = root_dir->cluster[i];
			_get_dir();
			//printf("cluster = %i\n", cluster[path_now] );
			break;
		}
	}
}

void tftp_cmd()
{
	boot_tftp(network_boot_server_name(), network_boot_file_name());
}

void help_cmd()
{
	tel_tx_str("\n", 0);
	tel_tx_str("print: print a text on screen\n", 0);	
	tel_tx_str("ls: list current directory\n", 0);
	tel_tx_str("cd: enter directory\n", 0);	
	tel_tx_str("exec: execute an elf32 binary\n", 0);
	tel_tx_str("tftp: try tftp boot\n", 0);
	tel_tx_str("help: this message\n", 0);
	tel_tx_str("\n", 0);
	
}


void telnet_check_cmd( struct pbuf *p )
{
	char msg[512];
	memset(msg, 0, 512);

	strncpy(msg, p->payload, p->tot_len );
	//printf("> %s", msg ); //p->tot_len,

	if ( strncmp ( msg, "exit", 4 ) == 0 )
	{
		tel_close();
			
	}
	else if ( strncmp ( msg, "print", 5 ) == 0 ) 
	{
		printf("%s", msg );
			
	}
	else if ( strncmp ( msg, "ls", 2 ) == 0 )
	{
		ls_cmd();		
	}
	else if ( strncmp ( msg, "cd", 2 ) == 0 )
	{
		cd_cmd(msg);		
	}
	else if ( strncmp ( msg, "exec", 4 )  == 0 )
	{
		ld_cmd( msg );
	}
	else if ( strncmp ( msg, "tftp", 4 )  == 0 )
	{
		tftp_cmd( msg );
	}
	else if ( strncmp ( msg, "help", 4 )  == 0 )
	{
		help_cmd();
	}
	else
	{
		tel_tx_str("> Unknow command", 16);
		tel_tx_str("\n", 1);
	}
	tel_tx_str("> ", 2);
}

static err_t telnet_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{

	if (err == ERR_OK && p != NULL)
	{
		tcp_recved(pcb, p->tot_len);	/* some recieved data */
		telnet_check_cmd(p);
	}

	pbuf_free(p);				/* dealloc mem */
	if (err == ERR_OK && p == NULL)
	{
		telnet_close(pcb);
	}

	return ERR_OK;
}

static void telnet_err(void *arg, err_t err)
{

	printf ("tel -> telnet_err - error (0x%.4x)\n", err);
}

static err_t telnet_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
int i;


//	vt1_init();					/* init vt100 interface */

	tel_pcb = pcb;
	tel_st.len = NULL;				/* reset length */
	close_session = FALSE;				/* reset session */
	tcp_arg(pcb, &tel_st);				/* argument passed to callbacks */
	tcp_err(pcb, telnet_err);			/* register error callback */
	tcp_recv(pcb, telnet_recv);			/* register recv callback */	

	tel_tx_str("\x1b[2J", 4);			/* clear screen */		
	tel_tx_str("\x1b[0;0H", 6);			/* set cursor position */
	tel_tx_str("\x1b[0m", 0 );
	tel_tx_str("Xell telnet shell by Cpasjuste\n", 30); /* display message */
	tel_tx_str("Type help for ... help\n", 30); /* display message */
	tel_tx_str("> ", 2);

	path_now = 0;
	cluster[path_now] = 0;
	_get_dir( );


	//printf ("tel -> telnet_accept\n", err);

	return ERR_OK;
}

void tel_process(void)
{	

	if(close_session == TRUE)
	{
		telnet_close(tel_pcb);			/* close telnet session */
	}
	else
	{
		check_tx_que();					/* any data to be sent */
	}
}

