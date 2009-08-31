#include "time.h"
#include "lwipopts.h"
#include "lwip/debug.h"

#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/sys.h"

#include "lwip/stats.h"

#include "lwip/ip.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "netif/etharp.h"

struct netif netif;
struct ip_addr ipaddr, netmask, gw;
static tb_t now, last_tcp, last_dhcp;

extern void enet_poll(struct netif *netif);
extern err_t enet_init(struct netif *netif);

extern void httpd_start(void);

void network_poll();


void network_init()
{
	printf("trying to initialize network...\n");

#ifdef STATS
	stats_init();
#endif /* STATS */

	mem_init();
	memp_init();
	pbuf_init(); 
	netif_init();
	ip_init();
	udp_init();
	tcp_init();
	etharp_init();
	printf("ok now the NIC\n");
	
	if (!netif_add(&netif, &ipaddr, &netmask, &gw, NULL, enet_init, ip_input))
	{
		printf("netif_add failed!\n");
		return;
	}
	
	netif_set_default(&netif);
	
	dhcp_start(&netif);
	
	mftb(&last_tcp);
	mftb(&last_dhcp);
	
	printf("\nWaiting for DHCP");

	int i;	
	for (i=0; i<10; i++) {
		mdelay(500);
		network_poll();
		
		printf(".");
		
		if (netif.ip_addr.addr)
			break;
	}
	
	if (netif.ip_addr.addr) {
		printf("%u.%u.%u.%u\n",	
			(netif.ip_addr.addr >> 24) & 0xFF,
			(netif.ip_addr.addr >> 16) & 0xFF,
			(netif.ip_addr.addr >>  8) & 0xFF,
			(netif.ip_addr.addr >>  0) & 0xFF);
			
		printf("\n");
	} else {
		printf("failed.\n\n");
		
		IP4_ADDR(&ipaddr, 10, 0, 120, 209);
		IP4_ADDR(&gw, 10, 0, 120, 1);
		IP4_ADDR(&netmask, 255, 255, 255, 0);
		netif_set_addr(&netif, &ipaddr, &netmask, &gw);
		netif_set_up(&netif);
	}
	

	printf("starting httpd server..");
	httpd_start();
	printf("ok!\n");
}

void print_network_config()
{
#define NTOA(ip) (int)((ip.addr>>24)&0xff), (int)((ip.addr>>16)&0xff), (int)((ip.addr>>8)&0xff), (int)(ip.addr&0xff)
	printf(" * XeLL network config: %d.%d.%d.%d / %d.%d.%d.%d\n",
		NTOA(ipaddr), NTOA(netmask));
}

void network_poll()
{
	mftb(&now);
	enet_poll(&netif);

	if (tb_diff_msec(&now, &last_tcp) >= 100)
	{
		mftb(&last_tcp);
		tcp_tmr();
	}
	
	if (tb_diff_msec(&now, &last_dhcp) >= DHCP_FINE_TIMER_MSECS)
	{
		mftb(&last_dhcp);
		dhcp_fine_tmr();
	}
}

char *network_boot_server_name()
{
	if (netif.dhcp && netif.dhcp->boot_server_name)
		return netif.dhcp->boot_server_name;
		
	return "10.0.120.78";
}

char *network_boot_file_name()
{
	if (netif.dhcp && netif.dhcp->boot_file_name)
		return netif.dhcp->boot_file_name;
		
	return "/tftpboot/xenon";
}
