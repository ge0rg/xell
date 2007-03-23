// quick&dirty fixed sector reader (c) Felix Domke <tmbinc@elitedvb.net>
.globl .do_readsector
.do_readsector:
.globl _start
_start:
nop
bl 1f
.long 0x80000200
.long 0xea000000

.long 0x80000000 // code location:
.long 0x01310000 // 0x1310000

.long 0x28000000 // READ(10)
.long 0x00200000 // LBA = 0x20 (first file, usually)
.long 0x40000000  // 0x40 sectors == 128k


1:
mflr %r3             // get address of constants
ld  %r4, 0(%r3)      // base address for PCI
ld  %r7, 8(%r3)      // target in memory
lwz %r5, 0x10(%r3)   // CDB
lwz %r9, 0x14(%r3)   // CDB
lwz %r10, 0x18(%r3)  // CDB
li %r6, 0

	/* open tray using SMC command */
lis %r3, 0x4000
stw %r3, 0x1084(%r4)  /* start command */
lis %r3, 0x8b60       /* smc command 8b60 -> open tray */
stw %r3, 0x1080(%r4)
stw %r6, 0x1080(%r4)
stw %r6, 0x1080(%r4)
stw %r6, 0x1080(%r4)
stw %r6, 0x1084(%r4)  /* end command */

retry:

		/* wait a bit */
lis %r3, 0x1000
mtctr %r3
1:
bdnz 1b

		/* try reading */
li %r3, 0x7fff
mtctr %r3

//li %r5, 2048
//stb %r3, 0x1206(%r4) //  drive select (A0)
//bl waitready

//stb %r5, 0x1204(%r4) // byte_cnt_l
//stb %r6, 0x1205(%r4) // byte_cnt_h
li %r3, 0xA0

stb %r6, 0x1201(%r4)  // atapi_features
stb %r3, 0x1207(%r4)  // atapi command
bl waitready
stw %r5, 0x1200(%r4)  // write cdb
stw %r9, 0x1200(%r4)
stw %r10, 0x1200(%r4)
bl waitready

mr %r8, %r7

1:
bl waitdrq
lwz %r3, 0x1200(%r4)
stw %r3, 0(%r8)
dcbst %r0, %r8
sync
icbi %r0, %r8
addi %r8, %r8, 4
bdnz 1b

lwz %r3, 0x1200(%r4)
lwz %r3, 0x1200(%r4)


	li %r3, 'O'
	bl putc
	li %r3, 'K'
	bl putc
	li %r3, '\r'
	bl putc
	li %r3, '\n'
	bl putc

	li %r2, 0
	mtspr 313, %r2 // hrmor
	li %r3, 2
	mtspr 318, %r3 // lpcr


mtctr %r7
bctr

waitdrq:
lbz %r3, 0x1207(%r4)
rlwinm. %r3, %r3, 0, 28,28
beq waitdrq
blr

waitready:
lbz %r3, 0x1207(%r4)
rlwinm %r3, %r3, 0, 24, 25
cmpwi %r3, 0x40
bne waitready
lbz %r3, 0x1207(%r4)
rlwinm. %r3, %r3, 0, 31, 31
bne request_sense
blr

putc:
  slwi %r3, %r3, 24
  stw %r3, 0x1014(%r4)
1:
  lwz %r3, 0x1018(%r4)
  rlwinm. %r3, %r3, 0, 6, 6
  beq 1b
  blr

request_sense:

li %r3, 'R'
bl putc
li %r3, 'S'
bl putc
li %r3, ':'
bl putc

	// request sense
li %r3, 0xA0
stb %r6, 0x1201(%r4)  // atapi_features
stb %r3, 0x1207(%r4)  // atapi command
bl waitready
lis %r3, 0x0300
stw %r3, 0x1200(%r4)  // write mode sense cdb
stw %r6, 0x1200(%r4)
stw %r6, 0x1200(%r4)
bl waitready

li %r3, '\r'
bl putc
li %r3, '\n'
bl putc

b retry
