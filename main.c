#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <stdio.h>
#include <stdint.h>

#include "libraries/i2c.h"
#include "libraries/LCD.h"

/* ============================================================
 *  Parámetros del taxímetro
 *  Dinero en centavos, distancia en decímetros (1.6 m = 16 dm)
 * ============================================================ */
#define BANDERAZO_CENT      560UL    /* $5.60 */
#define INCREMENTO_CENT     120UL    /* $1.20 */
#define PASO_RUEDA_DM       300UL     /* 1.6 m por pulso */
#define ODO_LIMITE_DM       10000UL  /* 1 km = 10000 dm */
#define CRONO_LIMITE_S      60U      /* 1 minuto */

/* Timer1: CTC, prescaler 256 -> 1 segundo */
#define T1_PRESC            256UL
#define T1_OCR_1S           ((F_CPU / T1_PRESC) - 1)   /* 62499 @16MHz */

/* ============================================================
 *  Variables compartidas con las ISR
 * ============================================================ */
static volatile uint8_t  viaje_activo = 0;
static volatile uint8_t  refrescar_lcd = 0;
static volatile uint8_t  mostrar_final = 0;

static volatile uint32_t monto_cent   = 0;  /* Monto a pagar          */
static volatile uint32_t dist_total_dm = 0; /* Distancia total        */
static volatile uint32_t tiempo_total_s = 0;/* Tiempo total (s)       */
static volatile uint32_t odo_dm       = 0;  /* Odómetro (se reinicia) */
static volatile uint16_t crono_s      = 0;  /* Cronómetro (se reinicia)*/

/* ============================================================
 *  Control de timers por software
 * ============================================================ */
static void timers_start(void)
{
    /* Timer0: contador de pulsos externos en T0 (PD4), flanco de subida */
    TCNT0  = 255;
    TIFR0  = (1 << TOV0);
    TIMSK0 |= (1 << TOIE0);
    TCCR0B = (1 << CS02) | (1 << CS01) | (1 << CS00);

    /* Timer1: CTC, prescaler 256 -> 1 segundo */
    TCCR1A = 0;                       /* modo normal de pines, WGM11:10 = 0 */
    OCR1A  = (uint16_t)T1_OCR_1S;     /* 62499 @ 16 MHz  <-- faltaba */
    TCNT1  = 0;
    TIFR1  = (1 << OCF1A);            /* limpia bandera pendiente */
    TIMSK1 |= (1 << OCIE1A);
    TCCR1B = (1 << WGM12) | (1 << CS12);
}

static void timers_stop(void)
{
    TCCR0B = 0;                       /* detiene Timer0 */
    TIMSK0 &= ~(1 << TOIE0);

    TCCR1B = (1 << WGM12);            /* detiene Timer1 (sin reloj) */
    TIMSK1 &= ~(1 << OCIE1A);
}

/* Aplica el incremento de tarifa y reinicia cronómetro y odómetro */
static inline void cobrar_incremento(void)
{
    monto_cent += INCREMENTO_CENT;
    odo_dm  = 0;
    crono_s = 0;
}

/* ============================================================
 *  ISR Timer0: una vuelta de llanta = 1.6 m
 * ============================================================ */
ISR(TIMER0_OVF_vect)
{
    TCNT0 = 255;                      /* recargar para el siguiente pulso */

    dist_total_dm += PASO_RUEDA_DM;
    odo_dm        += PASO_RUEDA_DM;

    if (odo_dm >= ODO_LIMITE_DM)
        cobrar_incremento();

    refrescar_lcd = 1;
}

/* ============================================================
 *  ISR Timer1: cada segundo
 * ============================================================ */
ISR(TIMER1_COMPA_vect)
{
    tiempo_total_s++;
    crono_s++;

    if (crono_s >= CRONO_LIMITE_S)
        cobrar_incremento();

    refrescar_lcd = 1;
}

/* ============================================================
 *  INT0: botón INICIO DE VIAJE (PD2)
 * ============================================================ */
ISR(INT0_vect)
{
    if (viaje_activo)             /* ignora rebotes / pulsaciones repetidas */
        return;

    monto_cent     = BANDERAZO_CENT;
    dist_total_dm  = 0;
    tiempo_total_s = 0;
    odo_dm         = 0;
    crono_s        = 0;

    mostrar_final  = 0;
    viaje_activo   = 1;
    refrescar_lcd  = 1;

    timers_start();
}

/* ============================================================
 *  INT1: botón FIN DE VIAJE (PD3)
 * ============================================================ */
ISR(INT1_vect)
{
    if (!viaje_activo)
        return;

    timers_stop();

    viaje_activo  = 0;
    refrescar_lcd = 0;
    mostrar_final = 1;
}

/* ============================================================
 *  Pantalla
 * ============================================================ */
static void lcd_linea(uint8_t linea, const char *txt)
{
    char buf[17];
    snprintf(buf, sizeof(buf), "%-16s", txt);   /* rellena con espacios */
    lcd_goto_xy(linea, 0);
    lcd_write_string(buf);
}

static void pantalla_viaje(void)
{
    uint32_t monto, dist_dm, tiempo;
    char l0[20], l1[20];

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        monto   = monto_cent;
        dist_dm = dist_total_dm;
        tiempo  = crono_s;
    }

    uint32_t metros = dist_dm / 10;

    snprintf(l0, sizeof(l0), "Monto: $%lu.%02lu",
             monto / 100, monto % 100);
    snprintf(l1, sizeof(l1), "%lu.%02luKm T:%lus",
             metros / 1000, (metros % 1000) / 10, tiempo);

    lcd_linea(0, l0);
    lcd_linea(1, l1);
}

static void pantalla_final(void)
{
    uint32_t monto;
    char l1[20];

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        monto = monto_cent;
    }

    snprintf(l1, sizeof(l1), "$%lu.%02lu", monto / 100, monto % 100);

    lcd_linea(0, "Total a pagar:");
    lcd_linea(1, l1);
}

/* ============================================================
 *  Inicialización de pines e interrupciones externas
 * ============================================================ */
static void io_init(void)
{
    /* PD2 (INT0), PD3 (INT1), PD4 (T0) como entradas con pull-up */
    DDRD  &= ~((1 << PD2) | (1 << PD3) | (1 << PD4));
    PORTD |=  ((1 << PD2) | (1 << PD3) | (1 << PD4));

    /* INT0 e INT1 por flanco de bajada */
    EICRA = (1 << ISC01) | (1 << ISC11);
    EIFR  = (1 << INTF0) | (1 << INTF1);
    EIMSK = (1 << INT0)  | (1 << INT1);
}

/* ============================================================
 *  MAIN
 * ============================================================ */
int main(void)
{
    i2c_init();
    lcd_init();
    io_init();

    lcd_linea(0, "   TAXIMETRO");
    lcd_linea(1, "Pulse INICIO");

    sei();

    while (1)
    {
        if (mostrar_final) {
            mostrar_final = 0;
            pantalla_final();
        }
        else if (refrescar_lcd) {
            refrescar_lcd = 0;
            pantalla_viaje();
        }
    }
}
