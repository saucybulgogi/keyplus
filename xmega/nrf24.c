// Copyright 2017 jem@seethis
// Licensed under the MIT license (http://opensource.org/licenses/MIT)

#include "core/hardware.h"

#include <avr/io.h>

#include "core/nrf24.h"

// add a little abstraction here so that we can move the SPI port around if desired
#define NRF24_SPI_PORT PORTC
#define NRF24_SPI SPIC

#ifndef NRF24_CE_PORT
#define NRF24_CE_PORT PORTR
#endif

#ifndef NRF24_CE_PIN
#define NRF24_CE_PIN PIN0_bm
#endif

#define SS   4
#define MOSI 5
#define MISO 6
#define SCK  7

#define MAX_NRF24_SPI_SPEED 10000000UL

// enable SPI master, mode 0, msb
#define SPI_MODE (SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc)

// NOTE: @32MHz, spi bit rate is 8Mbps. nRF24L01+ max SPI rate is 10Mbps
#if ((F_CPU/2) <= MAX_NRF24_SPI_SPEED)
#define SPI_SETTINGS (SPI_MODE | SPI_CLK2X_bm | SPI_PRESCALER_DIV4_gc)
#elif ((F_CPU/4) <= MAX_NRF24_SPI_SPEED)
#define SPI_SETTINGS (SPI_MODE | SPI_PRESCALER_DIV4_gc)
#else
#define SPI_SETTINGS (SPI_MODE | SPI_PRESCALER_DIV8_gc)
#endif

#if ((CLOCK_SPEED_SLOW/2) <= MAX_NRF24_SPI_SPEED)
#define SPI_SETTINGS_SLOW_CLOCK (SPI_SETTINGS | SPI_CLK2X_bm | SPI_PRESCALER_DIV4_gc)
#elif ((CLOCK_SPEED_SLOW/4) <= MAX_NRF24_SPI_SPEED)
#define SPI_SETTINGS_SLOW_CLOCK (SPI_SETTINGS | SPI_PRESCALER_DIV4_gc)
#else
#define SPI_SETTINGS_SLOW_CLOCK (SPI_SETTINGS | SPI_PRESCALER_DIV8_gc)
#endif

bool is_nrf24_initialized = false;

static void spi_init(PORT_t *port, SPI_t *spi) {
    port->DIRSET = (1<<SS) | (1<<MOSI) | (1<<SCK); // outputs
    port->OUTSET = (1<<SS); // pull high so slave is inactive

    // spi is polling based
    spi->INTCTRL = 0x00;

    if (g_slow_clock_mode) {
        spi->CTRL = SPI_SETTINGS_SLOW_CLOCK;
    } else {
        spi->CTRL = SPI_SETTINGS;
    }

    while (spi->STATUS & SPI_IF_bm) {
        *((volatile uint8_t*)&spi->DATA);
    }

}

inline static uint8_t spi_send_byte(SPI_t *spi, uint8_t byte) {
    spi->DATA = byte;
    // TODO/Note: use idle sleep mode here instead of busy loop to save power ???
    //
    // It might take longer to enter idle sleep mode than to receive a response
    // though. At mcu clock at 2MHz, then SPI is 1Mbps. To send a byte, it'll
    // take 8µs (16 cycles, really 12 since it takes 4 cycles to wake from
    // sleep), so if it takes longer that to enter sleep, then there's no
    // point.
    //
    // However, if we use prescaler of 4 for the SPI clock when running with
    // faster clock (USB mode CPU@32MHz and SPI@8Mbps), then idle sleep mode
    // will make a difference.
    while (!(spi->STATUS & SPI_IF_bm));
    return spi->DATA;
}

inline static void spi_ss(PORT_t *port, uint8_t val) {
    if (val) {
        port->OUTSET = (1<<SS); // release ss
    } else {
        port->OUTCLR = (1<<SS); // pull ss
    }
}

inline static void nrf24_ce_set(PORT_t *port, uint8_t val) {
    if (val) {
        port->OUTSET = NRF24_CE_PIN;
    } else {
        port->OUTCLR = NRF24_CE_PIN;
    }
}

inline static void nrf24_ce_init(PORT_t *port) {
    port->DIRSET = NRF24_CE_PIN;
}

uint8_t nrf24_spi_send_byte(uint8_t byte) {
    return spi_send_byte(&NRF24_SPI, byte);
}

void nrf24_csn(uint8_t val) {
    spi_ss(&NRF24_SPI_PORT, val);
}

void nrf24_ce(uint8_t val) {
    nrf24_ce_set(&NRF24_CE_PORT, val);
}

void nrf24_init(void) {
    is_nrf24_initialized = true;
    spi_init(&NRF24_SPI_PORT, &NRF24_SPI);
    nrf24_ce_init(&NRF24_CE_PORT);
    nrf24_ce_set(&NRF24_CE_PORT, 0);
}

void nrf24_disable(void) {
    if (is_nrf24_initialized) {
        nrf24_power_set(0);
        NRF24_SPI_PORT.DIRCLR = (1<<SS) | (1<<MOSI) | (1<<SCK); // inputs
        NRF24_CE_PORT.DIRSET = NRF24_CE_PIN;
    }
}
