/*
 * Copyright (c) 2015, Vijay Karthick Baskar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of the FreeBSD Project.
 */

/*
 *------------------------------------------------------------------------------
 * Include Files
 *------------------------------------------------------------------------------
 */

#include "startup.h"

/*
 *------------------------------------------------------------------------------
 * Private Defines
 *------------------------------------------------------------------------------
 */

/*
 *------------------------------------------------------------------------------
 * Private Macros
 *------------------------------------------------------------------------------
 */

/*
 *------------------------------------------------------------------------------
 * Private Data Types
 *------------------------------------------------------------------------------
 */

/*
 *------------------------------------------------------------------------------
 * Public Variables
 *------------------------------------------------------------------------------
 */

/*
 *------------------------------------------------------------------------------
 * Private Variables
 *------------------------------------------------------------------------------
 */

/* Counter to check WDOG reset */
int wdog_counter;

/* Dummy variable to hold received byte */
volatile char received_ch = 0;

/*
 *------------------------------------------------------------------------------
 * Public Constants
 *------------------------------------------------------------------------------
 */

/*
 *------------------------------------------------------------------------------
 * Private Constants
 *------------------------------------------------------------------------------
 */

/*
 *------------------------------------------------------------------------------
 * Private Function Prototypes
 *------------------------------------------------------------------------------
 */

/*
 *------------------------------------------------------------------------------
 * Public Function Definitions
 *------------------------------------------------------------------------------
 */

void disable_watchdog() {
	/* Disable the watchdog */
	WDTCTL = WDTPW | WDTHOLD;
}

void enable_watchdog() {
	/* Enable the watchdog */
	WDTCTL = WDTPW;
}

void clock_init() {
	/* Set the DCO frequency to 8 MHz */
	DCOCTL = DCO_8MHZ;

	/* Disable the external oscillator */
	BCSCTL1 = XT2OFF;

	/* Set the range select to produce 8 MHz frequency clock */
	BCSCTL1 |= RSEL_8MHZ;

	/* Select DCO as the clock source */
	BCSCTL2 &= ~(SELM1 | SELM0);

	/* Select the clock scaling factor as 1 */
	BCSCTL2 &= ~(DIVM1 | DIVM0);

	/* Set the sub module clock source as DCO */
	BCSCTL2 &= ~SELS;

	/* Set scaling factor for sub module clock as 8 to get 1 MHz frequency */
	BCSCTL2 |= (DIVS1 | DIVS0);

	/* Set the DCO resistor to be internal - internal resistor is not accurate */
	BCSCTL2 &= ~DCO_EXT_RESISTOR_ENABLE;
}

void init_wdog() {
	/**
	 * No changes required, as SMCLK is selected with scaler as 32768 = 32.768ms timeout by default
	 * i.e. 1/1MHz * 32768
	 */
}

void reset_wdog() {
	if (wdog_counter < MIN_REFRESH_VALUE) {
		wdog_counter++;
		return;
	}
	else {
		wdog_counter = 0;
		WDTCTL = WDTPW | WDTCNTCL;
	}
}

void gpio_init() {
	/* Settings for port 1 I/O */
	/* Pin 0 is LED1 */
	PORT1_GPIO_MODE(PORT1_PIN0_LED1);
	PORT1_DIR_OUTPUT(PORT1_PIN0_LED1);
	/* Pin 1 is MISO */
	PORT1_SECONDARY_FUNCTION(PORT1_PIN1_MISO);
	PORT1_DIR_INPUT(PORT1_PIN1_MISO);
	/* Pin 2 is MOSI */
	PORT1_SECONDARY_FUNCTION(PORT1_PIN2_MOSI);
	PORT1_DIR_OUTPUT(PORT1_PIN2_MOSI);
	/* Pin 4 is SPI clock */
	PORT1_SECONDARY_FUNCTION(PORT1_PIN4_SPI_CLK);
	PORT1_DIR_OUTPUT(PORT1_PIN4_SPI_CLK);
	/* Make PORT1 Pin 5 as SPI Chip Select output, CS is active low */
	PORT1_GPIO_MODE(PORT1_PIN5_SPI_CS);
	PORT1_OUTPUT_HIGH(PORT1_PIN5_SPI_CS);
	PORT1_DIR_OUTPUT(PORT1_PIN5_SPI_CS);
}

void delay_ms(int ms) {
	/* We compare with 30ms as the WDOG timeout value is 32.768ms */
	/* Number of cycles for 1 second delay = 8000000 cycles */
	/* Number of cycles for x ms delay = ((x * 8000000)/1000) cycles */
	/* Number of cycles for 10 ms delay = ((10 * 8000000)/1000) cycles = 80000 cycles */
	if (ms <= 10) {
		_delay_cycles(80000);
		reset_wdog();
	}
	else {
		int times = ms/10;
		int count;
		for (count=0; count<times; count++) {
			_delay_cycles(80000);
			reset_wdog();
			if (ms%10 > 5) {
				/* 10ms worth more clocks of delay to incorporate ms%30 remainder */
				_delay_cycles(80000);
				reset_wdog();
			}
		}
	}
}

void spi_init() {
	/* Hold the USCI in a software reset state */
	UCA0CTL1 = SOFTWARE_RESET_ENABLE;

	/* Configure the UCA0CTL0 register */
	UCA0CTL0 = SPI_MODE_ENABLE;
	UCA0CTL0 |= CLOCK_PHASE_CAPTURE_FIRST_EDGE;		//Data is changed on the first UCLK edge and captured on the following edge.
	UCA0CTL0 &= ~CLOCK_POLARITY_INACTIVE_ST_HIGH;	//The inactive state is low.
	UCA0CTL0 |= MSB_FIRST | MASTER_MODE_ENABLE | SYNCH_MODE_ENABLE;

	/* Configure the UCA0CTL1 register */
	UCA0CTL1 = SPI_CLOCK_SRC_SMCLK;
	UCA0BR0 |= 0x02;								//  /2
	UCA0BR1 = 0;									//
	UCA0MCTL   =   0;								//  No  modulation
	UCA0CTL1 &= ~SOFTWARE_RESET_ENABLE;

	/*
	 * Configure the IE2 register
	 * @Todo: Move the interrupts to interrups section
	 */
	IE2 = TRANSMIT_INTERRUPT_ENABLE | RECEIVE_INTERRUPT_ENABLE;
}

void spi_test_method1() {
	/* Enable the slave, assuming slave is active low */
	PORT1_OUTPUT_LOW(PORT1_PIN5_SPI_CS);

	/* Check if any transfer is in progress */
	while (SPI_STATUS_REGISTER & DATA_TRANSFER_IN_PROGRESS);
	/* Transmit test byte */
	TRANSMIT_BUFFER = 0xAB;

	/* Check if any receive is in progress */
	while (SPI_STATUS_REGISTER & DATA_TRANSFER_IN_PROGRESS);
	/* Copy received data to dummy character variable */
	received_ch = RECEIVE_BUFFER;

	/* Disable the slave, assuming slave is active low */
	PORT1_OUTPUT_LOW(PORT1_PIN5_SPI_CS);
}

void spi_test_method2() {
	/* Enable the slave, assuming slave is active low */
	PORT1_OUTPUT_LOW(PORT1_PIN5_SPI_CS);

	/* Check if any transfer is in progress */
	while  (!(IFG2 &   UCA0TXIFG));
	/* Transmit test byte */
	TRANSMIT_BUFFER  =   0xAB;

	/* Check if any receive is in progress */
	while  (!(IFG2 &   UCA0RXIFG));
	/* Copy received data to dummy character variable */
	received_ch    =   RECEIVE_BUFFER;

	/* Disable the slave, assuming slave is active low */
	PORT1_OUTPUT_LOW(PORT1_PIN5_SPI_CS);
}
