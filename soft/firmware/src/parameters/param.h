#ifndef PARAM_H_
#define PARAM_H_

#include <mavlink.h>
#include <common.h>


#define ONBOARD_PARAM_NAME_LENGTH 15
#define PARAM_ID_SIZE             16

typedef struct GlobalParam_t GlobalParam_t;

typedef bool_t (*setval_t)(float value, GlobalParam_t *param);

struct GlobalParam_t
{
  const char *name;
  const float min;
  float value;
  const float max;
  const uint8_t param_type;
  /* this function checks and sets value. If value out of range than it will
   * be set to neares allowable value */
  const setval_t setval;
};


//typedef struct GlobalParam_t GlobalParam_t;
//struct GlobalParam_t
//{
//  const char *name;
//  union{
//    float    f32;
//    uint32_t u32;
//    int32_t  i32;
//    uint16_t u16;
//    int16_t  i16;
//    uint8_t  u8;
//    int8_t   i8;
//  }value;
//  const float min;
//  const float max;
//  const uint8_t param_type;
//};


void ParametersInit(void);
int32_t KeyValueSearch(char* key);

#endif /* PARAM_H_ */