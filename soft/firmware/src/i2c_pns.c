#include "ch.h"
#include "hal.h"

#include "i2c_pns.h"
#include "main.h"

#include "eeprom.h"
#include "itg3200.h"
#include "mma8451.h"
#include "tmp75.h"
#include "max1236.h"
#include "bmp085.h"
#include "mag3110.h"


/* interface #2 */
static const I2CConfig i2cfg2 = {
    OPMODE_I2C,
    400000, //100000, //
    FAST_DUTY_CYCLE_16_9, //STD_DUTY_CYCLE, //
};


void I2CInit_pns(void){

  i2cStart(&I2CD2, &i2cfg2);

  chThdSleepMilliseconds(25); /* wait untill all devices ready */

  /* startups */
//  init_eeprom();
  init_tmp75();
//  init_max1236();
//  init_mag3110();
//  init_itg3200();
//  init_mma8451();
//  init_bmp085();
}


/* ������� ���������� ���������� */
msg_t i2c_transmit(I2CDriver *i2cp,
                  i2caddr_t addr,
                  const uint8_t *txbuf,
                  size_t txbytes,
                  uint8_t *rxbuf,
                  size_t rxbytes){
  msg_t status = RDY_OK;

  i2cAcquireBus(i2cp);
  status = i2cMasterTransmitTimeout(i2cp, addr, txbuf, txbytes, rxbuf, rxbytes, MS2ST(5));
  i2cReleaseBus(i2cp);
  //chDbgAssert(status == RDY_OK, "i2c_transmit(), #1", "error in driver");
  if (status == RDY_TIMEOUT){
    /* � ������ �������� ���������� ������������� ������� */
    i2cStart(i2cp, &i2cfg2);
    return status;
  }
  return status;
}

/* ������� ���������� ���������� */
msg_t i2c_receive(I2CDriver *i2cp,
                  i2caddr_t addr,
                  uint8_t *rxbuf,
                  size_t rxbytes){
  msg_t status = RDY_OK;

  i2cAcquireBus(i2cp);
  status = i2cMasterReceiveTimeout(i2cp, addr, rxbuf, rxbytes, MS2ST(3));
  i2cReleaseBus(i2cp);
  chDbgAssert(status == RDY_OK, "i2c_transmit(), #1", "error in driver");
  if (status == RDY_TIMEOUT){
    /* � ������ �������� ���������� ������������� ������� */
    i2cStart(i2cp, &i2cfg2);
    return status;
  }
  return status;
}