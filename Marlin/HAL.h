/*
 Contributors:
    Copyright (c) 2014 Bob Cousins bobcousins42@googlemail.com
*/
/* **************************************************************************
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
****************************************************************************/

// **************************************************************************
//
// Description:          *** HAL for Arduino Due ***
//
// ARDUINO_ARCH_SAM
// **************************************************************************

#ifndef _HAL_H
#define _HAL_H

// --------------------------------------------------------------------------
// Includes
// --------------------------------------------------------------------------

#include <stdint.h>

#include "Arduino.h"
#include "fastio.h"

#if SAMG //FEYNMAN - SAMG55 ASF drivers
#include <adc2.h>
#include <pmc.h>
#include <tc.h>
#endif

// --------------------------------------------------------------------------
// Defines
// --------------------------------------------------------------------------
/*
//LIBSAM "TC" TO ATMEL ASF "TC" DRIVER API
#define TC_Start tc_start
#define TC_Stop tc_stop
#define PIO_Set pio_set
#define PIO_Clear pio_clear
#define TC_Configure tc_init
#define TC_SetRA tc_write_ra
#define TC_SetRC tc_write_rc
#define PIO_Get pio_get
#define TC_ReadCV tc_read_cv
#define TC_GetStatus tc_get_status
*/
// ---
// Defines
// ---
#if SAMG //FEYNMAN
#define PIO_Configure pio_configure
#define PIO_GetOutputDataStatus pio_get_output_data_status
#define PIO_SetOutput pio_set_output
#define PIO_PullUp pio_pull_up
#define PIO_Set pio_set
#define PIO_Clear pio_clear
#define PIO_Get pio_get
#define TC_Start tc_start
#define TC_Stop tc_stop
#define TC_Configure tc_init
#define TC_SetRA tc_write_ra
#define TC_SetRC tc_write_rc
#define TC_ReadCV tc_read_cv
#define TC_GetStatus tc_get_status
#define TC_SetRB tc_write_rb
typedef enum adc_channel_num adc_channel_num_t;
#endif //SAMG - FEYNMAN

#define analogInputToDigitalPin(IO) IO
#define FORCE_INLINE __attribute__((always_inline)) inline

#define     CRITICAL_SECTION_START	uint32_t primask=__get_PRIMASK(); __disable_irq();
#define     CRITICAL_SECTION_END    if (primask==0) __enable_irq();

// On AVR this is in math.h?
#define square(x) ((x)*(x))

#define strncpy_P(dest, src, num) strncpy((dest), (src), (num))

// --------------------------------------------------------------------------
// Types
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Public Variables
// --------------------------------------------------------------------------

// reset reason set by bootloader
extern uint8_t MCUSR;
volatile static uint32_t debug_counter;

// --------------------------------------------------------------------------
// Public functions
// --------------------------------------------------------------------------

// Disable interrupts
void cli(void);

// Enable interrupts
void sei(void);

static inline void _delay_ms(uint32_t msec) {
	delay(msec);
}

static inline void _delay_us(uint32_t usec) {
  uint32_t n = usec * (F_CPU / 3000000);
  asm volatile(
      "L2_%=_delayMicroseconds:"       "\n\t"
      "subs   %0, #1"                 "\n\t"
      "bge    L2_%=_delayMicroseconds" "\n"
      : "+r" (n) :  
  );
}

int freeMemory(void);
void eeprom_write_byte(unsigned char *pos, unsigned char value);
unsigned char eeprom_read_byte(unsigned char *pos);


// timers
#if MOTHERBOARD == BOARD_AMBIT //Change stepper timer number because it conflicts with Ambit using pin PA6 (TIOB2) to create voltage reference for Y Motor driver.
 #define STEP_TIMER_NUM 5
 #define STEP_TIMER_COUNTER TC1
 #define STEP_TIMER_CHANNEL 2
 #define STEP_TIMER_IRQN TC5_IRQn
 #define HAL_STEP_TIMER_ISR 	void TC5_Handler()
#else
 #define STEP_TIMER_NUM 2
 #define STEP_TIMER_COUNTER TC0
 #define STEP_TIMER_CHANNEL 2
 #define STEP_TIMER_IRQN TC2_IRQn
 #define HAL_STEP_TIMER_ISR 	void TC2_Handler()
#endif

#define TEMP_TIMER_NUM 3
#define TEMP_TIMER_COUNTER TC1
#define TEMP_TIMER_CHANNEL 0
#define TEMP_FREQUENCY 2000

#define TEMP_TIMER_IRQN TC3_IRQn
#define HAL_TEMP_TIMER_ISR 	void TC3_Handler()

#define BEEPER_TIMER_NUM 4
#define BEEPER_TIMER_COUNTER TC1
#define BEEPER_TIMER_CHANNEL 1
#define BEEPER_TIMER_IRQN TC4_IRQn
#define HAL_BEEPER_TIMER_ISR  void TC4_Handler()

#define HAL_TIMER_RATE 		     (F_CPU/2)
#define TICKS_PER_NANOSECOND   (HAL_TIMER_RATE)/1000

#define ENABLE_STEPPER_DRIVER_INTERRUPT()	HAL_timer_enable_interrupt (STEP_TIMER_NUM)
#define DISABLE_STEPPER_DRIVER_INTERRUPT()	HAL_timer_disable_interrupt (STEP_TIMER_NUM)

//

void HAL_step_timer_start(void);
void HAL_temp_timer_start (uint8_t timer_num);

void HAL_timer_enable_interrupt (uint8_t timer_num);
void HAL_timer_disable_interrupt (uint8_t timer_num);

inline
void HAL_timer_isr_status (Tc* tc, uint32_t channel) {
  tc->TC_CHANNEL[channel].TC_SR; // clear status register
}

int HAL_timer_get_count (uint8_t timer_num);
//

void tone(uint8_t pin, int frequency);
void noTone(uint8_t pin);
//void tone(uint8_t pin, int frequency, long duration);

//<<<<<<< HEAD

uint8_t spiflash_busy();
uint8_t spiflash_read_byte(long address);
void spiflash_erase(long address);
void spiflash_write_byte(long address, uint8_t value);
void spiflash_init();
//=======
uint16_t getAdcReading(adc_channel_num_t chan);
void startAdcConversion(adc_channel_num_t chan);
adc_channel_num_t pinToAdcChannel(int pin);

uint16_t getAdcFreerun(adc_channel_num_t chan, bool wait_for_conversion = false);
uint16_t getAdcSuperSample(adc_channel_num_t chan);
void stopAdcFreerun(adc_channel_num_t chan);
//>>>>>>> wurst/master

// --------------------------------------------------------------------------
//
// --------------------------------------------------------------------------

#endif // _HAL_H

