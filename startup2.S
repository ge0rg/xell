#define rmor 312
#define hrmor 313
#define hid1 1009
#define hid4 1012
#define hid6 1017
#define lpcr 318
#define pir 1023
#define tsrr 897
#define tscr 921
#define ctrlb 152
.globl _start
.extern start
.globl __start_other
_start:
	b start_from_rom

__start_other:
start_from_rom:
	or %r2, %r2, %r2 // normal priority

		// disable interrupts
	li %r13, 0
	mtmsrd %r13, 1


	li %r3, 2
	isync	
	mtspr lpcr, %r2
	isync
	li      %r3, 0x3FF
	rldicr  %r3, %r3, 32,31
	.long 0x7C201A24
	sync
	isync
	

	mfspr %r10, hid1
	li %r11, 3
	rldimi %r10, %r11, 58,4     // enable icache
	rldimi %r10, %r11, 38,25    // instr. prefetch
	sync
	mtspr hid1, %r10
	sync
	isync

	mfspr %r10, lpcr
	li %r11, 1
	rldimi %r10, %r11, 1,62
	isync
	mtspr lpcr, %r10
	isync

		// set stack
	li %sp, 0
	oris %sp, %sp, 0x8000
	rldicr %sp, %sp, 32,31
	oris %sp, %sp, 0x138

	mfspr %r3, pir
	slwi %r4, %r3, 16  // 64k stack
	sub %sp, %sp, %r4
	subi %sp, %sp, 0x80

	cmpwi %r3, 0
	bne hangon

	lis %r3, start@h
	ori %r3, %r3, start@l
	ld %r2, 8(%r3)
	ld %r3, 0(%r3)

	mfspr %r3, pir
	mfpvr %r5
	mr %r4, %r15
	mr %r6, %r27
	bl start

1:
	b 1b

putc:
	lis %r4, 0x8000
	ori %r4, %r4, 0x200
	rldicr  %r4, %r4, 32,31
  oris  %r4, %r4, 0xea00

  slwi %r3, %r3, 24
  stw %r3, 0x1014(%r4)
1:
  lwz %r3, 0x1018(%r4)
  rlwinm. %r3, %r3, 0, 6, 6
  beq 1b

  blr

	// r6 = addr, r7 = hrmor
.globl jump
jump:
	mtspr rmor, %r7
	mtspr hrmor, %r7
	mtsrr0 %r6

		/* switch into real mode */
	mfmsr %r6
	li %r7, 0x30
	andc %r6, %r6, %r7
	mtsrr1 %r6
	rfid

hangon:
	lis %r4, 0x8000
	rldicr %r4, %r4, 32,31
	oris %r4, %r4, processors_online@h
	ori %r4, %r4, processors_online@l
	slwi %r3, %r3, 2
	add %r4, %r4, %r3
	li %r5, 1
	stw %r5, 0(%r4)

	lis %r3, 0x8000
	rldicr %r3, %r3, 32,31
	oris %r3, %r3, secondary_hold_addr@h
	ori %r3, %r3, secondary_hold_addr@l

.globl Idle
Idle:
1:
	or %r1, %r1, %r1       /* low priority */
	ld %r4, 0(%r3)
	cmpwi %r4, 0
	beq 1b

	li %r3, 0
	mtspr hrmor, %r3
	mtspr rmor, %r3

	mtctr %r4
	
	mfspr %r3, pir

	cmpwi %r3, 1
	beq stop
	cmpwi %r3, 3
	beq stop
	cmpwi %r3, 5
	beq stop

	bctr

stop:
	li %r3, 0
1:
	mtspr ctrlb, %r3
	b 1b 

