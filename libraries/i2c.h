#ifndef I2C_H_
#define I2C_H_

#include <stdint.h>

#ifndef I2C_FREQ
#define I2C_FREQ  100000UL   /* 100 kHz */
#endif

void    i2c_init(void);
uint8_t i2c_start(uint8_t addr_rw);   /* 0 = ACK, 1 = NACK/error */
uint8_t i2c_write(uint8_t data);      /* 0 = ACK, 1 = NACK      */
uint8_t i2c_read(uint8_t ack);        /* ack=1 -> ACK, 0 -> NACK */
void    i2c_stop(void);

#endif /* I2C_H_ */
