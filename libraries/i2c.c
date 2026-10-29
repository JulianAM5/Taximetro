#include <avr/io.h>
#include "i2c.h"

/* ============================================================
 *  Frecuencia del bus I2C (100 kHz estándar, 400 kHz fast mode)
 * ============================================================ */
#ifndef I2C_FREQ
#define I2C_FREQ  100000UL
#endif

/* ============================================================
 *  Inicialización del TWI como Master
 *  - Prescaler = 1
 *  - Bit rate = F_CPU / (16 + 2*TWBR*Prescaler)
 *    => TWBR = (F_CPU / F_SCL - 16) / 2
 * ============================================================ */
void i2c_init(void)
{
    /* TWBR debe calcularse en tiempo de compilación */
    TWBR = (uint8_t)(((F_CPU / I2C_FREQ) - 16) / 2);
    TWSR = 0x00;   /* prescaler = 1 */
    TWCR = (1 << TWEN);
}

/* ============================================================
 *  START + dirección (7 bits << 1 | R/W)
 *  Retorna 0 si el esclavo hizo ACK, 1 si NACK
 * ============================================================ */
uint8_t i2c_start(uint8_t addr_rw)
{
    /* Generar START */
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    /* Verificar status: START (0x08) o repeated START (0x10) */
    uint8_t status = TWSR & 0xF8;
    if (status != 0x08 && status != 0x10)
        return 1;

    /* Enviar dirección + R/W */
    TWDR = addr_rw;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    /* ACK recibido (0x18 = SLA+W ACK, 0x40 = SLA+R ACK) */
    status = TWSR & 0xF8;
    if (status == 0x18 || status == 0x40)
        return 0;
    return 1;
}

/* ============================================================
 *  Enviar un byte. Retorna 0 si ACK, 1 si NACK
 * ============================================================ */
uint8_t i2c_write(uint8_t data)
{
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));

    /* 0x28 = Data ACK, 0x30 = Data NACK */
    if ((TWSR & 0xF8) == 0x28)
        return 0;
    return 1;
}

/* ============================================================
 *  Leer un byte. ack=1 -> ACK (más bytes), ack=0 -> NACK (último)
 * ============================================================ */
uint8_t i2c_read(uint8_t ack)
{
    if (ack)
        TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
    else
        TWCR = (1 << TWINT) | (1 << TWEN);

    while (!(TWCR & (1 << TWINT)));
    return TWDR;
}

/* ============================================================
 *  Generar STOP
 * ============================================================ */
void i2c_stop(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    /* TWSTO se limpia solo al completarse el STOP */
    while (TWCR & (1 << TWSTO));
}
