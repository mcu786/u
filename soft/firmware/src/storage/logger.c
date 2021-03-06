#include <string.h>

#include "ch.h"
#include "hal.h"

#include "message.h"
#include "logger.h"
#include "timekeeping.h"
#include "link_sortout.h"

/*
 ******************************************************************************
 * DEFINES
 ******************************************************************************
 */

/* use mavlink log format or raw format */
#define MAVLINK_LOG_FORMAT  FALSE

/* FS buffer size */
#define BUFF_SIZE 8192

/* how many spare bytes we have in buffer */
#define FREE (BUFF_SIZE - offset)

/* length of timestamp field in mavlink log record */
#if MAVLINK_LOG_FORMAT
#define TIME_LEN    (sizeof(uint64_t))
#else
#define TIME_LEN    0
#endif

/* length of record */
#define RECORD_LEN  (MAVLINK_MAX_PACKET_LEN + TIME_LEN)

/*
 ******************************************************************************
 * EXTERNS
 ******************************************************************************
 */
extern mavlink_system_t              mavlink_system_struct;

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */

/* 2 switching buffers */
static uint8_t b0[BUFF_SIZE];
static uint8_t b1[BUFF_SIZE];

/* pointer to current working buffer */
static uint8_t* currbuf = b0;

/* offset in current buffer */
static uint32_t offset = 0;

/* some buffers for mavlink handling */
static mavlink_message_t mavlink_msgbuf_log;
static uint8_t recordbuf[RECORD_LEN];

/*
 ******************************************************************************
 * PROTOTYPES
 ******************************************************************************
 */

/*
 *******************************************************************************
 *******************************************************************************
 * LOCAL FUNCTIONS
 *******************************************************************************
 *******************************************************************************
 */

/**
 * Switch pointer to next free buffer.
 */
void swap_buf(void){
  if (currbuf == b0)
    currbuf = b1;
  else
    currbuf = b0;
}


/**
 * Fit data to double buffer.
 * return pointer to full buffer or NULL if current buffer is not full
 */
uint8_t* bufferize(uint8_t *payload, uint32_t count){

  uint8_t *ret;

  if (FREE > count){
    memcpy(currbuf + offset, payload, count);
    offset += count;
    return NULL;
  }
  else{
    /* put in current buffer as much as possible */
    memcpy(currbuf + offset, payload, FREE);
    /* this pointer will be returned as a result of work */
    ret = currbuf;
    /* switch to free buffer */
    swap_buf();
    /* rest of data put in free buffer */
    memcpy(currbuf, payload + FREE, count - FREE);
    offset = count - FREE;
    return ret;
  }
}

/*
 *******************************************************************************
 * EXPORTED FUNCTIONS
 *******************************************************************************
 */

/**
 * Get id of data
 * Pack it
 * Store to FS buffer
 * Raise bool flag if fresh data available
 */
FRESULT WriteLog(FIL *Log, msg_t id, bool_t *fresh_data){
  uint32_t bytes_written;
  uint8_t *fs_buf;
  FRESULT err = FR_OK;

  mavencoder(id, mavlink_system_struct.sysid, &mavlink_msgbuf_log);

#if MAVLINK_LOG_FORMAT
#if CH_DBG_ENABLE_CHECKS
  /* fill buffer with zeros except the timestamp region. Probably not necessary */
  memset(recordbuf + TIME_LEN, 0, RECORD_LEN - TIME_LEN);
#endif
  uint64_t timestamp = pnsGetTimeUnixUsec();
  mavlink_msg_to_send_buffer(recordbuf + TIME_LEN, &mavlink_msgbuf_log);
  memcpy(recordbuf, &timestamp, TIME_LEN);
  fs_buf = bufferize(recordbuf, RECORD_LEN);
#else
  uint16_t len = 0;
  len = mavlink_msg_to_send_buffer(recordbuf, &mavlink_msgbuf_log);
  fs_buf = bufferize(recordbuf, len);
#endif

  if (fs_buf != NULL){
    err = f_write(Log, fs_buf, BUFF_SIZE, (void *)&bytes_written);
    if (err == FR_OK)
      *fresh_data = TRUE;
  }
  return err;
}





