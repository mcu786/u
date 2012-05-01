#include <stdlib.h>
#include <math.h>

#include "ch.h"
#include "hal.h"

#include "i2c_pns.h"
#include "utils.h"
#include "imu.h"
#include "itg3200.h"
#include "message.h"
#include "param.h"
#include "main.h"
#include "link.h"

/*
 ******************************************************************************
 * DEFINES
 ******************************************************************************
 */
#define itg3200addr   0b1101000
#define PI            3.14159265f

#define XPOL          (global_data[xpol_index].value)
#define YPOL          (global_data[ypol_index].value)
#define ZPOL          (global_data[zpol_index].value)

#define XSENS         (global_data[xsens_index].value)
#define YSENS         (global_data[ysens_index].value)
#define ZSENS         (global_data[zsens_index].value)

//#define AVG_SAMPLES_CNT  (global_data[samplescnt_index].value)

/*
 ******************************************************************************
 * EXTERNS
 ******************************************************************************
 */
extern uint32_t GlobalFlags;

extern RawData raw_data;
extern CompensatedData comp_data;
extern BinarySemaphore itg3200_sem;
extern BinarySemaphore imu_sem;
extern GlobalParam_t global_data[];
extern uint32_t itg3200_period;
extern EventSource pwrmgmt_event;
extern mavlink_system_t mavlink_system;

extern mavlink_raw_imu_t mavlink_raw_imu_struct;
extern mavlink_scaled_imu_t mavlink_scaled_imu_struct;

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */
static uint8_t rxbuf[GYRO_RX_DEPTH];
static uint8_t txbuf[GYRO_TX_DEPTH];

// ������� ��� �������� �����
static uint32_t zero_cnt = 0;

/* ������� � ��������� � ����������� */
static uint32_t xsens_index, ysens_index, zsens_index;
static uint32_t xpol_index,  ypol_index,  zpol_index;
static uint32_t samplescnt_index;
static uint32_t awg_samplescnt;

/*
 *******************************************************************************
 *******************************************************************************
 * LOCAL FUNCTIONS
 *******************************************************************************
 *******************************************************************************
 */

/**
 * ����������� �������� �����.
 */
void gyrozeroing(void){
  if (zero_cnt > 0){
    raw_data.xgyro_zero += raw_data.xgyro;
    raw_data.ygyro_zero += raw_data.ygyro;
    raw_data.zgyro_zero += raw_data.zgyro;
    zero_cnt--;
    return;
  }
  else{
    clearGlobalFlag(GYRO_CAL);
    mavlink_system.state = MAV_STATE_STANDBY;
  }
}

/**
 * �������� �� ����� �������� � ���/���
 */
static float calc_gyro_rate(int32_t raw, float sens){
  float tmp = (float)raw;
  tmp /= (float)awg_samplescnt;
  tmp /= sens;
  tmp *= (PI / 180.0);
  return tmp;
}

/**
 * ��������� ���������� ���� ������ �� ������� �������� � �������� ����� ���������
 */
static float get_degrees(float raw){
  float t = (float)itg3200_period / 1000000.0;
  return raw * ((t * 180) / PI);
}

/**
 * ����� ��� ������ ���������
 */
static WORKING_AREA(PollGyroThreadWA, 512);
static msg_t PollGyroThread(void *arg){
  chRegSetThreadName("PollGyro");
  (void)arg;

  int32_t gyroX, gyroY, gyroZ;
  msg_t sem_status = RDY_OK;

  struct EventListener self_el;
  chEvtRegister(&pwrmgmt_event, &self_el, PWRMGMT_SIGHALT_EVID);

  while (TRUE) {
    sem_status = chBSemWaitTimeout(&itg3200_sem, MS2ST(20));

    txbuf[0] = GYRO_OUT_DATA;     // register address
    if ((i2c_transmit(itg3200addr, txbuf, 1, rxbuf, 8) == RDY_OK) && (sem_status == RDY_OK)){
      raw_data.gyro_temp  = complement2signed(rxbuf[0], rxbuf[1]);
      raw_data.xgyro      = complement2signed(rxbuf[2], rxbuf[3]);
      raw_data.ygyro      = complement2signed(rxbuf[4], rxbuf[5]);
      raw_data.zgyro      = complement2signed(rxbuf[6], rxbuf[7]);

      if (GlobalFlags & GYRO_CAL)
        gyrozeroing();
      else{
        /* correct placement (we need to swap just x and y axis) and advance to zero offset */
        gyroX = ((int32_t)raw_data.ygyro) * awg_samplescnt - raw_data.ygyro_zero;
        gyroY = ((int32_t)raw_data.xgyro) * awg_samplescnt - raw_data.xgyro_zero;
        gyroZ = ((int32_t)raw_data.zgyro) * awg_samplescnt - raw_data.zgyro_zero;

        /* adjust rotation direction */
        gyroX *= XPOL;
        gyroY *= YPOL;
        gyroZ *= ZPOL;

        /* fill debug struct */
        mavlink_raw_imu_struct.xgyro = gyroX;
        mavlink_raw_imu_struct.ygyro = gyroY;
        mavlink_raw_imu_struct.zgyro = gyroZ;

        /* now get angular velocity in rad/sec */
        comp_data.xgyro = calc_gyro_rate(gyroX, XSENS);
        comp_data.ygyro = calc_gyro_rate(gyroY, YSENS);
        comp_data.zgyro = calc_gyro_rate(gyroZ, ZSENS);

        /* calc summary angle for debug purpose */
        comp_data.xgyro_angle += get_degrees(comp_data.xgyro);
        comp_data.ygyro_angle += get_degrees(comp_data.ygyro);
        comp_data.zgyro_angle += get_degrees(comp_data.zgyro);

        /* fill scaled debug struct */
        mavlink_scaled_imu_struct.xgyro = (int16_t)(1000 * comp_data.xgyro);
        mavlink_scaled_imu_struct.ygyro = (int16_t)(1000 * comp_data.ygyro);
        mavlink_scaled_imu_struct.zgyro = (int16_t)(1000 * comp_data.zgyro);

        /* say to IMU "we have fresh data "*/
        chBSemSignal(&imu_sem);
      }
    }
    else{
      //TODO: event GyroFail
      /* ��������, ��������������� � ���� */
      raw_data.gyro_temp = -32768;
      raw_data.xgyro = -32768;
      raw_data.ygyro = -32768;
      raw_data.zgyro = -32768;
    }

    if (chThdSelf()->p_epending & EVENT_MASK(PWRMGMT_SIGHALT_EVID))
      chThdExit(RDY_OK);
  }
  return 0;
}


/**
 *  perform searching of indexes
 */
static void search_indexes(void){
  int32_t i = -1;

  i = KeyValueSearch("GYRO_xsens");
  if (i == -1)
    chDbgPanic("key not found");
  else
    xsens_index = i;

  i = KeyValueSearch("GYRO_ysens");
  if (i == -1)
    chDbgPanic("key not found");
  else
    ysens_index = i;

  i = KeyValueSearch("GYRO_zsens");
  if (i == -1)
    chDbgPanic("key not found");
  else
    zsens_index = i;

  i = KeyValueSearch("GYRO_xpol");
  if (i == -1)
    chDbgPanic("key not found");
  else
    xpol_index = i;

  i = KeyValueSearch("GYRO_ypol");
  if (i == -1)
    chDbgPanic("key not found");
  else
    ypol_index = i;

  i = KeyValueSearch("GYRO_zpol");
  if (i == -1)
    chDbgPanic("key not found");
  else
    zpol_index = i;

  i = KeyValueSearch("GYRO_zeroconut");
  if (i == -1)
    chDbgPanic("key not found");
  else{
    samplescnt_index = i;
    awg_samplescnt = global_data[samplescnt_index].value;
  }
}

/*
 *******************************************************************************
 * EXPORTED FUNCTIONS
 *******************************************************************************
 */

/**
 *
 */
void init_itg3200(void){
  int32_t i = -1;

  search_indexes();

  #if CH_DBG_ENABLE_ASSERTS
    // clear bufers. Just to be safe.
    i = 0;
    for (i = 0; i < GYRO_TX_DEPTH; i++){txbuf[i] = 0x55;}
    for (i = 0; i < GYRO_RX_DEPTH; i++){rxbuf[i] = 0x55;}
  #endif

  txbuf[0] = GYRO_PWR_MGMT;
  txbuf[1] = 0b1000000; /* soft reset */
  while (i2c_transmit(itg3200addr, txbuf, 2, rxbuf, 0) != RDY_OK)
    ;
  chThdSleepMilliseconds(55);

  txbuf[0] = GYRO_PWR_MGMT;
  txbuf[1] = 1; /* select clock source */
  while (i2c_transmit(itg3200addr, txbuf, 2, rxbuf, 0) != RDY_OK)
    ;
  chThdSleepMilliseconds(2);

  txbuf[0] = GYRO_SMPLRT_DIV;
  txbuf[1] = 9; /* sample rate. Approximatelly (1000 / (9 + 1)) = 100Hz*/
  txbuf[2] = GYRO_DLPF_CFG | GYRO_FS_SEL; /* �������� ��������� � ������� ����� ����������� ������� */
  txbuf[3] = 0b110001; /* configure and enable interrupts */
  while (i2c_transmit(itg3200addr, txbuf, 4, rxbuf, 0) != RDY_OK)
    ;

  chThdSleepMilliseconds(2);
  chThdCreateStatic(PollGyroThreadWA,
          sizeof(PollGyroThreadWA),
          I2C_THREADS_PRIO + 2,
          PollGyroThread,
          NULL);
  chThdSleepMilliseconds(2);

  mavlink_system.state = MAV_STATE_CALIBRATING;
  gyro_refresh_zeros();
}

/**
 * ���������� ������������ ���� � �������� ���������� ������.
 */
void gyro_refresh_zeros(void){

  chSysLock();

  raw_data.xgyro_zero = 0;
  raw_data.ygyro_zero = 0;
  raw_data.zgyro_zero = 0;

  zero_cnt = awg_samplescnt;
  GlobalFlags |= GYRO_CAL;

  chSysUnlock();
}

