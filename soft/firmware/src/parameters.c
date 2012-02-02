#include <math.h>
#include <string.h>

#include "ch.h"
#include "hal.h"

#include "message.h"
#include "parameters.h"
#include "main.h"
#include "imu.h"
#include "servo.h"
#include "eeprom_file.h"
#include "link.h"

/*
 ******************************************************************************
 * DEFINES
 ******************************************************************************
 */
#define ONBOARD_PARAM_NAME_LENGTH 15

/*
 ******************************************************************************
 * EXTERNS
 ******************************************************************************
 */
extern Mailbox param_mb;
extern Mailbox tolink_mb;
extern mavlink_system_t mavlink_system;
extern EepromFileStream* EepromFile_p;

GlobalParam_t global_data[] = {
    /*  key             val    min        max         type                       */
    /*-----------------------------------------------------------------------*/
    {"SYS_ID",          20,    1,         255,    MAVLINK_TYPE_UINT32_T},
    /* IMU - inertial measurement unit */
    {"IMU_g1",          0.1,   -1,        1,      MAVLINK_TYPE_FLOAT},
    {"IMU_g2",          0.2,   -1,        1,      MAVLINK_TYPE_FLOAT},
    {"IMU_g3",          0.3,   -1,        1,      MAVLINK_TYPE_FLOAT},
    /* �������� ���� ������������ */
    {"MAG_xoffset",     110,   -5000,     5000,   MAVLINK_TYPE_INT32_T},
    {"MAG_yoffset",     -90,   -5000,     5000,   MAVLINK_TYPE_INT32_T},
    {"MAG_zoffset",     351,   -5000,     5000,   MAVLINK_TYPE_INT32_T},
    /* �������� ���� ������������ */
    {"ACC_xoffset",     2,     -100,      100,    MAVLINK_TYPE_INT32_T},
    {"ACC_yoffset",     0,     -100,      100,    MAVLINK_TYPE_INT32_T},
    {"ACC_zoffset",     -3,    -100,      100,    MAVLINK_TYPE_INT32_T},
    /* PMU - pressure measurement unit */
    {"PMU_D_offset",    3,     -1100,     1024,   MAVLINK_TYPE_INT32_T},   /* dinamic pressure*/
    {"PMU_D_gain",      1048,  0,         1224,   MAVLINK_TYPE_UINT32_T},
    {"PMU_A_offset",    3,     -1000,     1224,   MAVLINK_TYPE_INT32_T},   /* absolute pressure */
    {"PMU_A_gain",      1048,  0,         1224,   MAVLINK_TYPE_UINT32_T},
    /* ADC coefficients */
    {"ADC_I_offset",    1048,  0,         1224,   MAVLINK_TYPE_UINT32_T},  /* �������� ���� ������� ���� */
    {"ADC_I_gain",      1048,  0,         1224,   MAVLINK_TYPE_UINT32_T},  /* �� ������� ���� ��������, ����� �������� ����������� */
    /* Servos coefficients */
    {"SERVO_1_min",     1000,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_1_max",     2000,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_1_neutra",  1500,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_2_min",     1000,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_2_max",     2000,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_2_neutra",  1500,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_3_min",     1000,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_3_max",     2000,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_3_neutra",  1500,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_4_min",     1000,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_4_max",     2000,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_4_neutra",  1500,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_5_min",     1000,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_5_max",     2000,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_5_neutra",  1500,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_6_min",     1000,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_6_max",     2000,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_6_neutra",  1500,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_7_min",     1000,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_7_max",     2000,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_7_neutra",  1500,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_8_min",     1000,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_8_max",     2000,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_8_neutra",  1500,  SERVO_MIN, SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    /* �������-����������� ��������� */
    {"SERVO_car_max",   2000,  0,         SERVO_MAX,  MAVLINK_TYPE_UINT32_T},
    {"SERVO_car_dz",    32,    0,         64,         MAVLINK_TYPE_UINT32_T},

    /* fake field with 14 symbols name */
    {"fake_14_bytes_",  1048,  0,         1224,       MAVLINK_TYPE_FLOAT},
};

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */
static Mailbox param_confirm_mb;
static msg_t param_confirm_mb_buf[1];
static const uint32_t ONBOARD_PARAM_COUNT = (sizeof(global_data) / sizeof(GlobalParam_t));
static uint8_t eeprombuf[(sizeof(global_data) / sizeof(GlobalParam_t)) * sizeof(global_data[0].value)];

/*
 *******************************************************************************
 *******************************************************************************
 * LOCAL FUNCTIONS
 *******************************************************************************
 *******************************************************************************
 */

/**
 * @brief   Performs key-value search.
 *
 * @return      Index in dictionary.
 * @retval -1   key not found.
 */
int32_t key_value_search(char* key, GlobalParam_t *global_data){
  uint32_t i = 0;

  for (i = 0; i < ONBOARD_PARAM_COUNT; i++){
    if (strcmp(key, global_data[i].name) == 0)
      return i;
  }
  return -1;
}

/**
 * Checks parameter and writes it to global struct.
 */
static bool_t set_parameter(mavlink_param_set_t *set){
  int32_t index = -1;

  index = key_value_search(set->param_id, global_data);

  if (index >= 0){
    // Only write and emit changes if there is actually a difference
    // AND only write if new value is NOT "not-a-number"
    // AND is NOT infinity
    if ((set->param_type == MAVLINK_TYPE_FLOAT) &&
        (isnan(set->param_value) || isinf(set->param_value))){
      return FAILED;
    }
    if (global_data[index].value == set->param_value){
      return FAILED;
    }
    /* If value fall out of min..max bound than just silently nearest allowable value */
    if (set->param_value > global_data[index].max){
      global_data[index].value = global_data[index].max;
      return SUCCESS;
    }
    if (set->param_value < global_data[index].min){
      global_data[index].value = global_data[index].min;
      return SUCCESS;
    }

    global_data[index].value = set->param_value;
    return SUCCESS;
  }

  /*error trap*/
  return FAILED;
}

/**
 * �������� �������� ���������� �� EEPROM
 */
static bool_t load_params_from_eeprom(void){
  uint32_t i = 0;

  chFileStreamSeek(EepromFile_p, EEPROM_SETTINGS_START);

  for (i = 0; i < ONBOARD_PARAM_COUNT; i++){
    if (chFileStreamRead(EepromFile_p, eeprombuf, 4) != 4)
      return FAILED;

    global_data[i].value = eeprombuf[0] << 24 | eeprombuf[1] << 16 | eeprombuf[2] << 8 | eeprombuf[3];;
    if (global_data[i].value < global_data[i].min)
      global_data[i].value = global_data[i].min;
    else if (global_data[i].value > global_data[i].max)
      (global_data[i].value = global_data[i].max);
  }
  return 0;
}

/**
 * ���������� �������� ���������� � EEPROM
 */
static bool_t save_params_to_eeprom(void){
  uint32_t i = 0;

  chFileStreamSeek(EepromFile_p, EEPROM_SETTINGS_START);
  for (i = 0; i < ONBOARD_PARAM_COUNT; i++){
    eeprombuf[i+0] = (((uint32_t)(global_data[i].value)) >> 24) & 0xFF;
    eeprombuf[i+1] = (((uint32_t)(global_data[i].value)) >> 16) & 0xFF;
    eeprombuf[i+2] = (((uint32_t)(global_data[i].value)) >> 8)  & 0xFF;
    eeprombuf[i+3] = (((uint32_t)(global_data[i].value)) >> 0)  & 0xFF;
  }
  i = ONBOARD_PARAM_COUNT * sizeof(global_data[0].value);
  chFileStreamWrite(EepromFile_p, eeprombuf, i);
  return 0;
}

/**
 * �������� ������ �� EEPROM
 */
static bool_t load_mission_from_eeprom(void){
  return 0;
}

/**
 * ���������� ������ � EEPROM
 */
static bool_t save_mission_to_eeprom(void){
  return 0;
}

/**
 * @brief   Sends mails to communication thread
 *
 * @param[in] key   if NULL than perform search by index
 * @param[in] n     search index
 */
static bool_t send_value(Mail *param_value_mail,
                         mavlink_param_value_t *param_value_struct,
                         char *key,
                         uint32_t n){
  int32_t index = -1;
  msg_t status = RDY_TIMEOUT;
  msg_t tmp = 0;
  uint32_t j = 0;

  if (key != NULL)
    index = key_value_search(key, global_data);
  else
    index = n;

  if ((index >= 0) && (index <= ONBOARD_PARAM_COUNT)){
    /* fill all fields */
    param_value_struct->param_value = global_data[index].value;
    param_value_struct->param_count = ONBOARD_PARAM_COUNT;
    param_value_struct->param_index = index;
    param_value_struct->param_type  = MAVLINK_TYPE_FLOAT;
    for (j = 0; j < ONBOARD_PARAM_NAME_LENGTH; j++)
      param_value_struct->param_id[j] = global_data[index].name[j];

    /* send */
    param_value_mail->payload = param_value_struct;
    status = chMBPostAhead(&tolink_mb, (msg_t)param_value_mail, MS2ST(5));
    if (status != RDY_OK)
      return FAILED;

    /* wait until message processed */
    chMBFetch(&param_confirm_mb, &tmp, TIME_INFINITE);
  }
  else
    return FAILED;

  return SUCCESS;
}

/**
 * Send all values one by one.
 */
static void send_all_values(Mail *param_value_mail,
           mavlink_param_value_t *param_value_struct){
  uint32_t i = 0;
  uint32_t n = ONBOARD_PARAM_COUNT;
  for (i = 0; i < n; i++){
    send_value(param_value_mail, param_value_struct, NULL, i);
  }
}


/**
 * ����� ����������� ��������� � ����������� � ������������ ��������� �� �������.
 */
static WORKING_AREA(ParametersThreadWA, 512);
static msg_t ParametersThread(void *arg){
  chRegSetThreadName("Parameters");
  (void)arg;

  /* ���������� ��� �������� ������������� ���������� */
  mavlink_param_value_t param_value_struct;
  Mail param_value_mail = {NULL, MAVLINK_MSG_ID_PARAM_VALUE, &param_confirm_mb};

  /* ���������� ��� ������ ���������� */
  msg_t tmp = 0;
  Mail *input_mail = NULL;
  mavlink_param_set_t *set = NULL;
  mavlink_param_request_list_t *list = NULL;
  mavlink_param_request_read_t *read = NULL;
  mavlink_command_long_t *command = NULL;
  bool_t status = FAILED;

  while (TRUE) {
    chMBFetch(&param_mb, &tmp, TIME_INFINITE);
    input_mail = (Mail*)tmp;

    switch (input_mail->invoice){
    /*
     * �������� ���������, ��� �������� ��������� ���������, �� ������
     * �������� � ������� � ����� ���� �������� � �������� �������������
     */
    case MAVLINK_MSG_ID_PARAM_SET:
      set = (mavlink_param_set_t *)(input_mail->payload);
      input_mail->payload = NULL;
      status = set_parameter(set);
      if (status == SUCCESS){
        send_value(&param_value_mail, &param_value_struct, set->param_id, 0);
      }
      break;

    /*
     * ������ ���� ����������
     */
    case MAVLINK_MSG_ID_PARAM_REQUEST_LIST:
      list = (mavlink_param_request_list_t *)(input_mail->payload);
      input_mail->payload = NULL;
      if (list->target_system == mavlink_system.sysid)
        send_all_values(&param_value_mail, &param_value_struct);
      break;

    /*
     * ������ ������ ���������
     */
    case MAVLINK_MSG_ID_PARAM_REQUEST_READ:
      read = (mavlink_param_request_read_t *)(input_mail->payload);
      input_mail->payload = NULL;
      if (read->param_index >= 0)
        send_value(&param_value_mail, &param_value_struct, NULL, read->param_index);
      else
        send_value(&param_value_mail, &param_value_struct, read->param_id, 0);
      break;

    /*
     * ������� ��������/������ EEPROM
     */
    case MAV_CMD_PREFLIGHT_STORAGE:
      command = (mavlink_command_long_t *)(input_mail->payload);
      input_mail->payload = NULL;

      if (command->param1 == 0)
        load_params_from_eeprom();
      else if (command->param1 == 1)
        save_params_to_eeprom();

      if (command->param2 == 0)
        load_mission_from_eeprom();
      else if (command->param2 == 1)
        save_mission_to_eeprom();
      break;

    }

  }
  return 0;
}

/*
 *******************************************************************************
 * EXPORTED FUNCTIONS
 *******************************************************************************
 */
void ParametersInit(void){

  chMBInit(&param_confirm_mb, param_confirm_mb_buf, (sizeof(param_confirm_mb_buf)/sizeof(msg_t)));

  /* check hardcoded values */
  uint32_t i = 0;
  for (i = 0; i < ONBOARD_PARAM_COUNT; i++){
    if (sizeof (*(global_data[i].name)) > ONBOARD_PARAM_NAME_LENGTH)
      chDbgPanic("name too long");
    if ((global_data[i].value < global_data[i].min) ||
        (global_data[i].value > global_data[i].max))
      chDbgPanic("value out of bounds");
  }

  /* read data from eeprom to memory mapped structure */
  load_params_from_eeprom();

  chThdCreateStatic(ParametersThreadWA,
          sizeof(ParametersThreadWA),
          LINK_THREADS_PRIO + 1,
          ParametersThread,
          NULL);
}






/*****************************************************************************/
// Old stuff


//typedef struct GlobalStruct_t GlobalStruct_t;
//struct GlobalStruct_t
//{
//  float *value;
//  const char **name;
//};


//struct global_struct
//{
//  float param[ONBOARD_PARAM_COUNT];
//  char param_name[ONBOARD_PARAM_COUNT][MAVLINK_MSG_PARAM_SET_FIELD_PARAM_ID_LEN];
//};
//
//struct global_struct global_data;


//float value[] = {0.1, 0.2, 0.3};
//const char *name[] = {"IMU_g1", "IMU_g2", "IMU_g3", NULL};
//
//GlobalStruct_t global_data = {
//    value,
//    name,
//};
//#define ONBOARD_PARAM_COUNT  (sizeof(value) / sizeof(value[0]))


/**
 * @brief   Performs key-value search.
 *
 * @return      Index in dictionary.
 * @retval -1   key not found.
 */
//static int32_t key_value_search(char* key){
//  uint32_t i = 0;
//  uint32_t j = 0;
//
//  for (i = 0; i < ONBOARD_PARAM_COUNT; i++){
//    for (j = 0; j < ONBOARD_PARAM_NAME_LENGTH; j++){
//      // Compare keys
//      if (global_data.name[i][j] != key[j])
//        break;
//      // End matching if null termination is reached
//      if (global_data.name[i][j] == '\0')
//        return i;
//    }
//  }
//  return -1;
//}
