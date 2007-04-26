CROSS=powerpc64-linux-
CC=$(CROSS)gcc
OBJCOPY=$(CROSS)objcopy
LD=$(CROSS)ld
AS=$(CROSS)as
STRIP=$(CROSS)strip

# Configuration
CFLAGS = -Wall -nostdinc -O2 -I. -Ilwip/include \
	-Iinclude -I./lwip/include/ipv4 -Ilwip/arch/xenon/include \
	-m64 -mno-toc -DBYTE_ORDER=BIG_ENDIAN
	
AFLAGS = -Iinclude -m64
LDFLAGS = -nostdlib -n

#-DLWIP_DEBUG -O2 
#-Werror

OBJS = crt0.o main_ardl_.o string_asm.o string.o ctype.o video.o console.o exi.o \

LWIP_OBJS = ./lwip/core/tcp_in.o \
	./lwip/core/inet.o ./lwip/core/mem.o ./lwip/core/memp.o \
	./lwip/core/netif.o ./lwip/core/pbuf.o ./lwip/core/stats.o ./lwip/core/sys.o \
	./lwip/core/tcp.o  ./lwip/core/ipv4/ip_addr.o ./lwip/core/ipv4/icmp.o \
	./lwip/core/ipv4/ip.o ./lwip/core/ipv4/ip_frag.o  \
	./lwip/core/tcp_out.o \
	./lwip/core/udp.o ./lwip/netif/etharp.o ./lwip/netif/loopif.o ./lwip/core/dhcp.o \
	./lwip/core/raw.o \
	./lwip/arch/xenon/lib.o  ./lwip/arch/xenon/netif/enet.o

OBJS = startup2.o main.o string.o vsprintf.o ctype.o time.o  \
	cache.o  $(LWIP_OBJS)  network.o tftp.o httpd/httpd.o httpd/vfs.o dtc.o cdrom.o

# Build rules
all: rom.bin

clean:
	rm -rf $(OBJS) rom.elf rom.elf32 rom.bin

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $*.c

.S.o:
	$(CC) $(AFLAGS) -c -o $@ $*.S

%.elf: %.lds $(OBJS)
	$(CC) -n -T $< -nostdlib -m64 -o $@ $(OBJS)

%.elf32: %.elf
	$(OBJCOPY) -O elf32-powerpc $< $@
	$(STRIP) -s $@

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@
	echo -n "xxxxxxxxxxxxxxxx" >> $@
