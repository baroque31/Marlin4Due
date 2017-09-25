#include "Configuration.h"
#include "pins.h"
#include "macros.h"
#include "HAL.h"

#if ENABLED(HAVE_TMC2130_DRIVERS)
#include <SPI.h>

void tmc2130_write(uint8_t chipselect, uint8_t address,uint8_t wval1,uint8_t wval2,uint8_t wval3,uint8_t wval4)
{
  uint32_t val32;
  uint8_t val0;
  uint8_t val1;
  uint8_t val2;
  uint8_t val3;
  uint8_t val4;

  //datagram1 - write
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE3));
  digitalWrite(chipselect,LOW);
  SPI.transfer(address+0x80);
  SPI.transfer(wval1);
  SPI.transfer(wval2);
  SPI.transfer(wval3);
  SPI.transfer(wval4);
  digitalWrite(chipselect, HIGH);
  SPI.endTransaction();

  //datagram2 - response
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE3));
  digitalWrite(chipselect,LOW);
  val0 = SPI.transfer(0);
  val1 = SPI.transfer(0);
  val2 = SPI.transfer(0);
  val3 = SPI.transfer(0);
  val4 = SPI.transfer(0);
  digitalWrite(chipselect, HIGH);
  SPI.endTransaction();

  MYSERIAL.print("WriteRead 0x");
  MYSERIAL.print(address,HEX);
  MYSERIAL.print(" Status:");
  MYSERIAL.print(val0 & 0b00000111,BIN);
  MYSERIAL.print("  ");
  MYSERIAL.print(val1,BIN);
  MYSERIAL.print("  ");
  MYSERIAL.print(val2,BIN);
  MYSERIAL.print("  ");
  MYSERIAL.print(val3,BIN);
  MYSERIAL.print("  ");
  MYSERIAL.print(val4,BIN);

  val32 = (uint32_t)val1<<24 | (uint32_t)val2<<16 | (uint32_t)val3<<8 | (uint32_t)val4;
  MYSERIAL.print(" 0x");
  MYSERIAL.println(val32,HEX);
}

void tmc2130_chopconf(uint8_t cs, bool extrapolate256 = 1, uint16_t microstep_resolution = 16)
{
  uint8_t mres=0b0100;
  if(microstep_resolution == 256) mres = 0b0000;
  if(microstep_resolution == 128) mres = 0b0001;
  if(microstep_resolution == 64)  mres = 0b0010;
  if(microstep_resolution == 32)  mres = 0b0011;
  if(microstep_resolution == 16)  mres = 0b0100;
  if(microstep_resolution == 8)   mres = 0b0101;
  if(microstep_resolution == 4)   mres = 0b0110;
  if(microstep_resolution == 2)   mres = 0b0111;
  if(microstep_resolution == 1)   mres = 0b1000;

  mres |= extrapolate256 << 4; //bit28 intpol

  tmc2130_write(cs,0x6C,mres,1,00,0xC5);
}

#endif //ENABLED(HAVE_TMC2130_DRIVERS)

void tmc2130_init2()
{
  #ifdef HAVE_TMC2130_DRIVERS
  /*
  uint8_t cs[4] = { X_TMC2130_CS, Y_TMC2130_CS, Z_TMC2130_CS, E0_TMC2130_CS };
  uint8_t current[4] = { 23, 23, 23, 23 };

  digitalWrite(X_TMC2130_CS, HIGH);
  digitalWrite(Y_TMC2130_CS, HIGH);
  digitalWrite(Z_TMC2130_CS, HIGH);
  digitalWrite(E0_TMC2130_CS, HIGH);
  pinMode(X_TMC2130_CS,OUTPUT);
  pinMode(Y_TMC2130_CS,OUTPUT);
  pinMode(Z_TMC2130_CS,OUTPUT);
  pinMode(E0_TMC2130_CS,OUTPUT);
  */
  SPI.begin();
  uint8_t CURRENT0 = 17;
  uint8_t CS0 = 26; //PIN_SPI_SS0;
  digitalWrite(CS0,HIGH);
  pinMode(CS0,OUTPUT);

  tmc2130_chopconf(CS0,1,16); // 16 Microstepping to 256 microstepping
  tmc2130_write(CS0,0x10,0,15,CURRENT0,CURRENT0); //0x10 IHOLD_IRUN
  tmc2130_write(CS0,0x0,0,0,0,0b100); //address=0x0 GCONF EXT VREF - STEALTH CHOP
  tmc2130_write(CS0,0x70,0,0b111,0x01,0xC8); //address=0x70 PWM_CONF //reset default=0x00050480
/*
  for(int i=0;i<4;i++)
  {
    //tmc2130_write(cs[i],0x6C,0b10100,01,00,0xC5);
    tmc2130_chopconf(cs[i],1,16); // 16 Microstepping to 256 microstepping
    tmc2130_write(cs[i],0x10,0,15,current[i],current[i]); //0x10 IHOLD_IRUN
    tmc2130_write(cs[i],0x0,0,0,0,0b101); //address=0x0 GCONF EXT VREF - STEALTH CHOP
    //tmc2130_write(cs[i],0x11,0,0,0,0xA);
    tmc2130_write(cs[i],0x70,0,0b111,0x01,0xC8); //address=0x70 PWM_CONF //reset default=0x00050480
  }
  */
  #endif
}
bool tmc2130_en_state[4] = {1};
void tmc2130_init()
{
  #ifdef HAVE_TMC2130_DRIVERS
  uint8_t cs[4] = { X_TMC2130_CS, Y_TMC2130_CS, Z_TMC2130_CS, E0_TMC2130_CS };
  //uint8_t current[4] = { 23, 23, 23, 23 };
  //uint8_t current[4] = { 18, 18, 18, 18 }; //108C drivers on eval board. 78C on Y motor.
  //uint8_t current[4] = { 16, 16, 16, 13 }; //motors too hot
  uint8_t current[4] = { 14, 14, 16, 11 };

  //pinMode(PIN_SPI_SS0,OUTPUT);  // PA25
  pinMode(PIN_SPI_MOSI,OUTPUT); // PA10
  pinMode(PIN_SPI_SCK,OUTPUT);  // PB0


  //digitalWrite(X_TMC2130_CS, HIGH);
  //pinMode(X_TMC2130_CS,OUTPUT);
/*
  digitalWrite(Y_TMC2130_CS, HIGH);
  digitalWrite(Z_TMC2130_CS, HIGH);
  digitalWrite(E0_TMC2130_CS, HIGH);
  pinMode(Y_TMC2130_CS,OUTPUT);
  pinMode(Z_TMC2130_CS,OUTPUT);
  pinMode(E0_TMC2130_CS,OUTPUT);
*/
  SPI.end();
  SPI.begin();
/*
  uint8_t CURRENT0 = 17;
  uint8_t CS0 = 26; //PIN_SPI_SS0;
  digitalWrite(CS0,HIGH);
  pinMode(CS0,OUTPUT);
  
  tmc2130_chopconf(CS0,1,16); // 16 Microstepping to 256 microstepping
  tmc2130_write(CS0,0x10,0,15,CURRENT0,8); //0x10 IHOLD_IRUN
  tmc2130_write(CS0,0x0,0,0,0,0b000); //address=0x0 GCONF EXT VREF - STEALTH CHOP
  tmc2130_write(CS0,0x70,0,0b111,0x01,0xC8); //address=0x70 PWM_CONF //reset default=0x00050480
  */
  for(int i=0;i<4;i++) {
    digitalWrite(cs[i],HIGH);
    pinMode(cs[i],OUTPUT);
  }

  for(int i=0;i<4;i++) //i<4
  {
    //tmc2130_write(cs[i],0x6C,0b10100,01,00,0xC5);
    //tmc2130_chopconf(cs[i],1,16); // 16 Microstepping to 256 microstepping
    tmc2130_write(cs[i],0x6C,0,01,0,0); // START WITH MOTORS DISABLE (NO ENABLE PIN USED)
    if(i==2) { //Z SETTINGS
      tmc2130_write(cs[i],0x10,0,15,current[i],current[i]-4); //0x10 IHOLD_IRUN
      tmc2130_write(cs[i],0x0,0,0,0,0b000); //address=0x0 GCONF EXT VREF - STEALTH CHOP
    }
    else {
      tmc2130_write(cs[i],0x10,0,15,current[i],7); //0x10 IHOLD_IRUN
      tmc2130_write(cs[i],0x0,0,0,0,0b100); //address=0x0 GCONF EXT VREF - STEALTH CHOP
    }
    //tmc2130_write(cs[i],0x11,0,0,0,0xA);
    tmc2130_write(cs[i],0x70,0,0b100,0x01,0xC8); //address=0x70 PWM_CONF //reset default=0x00050480

    tmc2130_en_state[i] = false;
  }
  
  #endif //HAVE_TMC2130_DRIVERS
}

#if ENABLED(HAVE_TMC2130_DRIVERS)

void tmc2130_disable_motors()
{
  uint8_t cs[4] = { X_TMC2130_CS, Y_TMC2130_CS, Z_TMC2130_CS, E0_TMC2130_CS };
  
  tmc2130_init();
  for(int i=0;i<4;i++) {
    tmc2130_write(cs[i],0x6C,0,01,0,0);
    tmc2130_en_state[i] = 0;
  }
}

void tmc2130_disable_motor(uint8_t motor)
{
  uint8_t cs[4] = { X_TMC2130_CS, Y_TMC2130_CS, Z_TMC2130_CS, E0_TMC2130_CS };
  
  //tmc2130_init();
  //for(int i=0;i<4;i++)
  if(tmc2130_en_state[motor]) {
    tmc2130_write(cs[motor],0x6C,0,01,0,0);
    tmc2130_en_state[motor] = 0;
  }
}

void tmc2130_enable_motor(uint8_t motor)
{
  uint8_t cs[4] = { X_TMC2130_CS, Y_TMC2130_CS, Z_TMC2130_CS, E0_TMC2130_CS };
  
  //tmc2130_init();
  //for(int i=0;i<4;i++)
  if(!tmc2130_en_state[motor]) { //if not already enabled
    tmc2130_chopconf(cs[motor],1,32); // 16 Microstepping to 256 microstepping
    tmc2130_en_state[motor] = 1;
    //HAL_timer_set_count (STEP_TIMER_COUNTER, STEP_TIMER_CHANNEL, HAL_TIMER_RATE / 1000); //1ms wait
    HAL_timer_set_count (STEP_TIMER_NUM, HAL_TIMER_RATE / 1000);
  }
}

#endif //HAVE_TMC2130_DRIVERS
