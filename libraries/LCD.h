#ifndef LCD_H_
#define LCD_H_

#include <stdint.h>

/* ============================================================
 *  Comandos estándar HD44780
 * ============================================================ */
#define LCD_CMD_CLEAR_DISPLAY        0x01
#define LCD_CMD_RETURN_HOME          0x02
#define LCD_CMD_ENTRY_MODE           0x06
#define LCD_CMD_DISPLAY_OFF          0x08
#define LCD_CMD_DISPLAY_ON           0x0C
#define LCD_CMD_DISPLAY_NO_CURSOR    0x0C
#define LCD_CMD_DISPLAY_CURSOR       0x0E
#define LCD_CMD_DISPLAY_BLINK        0x0F
#define LCD_CMD_SHIFT_CURSOR_LEFT    0x10
#define LCD_CMD_SHIFT_CURSOR_RIGHT   0x14
#define LCD_CMD_SHIFT_DISPLAY_LEFT   0x18
#define LCD_CMD_SHIFT_DISPLAY_RIGHT  0x1C

#define LCD_CMD_4BIT_1ROW_5X7        0x20
#define LCD_CMD_4BIT_2ROW_5X7        0x28
#define LCD_CMD_8BIT_1ROW_5X7        0x30
#define LCD_CMD_8BIT_2ROW_5X7        0x38

#define LCD_CMD_SET_DDRAM_ADDR       0x80

/* ============================================================
 *  Dirección I2C del PCF8574 (ajustar según el módulo)
 *  0x27 es lo más común; 0x3F en algunos módulos.
 *  Dirección de 7 bits (sin el bit R/W).
 * ============================================================ */
#ifndef LCD_I2C_ADDR
#define LCD_I2C_ADDR   0x27
#endif

/* Backlight */
#define LCD_BACKLIGHT_ON   0x08
#define LCD_BACKLIGHT_OFF  0x00

/* Inicialización (debe llamarse después de i2c_init del usuario) */
void lcd_init(void);

/* Envía un comando al controlador */
void lcd_send_command(uint8_t command);

/* Escribe un solo carácter en la posición actual del cursor */
void lcd_write_character(uint8_t character);

/* Escribe una cadena terminada en '\0' */
void lcd_write_string(const char *word);

/* Limpia la pantalla y lleva el cursor a (0,0) */
void lcd_clear(void);

/* Posiciona el cursor. line: 0 o 1. pos: 0..15 */
void lcd_goto_xy(uint8_t line, uint8_t pos);

/* Control del backlight */
void lcd_backlight(uint8_t on);

#endif /* LCD_H_ */
