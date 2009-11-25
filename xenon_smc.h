#ifndef __XENON_SMC
#define __XENON_SMC

#include <types.h>

void xenon_smc_send_message(const unsigned char *msg);
int xenon_smc_receive_message(unsigned char *msg);
int xenon_smc_poll();
int xenon_smc_get_ir();

int xenon_smc_ana_write(uint8_t addr, uint32_t val);
int xenon_smc_ana_read(uint8_t addr, uint32_t *val);

void xenon_smc_set_led(int override, int value);
void xenon_smc_power_shutdown(void);
void xenon_smc_start_bootanim(void);



#endif
