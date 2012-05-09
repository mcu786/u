/*
 * Pressure measurement unit
 * ������������ ��������� �������� �� ����� ������ � MPX5100
 */
#include "ch.h"
#include "hal.h"

#define ARM_MATH_CM4
#include "arm_math.h"

#include "sensors.h"
#include "utils.h"

/*
 ******************************************************************************
 * DEFINES
 ******************************************************************************
 */

/*
 ******************************************************************************
 * EXTERNS
 ******************************************************************************
 */
extern CompensatedData comp_data;

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

/** ���������������� ����
 * ��������� ����� �������� � ������� � ����������� � �������� �������*/
static uint16_t zerocomp(uint16_t raw, int32_t t){

  putinrange(t, -10, 40);

  int32_t c1 = -9;
  int32_t c2 = 408;
  int32_t c3 = 7587;
  int32_t c4 = 60011;
  int32_t zero = (c1*t*t*t + c2*t*t + c3*t + c4) / 1000;

  if (zero >= raw)
    return 0;
  else
    return raw - zero;
}

/* ��������� ����� �������� � �������
 * ���������� ��������� �������� � �/�
 *
 * ��� ��������� ����������� �������� ���� 0.201 V, ���������� �������� 0.183 V,
 * �� ������ ��������� 0.167 V
 * �� ���������� - 9.277777
 */
#define KU    928     //��*100
#define Radc  122070  //uV*100 (���������������� ��� 5.0/4096 ����� �� �������)
#define Smpx  450     //(���������������� ������� 450uV/Pa)

float calc_air_speed(uint16_t press_diff_raw){
  uint16_t p;
  p = zerocomp(press_diff_raw, (int32_t)comp_data.temp_onboard);
  p = ((p * Radc) / Smpx) / KU; /* �������� � �������� */
  return sqrtf(2*p / 1.2);
}




