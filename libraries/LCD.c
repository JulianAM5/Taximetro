#include <avr/io.h>
#include <util/delay.h>
#include "LCD.h"
#include "i2c.h"   /* <-- tu driver I2C (TWI) */

/* ============================================================
 *  Bits del PCF8574 asignados al LCD
 * ============================================================ */
#define LCD_RS_BIT   0x01   /* P0 -> RS */
#define LCD_RW_BIT   0x02   /* P1 -> RW */
#define LCD_EN_BIT   0x04   /* P2 -> EN */
#define LCD_BL_BIT   0x08   /* P3 -> Backlight */

/* Estado actual del expansor (para no perder el backlight) */
static uint8_t lcd_state = LCD_BL_BIT;  /* backlight encendido por defecto */

/* ============================================================
 *  Escribir un byte al PCF8574
 * ============================================================ */
static void pcf_write(uint8_t data)
{
    i2c_start(LCD_I2C_ADDR << 1);       /* dirección + W */
    i2c_write(data);
    i2c_stop();
}

/* ============================================================
 *  Escritura de un nibble (4 bits) al LCD vía PCF8574
 *  rs = 0 -> comando, rs = 1 -> dato
 *  D4..D7 se colocan en P4..P7. Pulso en EN.
 * ============================================================ */
static void lcd_write_nibble(uint8_t nibble, uint8_t rs)
{
    uint8_t data = (uint8_t)((nibble & 0x0F) << 4);
    data |= lcd_state;                  /* preserva backlight */
    if (rs) data |= LCD_RS_BIT;

    /* EN = 0, presentar datos */
    pcf_write(data & ~LCD_EN_BIT);
    _delay_us(50);

    /* Pulso de enable (mín 450 ns) */
    pcf_write(data | LCD_EN_BIT);
    _delay_us(50);
    pcf_write(data & ~LCD_EN_BIT);
    _delay_us(50);
}

/* ============================================================
 *  Inicialización estándar HD44780 en modo 4 bits
 * ============================================================ */
void lcd_init(void)
{
    _delay_ms(50);   /* Espera de encendido */

    /* Backlight ON y bus estable */
    pcf_write(lcd_state);
    _delay_ms(10);

    /* Secuencia mágica de despertar (3x 0x03, luego 0x02) */
    lcd_write_nibble(0x03, 0);
    _delay_ms(5);
    lcd_write_nibble(0x03, 0);
    _delay_us(150);
    lcd_write_nibble(0x03, 0);
    _delay_us(150);
    lcd_write_nibble(0x02, 0);   /* switch a 4-bit */
    _delay_us(150);

    /* Ya en modo 4-bit */
    lcd_send_command(LCD_CMD_4BIT_2ROW_5X7);
    _delay_ms(1);

    /* Display ON, cursor OFF, blink OFF */
    lcd_send_command(LCD_CMD_DISPLAY_NO_CURSOR);
    _delay_ms(1);

    /* Limpiar pantalla */
    lcd_send_command(LCD_CMD_CLEAR_DISPLAY);
    _delay_ms(2);

    /* Entry mode: incrementa cursor, sin shift */
    lcd_send_command(LCD_CMD_ENTRY_MODE);
    _delay_ms(1);

    /* Cursor al inicio */
    lcd_send_command(LCD_CMD_SET_DDRAM_ADDR);
    _delay_ms(1);
}

/* ============================================================
 *  Enviar un comando (2 nibbles: alto, luego bajo)
 * ============================================================ */
void lcd_send_command(uint8_t command)
{
    lcd_write_nibble(command >> 4, 0);
    lcd_write_nibble(command & 0x0F, 0);

    /* Clear y HOME necesitan ~2 ms */
    if (command == LCD_CMD_CLEAR_DISPLAY || command == LCD_CMD_RETURN_HOME)
        _delay_ms(2);
}

/* ============================================================
 *  Escribir un carácter
 * ============================================================ */
void lcd_write_character(uint8_t character)
{
    lcd_write_nibble(character >> 4, 1);
    lcd_write_nibble(character & 0x0F, 1);
}

/* ============================================================
 *  Escribir una cadena terminada en '\0'
 * ============================================================ */
void lcd_write_string(const char *word)
{
    while (*word)
        lcd_write_character((uint8_t)*word++);
}

/* ============================================================
 *  Limpiar pantalla
 * ============================================================ */
void lcd_clear(void)
{
    lcd_send_command(LCD_CMD_CLEAR_DISPLAY);
    _delay_ms(2);
}

/* ============================================================
 *  Posicionar cursor. line: 0 o 1. pos: 0..15
 * ============================================================ */
void lcd_goto_xy(uint8_t line, uint8_t pos)
{
    uint8_t addr = (uint8_t)((line ? 0x40 : 0x00) | (pos & 0x0F));
    lcd_send_command((uint8_t)(LCD_CMD_SET_DDRAM_ADDR | addr));
    _delay_us(50);
}

/* ============================================================
 *  Control de backlight
 * ============================================================ */
void lcd_backlight(uint8_t on)
{
    if (on)
        lcd_state |=  LCD_BL_BIT;
    else
        lcd_state &= ~LCD_BL_BIT;

    pcf_write(lcd_state);
}
