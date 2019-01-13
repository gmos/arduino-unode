// 13/02/2019.  Gijs Mos.  Handling of pin states now conform Arduino "wiring" standard.
// Returned pin logic values are now either HIGH or LOW.  But for setting HIGH = non-zero = true
// and LOW = 0 = false.

#include <inttypes.h>

#include <Arduino.h>
#include <SPI.h>

#include "mcp23s08.h"

mcp23s08::mcp23s08(){
#if defined (SPI_HAS_TRANSACTION)
  _spiTransactionsSpeed = MAXSPISPEED;//set to max supported speed (in relation to chip and CPU)
#else
  _spiTransactionsSpeed = 0;
#endif
}

void mcp23s08::setSPIspeed(uint32_t spispeed){
  #if defined (SPI_HAS_TRANSACTION)
  if (spispeed > 0){
    if (spispeed > MAXSPISPEED) {
      _spiTransactionsSpeed = MAXSPISPEED;
    } else {
      _spiTransactionsSpeed = spispeed;
    }
  } else {
    _spiTransactionsSpeed = 0;//disable SPItransactons
  }
  #else
  _spiTransactionsSpeed = 0;
  #endif
}

mcp23s08::mcp23s08(const uint8_t csPin,const uint8_t haenAdrs){
  _spiTransactionsSpeed = 0;
  postSetup(csPin,haenAdrs);
}

mcp23s08::mcp23s08(const uint8_t csPin,const uint8_t haenAdrs,uint32_t spispeed){
  postSetup(csPin,haenAdrs,spispeed);
}


void mcp23s08::postSetup(const uint8_t csPin,const uint8_t haenAdrs,uint32_t spispeed){
  #if defined (SPI_HAS_TRANSACTION)
    if (spispeed > 0) setSPIspeed(spispeed);
  #endif
  _cs = csPin;
  if (haenAdrs >= 0x20 && haenAdrs <= 0x23){//HAEN works between 0x20...0x23
    _adrs = haenAdrs;
    _useHaen = 1;
  } else {
    _adrs = 0;
    _useHaen = 0;
  }
  _readCmd =  (_adrs << 1) | 1;
  _writeCmd = _adrs << 1;
  //setup register values for this chip
  IOCON =   0x05;
  IODIR =   0x00;
  GPPU =    0x06;
  GPIO =    0x09;
  GPINTEN =   0x02;
  IPOL =    0x01;
  DEFVAL =  0x03;
  INTF =    0x07;
  INTCAP =  0x08;
  OLAT =    0x0A;
  INTCON =  0x04;
}

void mcp23s08::begin(bool protocolInitOverride) {
  if (!protocolInitOverride){
    SPI.begin();
    #if defined (SPI_HAS_TRANSACTION)
    if (_spiTransactionsSpeed == 0){//do not use SPItransactons
      SPI.setClockDivider(SPI_CLOCK_DIV4); // 4 MHz (half speed)
      SPI.setBitOrder(MSBFIRST);
      SPI.setDataMode(SPI_MODE0);
    }
    #else//do not use SPItransactons
    SPI.setClockDivider(SPI_CLOCK_DIV4); // 4 MHz (half speed)
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);
    #endif
  }
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);
  delay(100);
  _useHaen == 1 ? writeByte(IOCON,0b00101000) : writeByte(IOCON,0b00100000);
  /*
    if (_useHaen){
    writeByte(IOCON,0b00101000);//read datasheet for details!
  } else {
    writeByte(IOCON,0b00100000);
  }
  */
  _gpioDirection = 0xFF; //all in
  _gpioState = 0x00; //all low
  _gpioPullup = 0x00; //no pullup
}





uint8_t mcp23s08::readAddress(byte addr){
  byte low_byte = 0x00;
  startSend(1);
  SPI.transfer(addr);
  low_byte  = SPI.transfer(0x0);
  endSend();
  return low_byte;
}



void mcp23s08::gpioPinMode(uint8_t mode){
  if (mode == INPUT){
    _gpioDirection = 0xFF;
  } else if (mode == OUTPUT){
    _gpioDirection = 0x00;
    _gpioState = 0x00;
  } else {
    _gpioDirection = mode;
  }
  writeByte(IODIR,_gpioDirection);
}

void mcp23s08::gpioPinMode(uint8_t pin, uint8_t mode){
  uint8_t pin_mask = 1 << pin;

  if (pin < 8) { //0...7

    // Update input/output bit
    (mode == OUTPUT) ? _gpioDirection &= ~pin_mask : _gpioDirection |= pin_mask;
    writeByte(IODIR, _gpioDirection);

    // Update pull-up state
    if (mode == INPUT_PULLUP) {
      if ((_gpioPullup & pin_mask) == 0) {
        _gpioPullup |= pin_mask;
        writeByte(GPPU, _gpioPullup);
      }
    } else {
      if ((_gpioPullup & pin_mask) != 0) {
        _gpioPullup &= ~pin_mask;
        writeByte(GPPU, _gpioPullup);
      }
    }
  }
}

void mcp23s08::gpioPort(uint8_t value){
  if (value == HIGH){
    _gpioState = 0xFF;
  } else if (value == LOW){
    _gpioState = 0x00;
  } else {
    _gpioState = value;
  }
  writeByte(GPIO,_gpioState);
}


uint8_t mcp23s08::readGpioPort(){
  return readAddress(GPIO);
}

uint8_t mcp23s08::readGpioPortFast(){
  return _gpioState;
}

int mcp23s08::gpioDigitalReadFast(uint8_t pin){
  if (pin < 8){//0...7
    int temp = bitRead(_gpioState,pin);
    return temp;
  } else {
    return LOW;
  }
}

void mcp23s08::portPullup(uint8_t data) {
  if (data == HIGH){
    _gpioState = 0xFF;
    _gpioPullup = 0xFF;
  } else if (data == LOW){
    _gpioState = 0x00;
    _gpioPullup = 0x00;
  } else {
    _gpioState = data;
    _gpioPullup = data;
  }
  writeByte(GPPU, _gpioPullup);
}




void mcp23s08::gpioDigitalWrite(uint8_t pin, uint8_t value){
  if (pin < 8){//0...7
    value  ? _gpioState |= (1 << pin) : _gpioState &= ~(1 << pin);
    writeByte(GPIO, _gpioState);
  }
}

void mcp23s08::gpioDigitalWriteFast(uint8_t pin, uint8_t value){
  if (pin < 8){//0...8
    value ? _gpioState |= (1 << pin) : _gpioState &= ~(1 << pin);
  }
}

void mcp23s08::gpioPortUpdate(){
  writeByte(GPIO, _gpioState);
}

int mcp23s08::gpioDigitalRead(uint8_t pin){
  if (pin < 8) return ((readAddress(GPIO) & (1 << pin) ? HIGH : LOW ));
  return LOW;
}

uint8_t mcp23s08::gpioRegisterReadByte(byte reg){
  uint8_t data = 0;
    startSend(1);
    SPI.transfer(reg);
    data = SPI.transfer(0);
    endSend();
  return data;
}


void mcp23s08::gpioRegisterWriteByte(byte reg,byte data){
  writeByte(reg,(byte)data);
}

/* ------------------------------ Low Level ----------------*/
void mcp23s08::startSend(bool mode){
#if defined (SPI_HAS_TRANSACTION)
  if (_spiTransactionsSpeed > 0) SPI.beginTransaction(SPISettings(_spiTransactionsSpeed, MSBFIRST, SPI_MODE0));
#endif
#if defined(__FASTWRITE)
  digitalWriteFast(_cs, LOW);
#else
  digitalWrite(_cs, LOW);
#endif
  mode == 1 ? SPI.transfer(_readCmd) : SPI.transfer(_writeCmd);
}

void mcp23s08::endSend(){
#if defined(__FASTWRITE)
  digitalWriteFast(_cs, HIGH);
#else
  digitalWrite(_cs, HIGH);
#endif
#if defined (SPI_HAS_TRANSACTION)
  if (_spiTransactionsSpeed > 0) SPI.endTransaction();
#endif
}


void mcp23s08::writeByte(byte addr, byte data){
  startSend(0);
  SPI.transfer(addr);
  SPI.transfer(data);
  endSend();
}


