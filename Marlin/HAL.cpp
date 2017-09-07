/*
   Contributors:
   Copyright (c) 2014 Bob Cousins bobcousins42@googlemail.com
*/

/*
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
*/

// **************************************************************************
//
// Description:          *** HAL for Arduino Due ***
//
// **************************************************************************

// --------------------------------------------------------------------------
// Includes
// --------------------------------------------------------------------------

#include "HAL.h"
#include "Configuration.h"

#ifdef EEPROM_SETTINGS
#include <Wire.h>
#include <SPI.h>
#endif

// --------------------------------------------------------------------------
// Externals
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Local defines
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Types
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Variables
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Public Variables
// --------------------------------------------------------------------------

uint8_t MCUSR;

// --------------------------------------------------------------------------
// Private Variables
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Function prototypes
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Private functions
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// Public functions
// --------------------------------------------------------------------------

// disable interrupts
void cli(void)
{
	noInterrupts();
}

// enable interrupts
void sei(void)
{
	interrupts();
}

extern "C" {
  extern unsigned int _ebss; // end of bss section
}

// return free memory between end of heap (or end bss) and whatever is current
int freeMemory() {
  int free_memory;
  int heap_end = (int)_sbrk(0);

  if(heap_end == 0)
    free_memory = ((int)&free_memory) - ((int)&_ebss);
  else
    free_memory = ((int)&free_memory) - heap_end;

  return free_memory;
}

// --------------------------------------------------------------------------
// spiflash
// --------------------------------------------------------------------------
#ifdef EEPROM_SETTINGS
#define SPIFLASH_CS 86 //D86 PB21 CS2 //77 //CS0  // Chip Select PIN
#define SPIFLASH_WRITE_ENABLE  0x06  //Write Enable (06h)
#define SPIFLASH_READ_STATUS   0x05  //Read Status Register (05h)
#define SPIFLASH_WRITE_IN_PROGRESS_MASK 0b00000001
#define SPIFLASH_PAGE_PROGRAM  0x02  //Page Program (PP) (02h)
#define SPIFLASH_READ_DATA     0x03  //Read Data (03h)
#define SPIFLASH_SECTOR_ERASE  0x20  //Sector Erase (SE) (20h)
#define SPIFLASH_READ_ID       0x90  //Read Manufacturer / Device ID

void spiflash_init()
{
  pinMode(SPIFLASH_CS,OUTPUT);
  digitalWrite(SPIFLASH_CS,HIGH);
  SPI.begin(SPIFLASH_CS);
  digitalWrite(SPIFLASH_CS,LOW);
  // MFG ID
  SPI.transfer(SPIFLASH_CS, SPIFLASH_READ_ID, SPI_CONTINUE);
  SPI.transfer(SPIFLASH_CS, 0x00, SPI_CONTINUE);
  SPI.transfer(SPIFLASH_CS, 0x00, SPI_CONTINUE);
  SPI.transfer(SPIFLASH_CS, 0x00, SPI_CONTINUE);
  SerialUSB.print(PSTR("SPIFLASH_MFG_ID: ")); 
  SerialUSB.print( SPI.transfer(SPIFLASH_CS, 0x00, SPI_CONTINUE) );
  SerialUSB.print(" "); 
  SerialUSB.println( SPI.transfer(SPIFLASH_CS, 0x00) );
  digitalWrite(SPIFLASH_CS,HIGH);

  //Quick fix for archim. The default SPI CS pin conflicts with E0_STEP_PIN.
  #if MOTHERBOARD == BOARD_AMBIT
    pinMode(E0_STEP_PIN,OUTPUT);
  #endif
}

uint8_t spiflash_busy()
{
  SPI.transfer(SPIFLASH_CS, SPIFLASH_READ_STATUS, SPI_CONTINUE);
  uint8_t busy = SPI.transfer(SPIFLASH_CS, 0x00) & 0b00000001;
  if(busy) SerialUSB.println(PSTR("SPIFLASH BUSY"));
  return(busy);
}

uint8_t spiflash_read_byte(long address)
{
  SPI.transfer(SPIFLASH_CS, SPIFLASH_READ_DATA, SPI_CONTINUE);
  SPI.transfer(SPIFLASH_CS, address >> 16, SPI_CONTINUE);
  SPI.transfer(SPIFLASH_CS, address >> 8, SPI_CONTINUE);
  SPI.transfer(SPIFLASH_CS, address, SPI_CONTINUE);
  uint8_t result = SPI.transfer(SPIFLASH_CS, 0x00);
  return result;  
}

void spiflash_erase(long address)
{
  //SerialUSB.println(PSTR("SPI FLASH ERASE!"));
  SPI.transfer(SPIFLASH_CS, SPIFLASH_WRITE_ENABLE);
  SPI.transfer(SPIFLASH_CS, SPIFLASH_SECTOR_ERASE, SPI_CONTINUE);
  SPI.transfer(SPIFLASH_CS, address >> 16, SPI_CONTINUE);
  SPI.transfer(SPIFLASH_CS, address >> 8, SPI_CONTINUE);
  SPI.transfer(SPIFLASH_CS, address);
  while(spiflash_busy());  
}

void spiflash_write_byte(long address, uint8_t value)
{
  while(spiflash_busy());
  SPI.transfer(SPIFLASH_CS, SPIFLASH_WRITE_ENABLE);
  SPI.transfer(SPIFLASH_CS, SPIFLASH_PAGE_PROGRAM, SPI_CONTINUE);
  SPI.transfer(SPIFLASH_CS, address >> 16, SPI_CONTINUE);
  SPI.transfer(SPIFLASH_CS, address >> 8, SPI_CONTINUE);
  SPI.transfer(SPIFLASH_CS, address, SPI_CONTINUE);
  SPI.transfer(SPIFLASH_CS, value); // Value to Write
}
#endif

// --------------------------------------------------------------------------
// eeprom
// --------------------------------------------------------------------------
#ifdef EEPROM_SETTINGS
static bool eeprom_initialised = false;
static uint8_t eeprom_device_address = 0x50;

static void eeprom_init(void) {
	if (!eeprom_initialised) {
		Wire.begin();
		eeprom_initialised = true;
	}
}

void eeprom_write_byte(unsigned char *pos, unsigned char value) {
	unsigned eeprom_address = (unsigned) pos;

	eeprom_init();

	Wire.beginTransmission(eeprom_device_address);
	Wire.write((int)(eeprom_address >> 8));   // MSB
	Wire.write((int)(eeprom_address & 0xFF)); // LSB
    Wire.write(value);
	Wire.endTransmission();

	// wait for write cycle to complete
	// this could be done more efficiently with "acknowledge polling"
	delay(5);
}


unsigned char eeprom_read_byte(unsigned char *pos) {
	byte data = 0xFF;
	unsigned eeprom_address = (unsigned) pos;

	eeprom_init ();

	Wire.beginTransmission(eeprom_device_address);
	Wire.write((int)(eeprom_address >> 8));   // MSB
	Wire.write((int)(eeprom_address & 0xFF)); // LSB
	Wire.endTransmission();
	Wire.requestFrom(eeprom_device_address, (byte)1);
	if (Wire.available())
		data = Wire.read();
	return data;
}
#endif

// --------------------------------------------------------------------------
// Timers
// --------------------------------------------------------------------------

typedef struct {
  Tc          *pTimerRegs;
  uint16_t    channel;
  IRQn_Type   IRQ_Id;
} tTimerConfig;

#ifdef TC2
#define  NUM_HARDWARE_TIMERS 9
#else
#define  NUM_HARDWARE_TIMERS 6
#endif

static const tTimerConfig TimerConfig [NUM_HARDWARE_TIMERS] =
{
  { TC0, 0, TC0_IRQn},
  { TC0, 1, TC1_IRQn},
  { TC0, 2, TC2_IRQn},
  { TC1, 0, TC3_IRQn},
  { TC1, 1, TC4_IRQn},
  { TC1, 2, TC5_IRQn},
#ifdef TC2
  { TC2, 0, TC6_IRQn},
  { TC2, 1, TC7_IRQn},
  { TC2, 2, TC8_IRQn},
#endif
};

/*
	Timer_clock1: Prescaler 2 -> 42MHz
	Timer_clock2: Prescaler 8 -> 10.5MHz
	Timer_clock3: Prescaler 32 -> 2.625MHz
	Timer_clock4: Prescaler 128 -> 656.25kHz
*/

// new timer by Ps991
// thanks for that work
// http://forum.arduino.cc/index.php?topic=297397.0

void HAL_step_timer_start() {
  pmc_set_writeprotect(false); //remove write protection on registers
  
  // Timer for stepper
  // Timer 3 HAL.h STEP_TIMER_NUM
  // uint8_t timer_num = STEP_TIMER_NUM;
  
  // Get the ISR from table
  Tc *tc = STEP_TIMER_COUNTER;
  IRQn_Type irq = STEP_TIMER_IRQN;
  uint32_t channel = STEP_TIMER_CHANNEL;
  
  pmc_enable_periph_clk((uint32_t)irq); //we need a clock?
  
  tc->TC_CHANNEL[channel].TC_CCR = TC_CCR_CLKDIS;
  
  tc->TC_CHANNEL[channel].TC_SR; // clear status register
  tc->TC_CHANNEL[channel].TC_CMR =  TC_CMR_WAVSEL_UP_RC | TC_CMR_WAVE | TC_CMR_TCCLKS_TIMER_CLOCK1;

  tc->TC_CHANNEL[channel].TC_IER /*|*/= TC_IER_CPCS; //enable interrupt on timer match with register C
  tc->TC_CHANNEL[channel].TC_IDR = ~TC_IER_CPCS;
  tc->TC_CHANNEL[channel].TC_RC   = (VARIANT_MCK >> 1) / 1000; // start with 1kHz as frequency; //interrupt occurs every x interations of the timer counter
  
  tc->TC_CHANNEL[channel].TC_CCR = TC_CCR_CLKEN | TC_CCR_SWTRG;
  
  NVIC_EnableIRQ(irq); //enable Nested Vector Interrupt Controller
}


void HAL_temp_timer_start (uint8_t timer_num) {
	Tc *tc = TimerConfig [timer_num].pTimerRegs;
	IRQn_Type irq = TimerConfig [timer_num].IRQ_Id;
	uint32_t channel = TimerConfig [timer_num].channel;

	pmc_set_writeprotect(false);
	pmc_enable_periph_clk((uint32_t)irq);
	
	NVIC_SetPriorityGrouping(4);
	
	NVIC_SetPriority(irq, NVIC_EncodePriority(4, 6, 0));
	
	TC_Configure (tc, channel, TC_CMR_CPCTRG | TC_CMR_TCCLKS_TIMER_CLOCK4);
  tc->TC_CHANNEL[channel].TC_IER |= TC_IER_CPCS; //enable interrupt on timer match with register C

	tc->TC_CHANNEL[channel].TC_RC   = (VARIANT_MCK >> 7) / TEMP_FREQUENCY;
	TC_Start(tc, channel);

	NVIC_EnableIRQ(irq);
}

void HAL_timer_enable_interrupt (uint8_t timer_num) {
	const tTimerConfig *pConfig = &TimerConfig [timer_num];
	pConfig->pTimerRegs->TC_CHANNEL [pConfig->channel].TC_IER = TC_IER_CPCS; //enable interrupt
	pConfig->pTimerRegs->TC_CHANNEL [pConfig->channel].TC_IDR = ~TC_IER_CPCS;//remove disable interrupt
}

void HAL_timer_disable_interrupt (uint8_t timer_num) {
	const tTimerConfig *pConfig = &TimerConfig [timer_num];
	pConfig->pTimerRegs->TC_CHANNEL [pConfig->channel].TC_IDR = TC_IER_CPCS; //disable interrupt
}

int HAL_timer_get_count (uint8_t timer_num) {
	Tc *tc = TimerConfig [timer_num].pTimerRegs;
	uint32_t channel = TimerConfig [timer_num].channel;
	return tc->TC_CHANNEL[channel].TC_RC;
}

void HAL_timer_set_count (uint8_t timer_num, uint32_t count)
{
  const tTimerConfig *pConfig = &TimerConfig [timer_num];

  TC_SetRC (pConfig->pTimerRegs, pConfig->channel, count);
}

// Due have no tone, this is from Repetier 0.92.3
static uint32_t tone_pin;

void tone(uint8_t pin, int frequency) {
  // set up timer counter 1 channel 0 to generate interrupts for
  // toggling output pin.  
  
  /*TC1, 1, TC4_IRQn*/
  uint8_t timer_num = BEEPER_TIMER_NUM;
  
  Tc *tc = TimerConfig [timer_num].pTimerRegs;
  IRQn_Type irq = TimerConfig [timer_num].IRQ_Id;
	uint32_t channel = TimerConfig [timer_num].channel;
  
  SET_OUTPUT(pin);
  tone_pin = pin;
  pmc_set_writeprotect(false);
  pmc_enable_periph_clk((uint32_t)irq);
  
  TC_Configure(tc, channel, TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC | 
               TC_CMR_TCCLKS_TIMER_CLOCK4);  // TIMER_CLOCK4 -> 128 divisor
  uint32_t rc = VARIANT_MCK / 128 / frequency; 
  TC_SetRA(tc, channel, rc/2);                     // 50% duty cycle
  TC_SetRC(tc, channel, rc);
  TC_Start(tc, channel);
  tc->TC_CHANNEL[channel].TC_IER=TC_IER_CPCS;
  tc->TC_CHANNEL[channel].TC_IDR=~TC_IER_CPCS;
  NVIC_EnableIRQ((IRQn_Type)irq);
}

void noTone(uint8_t pin) {
  uint8_t timer_num = BEEPER_TIMER_NUM;
  
  Tc *tc = TimerConfig [timer_num].pTimerRegs;
  uint32_t channel = TimerConfig [timer_num].channel;
  
  TC_Stop(tc, channel); 
  WRITE_VAR(pin, LOW);
}


// IRQ handler for tone generator
HAL_BEEPER_TIMER_ISR {
    static bool toggle;

    HAL_timer_isr_status(BEEPER_TIMER_COUNTER, BEEPER_TIMER_CHANNEL);
    WRITE_VAR(tone_pin, toggle);
    toggle = !toggle;
}

// --------------------------------------------------------------------------
// ADC
// --------------------------------------------------------------------------
static inline uint32_t mapRes(uint32_t value, uint32_t from, uint32_t to) {
  if (from == to)
    return value;
  if (from > to)
    return value >> (from-to);
  else
    return value << (to-from);
}
uint16_t getAdcReading(adc_channel_num_t chan)
{
	if ((ADC->ADC_ISR & BIT(chan)) == BIT(chan)) {
		uint16_t rslt = ADC->ADC_CDR[chan];
		ADC->ADC_CHDR |= BIT(chan);
    //rslt = mapRes(rslt, 12, 10);
		return rslt;
	}
	else {
		SERIAL_ECHOLN("error getAdcReading");
		return 0;
	}
}

void startAdcConversion(adc_channel_num_t chan)
{
  ADC->ADC_CHER |= BIT(chan);
}

// Convert an Arduino Due pin number to the corresponding ADC channel number
adc_channel_num_t pinToAdcChannel(int pin)
{
  if (pin < A0)
  {
    pin += A0;
  }
  return (adc_channel_num_t) (int) g_APinDescription[pin].ulADCChannelNumber;
}

uint16_t getAdcFreerun(adc_channel_num_t chan, bool wait_for_conversion)
{
  if (wait_for_conversion) while (!((ADC->ADC_ISR & BIT(chan)) == BIT(chan)));
  if ((ADC->ADC_ISR & BIT(chan)) == BIT(chan)) {
	  uint16_t rslt = ADC->ADC_CDR[chan];
    //rslt = mapRes(rslt, 12, 10);
	  return rslt;
  }
  else {
	  SERIAL_ECHOLN("wait freerun");
  	  return 0;
  }
}

uint16_t getAdcSuperSample(adc_channel_num_t chan) {
	uint16_t rslt = 0;
	for (int i = 0; i < 8; i++) rslt += getAdcFreerun(chan, true);
	return rslt /4 ;
}

void stopAdcFreerun(adc_channel_num_t chan)
{
  ADC->ADC_CHDR |= BIT(chan);
}

// --------------------------------------------------------------------------
//! @brief
//! @param[in]
//! @param[out]
//! @return
// --------------------------------------------------------------------------
