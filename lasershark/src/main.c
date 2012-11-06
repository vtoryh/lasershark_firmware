/*
main.c - Lasershark firmware.
Copyright (C) 2012 Jeffrey Nelson <nelsonjm@macpod.net>

This file is part of Lasershark's Firmware.

Lasershark is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

Lasershark is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Lasershark. If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef __USE_CMSIS
#include "LPC13xx.h"
#endif

#include <cr_section_macros.h>
#include <NXP/crp.h>
#include <stdbool.h>
#include "config.h"
#include "gpio.h"
#include "usb.h"
#include "usbcfg.h"
#include "usbhw.h"
#include "usbcore.h"
#include "type.h"
#include "lasershark.h"

// Variable to store CRP value in. Will be placed automatically
// by the linker when "Enable Code Read Protect" selected.
// See crp.h header for more information
__CRP const unsigned int CRP_WORD = CRP_NO_CRP;

// Turn this off if you are debugging.
#define WATCHDOG_ENABLED 1

#define WDEN (0x1<<0)
#define WDRESET (0x1<<1)

#if (WATCHDOG_ENABLED)
void watchdog_feed() {
	LPC_WDT->FEED = 0xAA;
	LPC_WDT->FEED = 0x55;
}

void watchdog_init() {
	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 15); // Power on WTD peripheral
	//	LPC_SYSCON->CLKOUTCLKSEL = 0x02; // Use WDTOSC for CLKOUT pin

	LPC_SYSCON->WDTOSCCTRL = 0x1 << 5 | 0xF; // FREQSEL = 1.4Mhz, 64 =  ~9.375kHz
	LPC_SYSCON->PDRUNCFG &= ~(0x1 << 6); // Power WDTOSC

	LPC_SYSCON->WDTCLKSEL = 0x02; // Use WDTOSC as the WTD clock source

	LPC_SYSCON->WDTCLKUEN = 0x01; // Update clock source
	// Write 0 then 1 to apply
	LPC_SYSCON->WDTCLKUEN = 0x00;
	LPC_SYSCON->WDTCLKUEN = 0x01;
	while (!(LPC_SYSCON->WDTCLKUEN & 0x01))
		; // Wait for update to occur.

	LPC_SYSCON->WDTCLKDIV = 0x01; // Enabled WDTCLK and set it to divide by 1 (7.8KHz)

	NVIC_EnableIRQ(WDT_IRQn);
	LPC_WDT->TC = 256; // Delay = <276 (minimum of 256, max of 2^24)>*4 / 9.375khz(WDTCLK) =0.10922666666 s
	LPC_WDT->MOD = WDEN | WDRESET; // Cause reset to occur when WDT hits.

	watchdog_feed();
}
#endif

int main(void) {
	/* Basic chip initialization is taken care of in SystemInit() called
	 * from the startup code. SystemInit() and chip settings are defined
	 * in the CMSIS system_<part family>.c file.
	 */

	/* Initialize GPIO (sets up clock) */
	GPIOInit();
	/* Set LED port pin to output */
	GPIOSetDir(LED_PORT, USR1_LED_BIT, 1);
	GPIOSetDir(LED_PORT, USR2_LED_BIT, 1);

	/* Enable IOCON blocks for io pin multiplexing.*/
	LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 16);

	lasershark_init();

	USBIOClkConfig();

	USB_Init(); // USB Initialization
	USB_Connect(TRUE); // USB Connect

#if (WATCHDOG_ENABLED)
	watchdog_init();
#endif

	while (!USB_Configuration) // wait until USB is configured
	{
#if (WATCHDOG_ENABLED)
		watchdog_feed();
#else
		asm("nop");
#endif
	}

	while (1) {
#if (WATCHDOG_ENABLED)
		watchdog_feed();
#else
		asm("nop");
#endif
	}
	return 0;
}