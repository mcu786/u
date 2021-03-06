/*                      WARNINGS!
 *
 * cmpilation without -fomit-frame-pointer cause stack overflows.
 */

// TODO: RTC timezones.
// TODO: ADC. In callback functions ticks counter. When got 32 samples - pass them to FIR.
// TODO: Magnetometer fusion with DCM not accelerometer.
// TODO: (semi)automated zeroing of magnetometer and accel.
// TODO: Rewrite messaging holy crap.
// TODO: Power brown out handler.
// TODO: Events on differnt subsystems failures (gyro_failed, gps_failed, etc.)
// TODO: One more point in pressure thermal compensation algorith (at 60 celsius)
// TODO: Rewrite XBee code for use DMA.
// TODO: WDT with backup domain for fuckups investigation.

#include "ch.h"
#include "hal.h"

#include "main.h"
#include "param.h"
#include "sanity.h"
#include "irq_storm.h"
#include "i2c_local.h"
#include "timekeeping.h"
#include "linkmgr.h"
#include "gps.h"
#include "servo.h"
#include "message.h"
#include "sensors.h"
#include "autopilot.h"
#include "eeprom.h"
#include "exti_local.h"
#include "microsd.h"
#include "tlm_sender.h"

#include "arm_math.h"

/*
 ******************************************************************************
 * EXTERNS
 ******************************************************************************
 */
/* RTC-GPS sync */
BinarySemaphore rtc_sem;

/* store here time from GPS */
struct tm gps_timp;

/* some global flags (deprecated, use events) */
uint32_t GlobalFlags = 0;

/* EEPROM "file" */
EepromFileStream EepromFile;

/* heap for link threads OR shell thread */
MemoryHeap LinkThdHeap;
static uint8_t link_thd_buf[LINK_THD_HEAP_SIZE + sizeof(stkalign_t)];

/* primitive "init system" */
EventSource init_event;

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */

/*
 *******************************************************************************
 *******************************************************************************
 * LOCAL FUNCTIONS
 *******************************************************************************
 *******************************************************************************
 */
//static TimeMeasurement tmup;
//static volatile float x = 0;
//static volatile uint32_t y = 0;
//static volatile uint32_t n = 1000000;
//static volatile uint32_t imu_update_period = 0;

int main(void) {
  halInit();
  chSysInit();

//  tmObjectInit(&tmup);
//  tmStartMeasurement(&tmup);
//  x = arm_cos_f32(PI/3.0);
//  tmStopMeasurement(&tmup);
//  imu_update_period = tmup.last;
//
//  imu_update_period = 0;
//  tmStartMeasurement(&tmup);
//  for (;n >0; n--)
//    y = arm_cos_q31(n);
//  tmStopMeasurement(&tmup);
//  imu_update_period = tmup.last;

  chBSemInit(&rtc_sem, TRUE);

  chEvtInit(&init_event);

  chThdSleepMilliseconds(100);

  /* give power to all needys */
  pwr5v_power_on();
  gps_power_on();
  xbee_reset_clear();
  xbee_sleep_clear();

  chHeapInit(&LinkThdHeap, (uint8_t *)MEM_ALIGN_NEXT(link_thd_buf), LINK_THD_HEAP_SIZE);

  EepromOpen(&EepromFile);

  LinkMgrInit();
  MsgInit();
  SanityControlInit();
  TimekeepingInit();
  I2CInitLocal(); /* also starts EEPROM and load global parameters from it */
  MavInit();      /* mavlink constants initialization must be called after I2C init */
  SensorsInit();  /* sensors use I2C */
  TlmSenderInit();
  ServoInit();
  AutopilotInit();  /* autopilot must be started only after servos */
  StorageInit();

  #if ENABLE_IRQ_STORM
    chThdSleepMilliseconds(5000);
    IRQStormInit();
  #endif /* ENABLE_IRQ_STORM */

  while (TRUE){
    chThdSleepMilliseconds(666);
  }

  return 0;
}

