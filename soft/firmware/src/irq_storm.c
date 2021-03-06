#include <stdlib.h>

#include "ch.h"
#include "hal.h"
#include "main.h"
#include "message.h"

#if ENABLE_IRQ_STORM

#define IRQSTROM_GPTD1 GPTD3
#define IRQSTROM_GPTD2 GPTD2
#define IRQSTORM_SD    SD2

/*===========================================================================*/
/* Extern vars.                                                              */
/*===========================================================================*/


/*===========================================================================*/
/* Configurable settings.                                                    */
/*===========================================================================*/

#ifndef RANDOMIZE
#define RANDOMIZE       TRUE
#endif

#ifndef ITERATIONS
#define ITERATIONS      1
#endif

#ifndef NUM_THREADS
#define NUM_THREADS     4
#endif

#ifndef MAILBOX_SIZE
#define MAILBOX_SIZE    4
#endif

/*===========================================================================*/
/* Test related code.                                                        */
/*===========================================================================*/

#define MSG_SEND_LEFT   0
#define MSG_SEND_RIGHT  1

static bool_t saturated;

/*
 * Mailboxes and buffers.
 */
static Mailbox mb[NUM_THREADS];
static msg_t b[NUM_THREADS][MAILBOX_SIZE];

/*
 * Test worker threads.
 */
static WORKING_AREA(waWorkerThread[NUM_THREADS], 256);
static msg_t WorkerThread(void *arg) {
  static volatile unsigned x = 0;
  static unsigned cnt = 0;
  unsigned me = (unsigned)arg;
  unsigned target;
  unsigned r;
  msg_t msg;

  /* Work loop.*/
  while (TRUE) {
    /* Waiting for a message.*/
   chMBFetch(&mb[me], &msg, TIME_INFINITE);

#if RANDOMIZE
   /* Pseudo-random delay.*/
   {
     chSysLock();
     r = rand() & 15;
     chSysUnlock();
     while (r--)
       x++;
   }
#else
   /* Fixed delay.*/
   {
     r = me >> 4;
     while (r--)
       x++;
   }
#endif

    /* Deciding in which direction to re-send the message.*/
    if (msg == MSG_SEND_LEFT)
      target = me - 1;
    else
      target = me + 1;

    if (target < NUM_THREADS) {
      /* If this thread is not at the end of a chain re-sending the message,
         note this check works because the variable target is unsigned.*/
      msg = chMBPost(&mb[target], msg, TIME_IMMEDIATE);
      if (msg != RDY_OK)
        saturated = TRUE;
    }
    else {
      /* Provides a visual feedback about the system.*/
      if (++cnt >= 500) {
        cnt = 0;
        palTogglePad(GPIOB, GPIOB_LED_B);
      }
    }
  }
  return 0;
}

/*
 * GPT1 callback.
 */
static void gpt1cb(GPTDriver *gptp) {
  msg_t msg;

  (void)gptp;
  chSysLockFromIsr();
  msg = chMBPostI(&mb[0], MSG_SEND_RIGHT);
  if (msg != RDY_OK)
    saturated = TRUE;
  chSysUnlockFromIsr();
}

/*
 * GPT2 callback.
 */
static void gpt2cb(GPTDriver *gptp) {
  msg_t msg;

  (void)gptp;
  chSysLockFromIsr();
  msg = chMBPostI(&mb[NUM_THREADS - 1], MSG_SEND_LEFT);
  if (msg != RDY_OK)
    saturated = TRUE;
  chSysUnlockFromIsr();
}

/*
 * GPT1 configuration.
 */
static const GPTConfig gpt1cfg = {
  1000000,  /* 1MHz timer clock.*/
  gpt1cb    /* Timer callback.*/
};

/*
 * GPT2 configuration.
 */
static const GPTConfig gpt2cfg = {
  1000000,  /* 1MHz timer clock.*/
  gpt2cb    /* Timer callback.*/
};


/*===========================================================================*/
/* Generic demo code.                                                        */
/*===========================================================================*/

static void print(char *p) {

  while (*p) {
    chIOPut(&IRQSTORM_SD, *p++);
  }
}

static void println(char *p) {

  while (*p) {
    chIOPut(&IRQSTORM_SD, *p++);
  }
  chIOWriteTimeout(&IRQSTORM_SD, (uint8_t *)"\r\n", 2, TIME_INFINITE);
}

static void printn(uint32_t n) {
  char buf[16], *p;

  if (!n)
    chIOPut(&IRQSTORM_SD, '0');
  else {
    p = buf;
    while (n)
      *p++ = (n % 10) + '0', n /= 10;
    while (p > buf)
      chIOPut(&IRQSTORM_SD, *--p);
  }
}

static const SerialConfig sercfg = {
    115200,
    0,
    0,
    0,
};

/* Главный тред. ПОМНИ, что ему 128 байт не хватает для помещения всего стэка */
static WORKING_AREA(StormTreadWA, 256);
static msg_t StormTread(void *arg){
  (void)arg;
  unsigned i;
  gptcnt_t interval, threshold, worst;

  /*
   * Initializes the mailboxes and creates the worker threads.
   */
  for (i = 0; i < NUM_THREADS; i++) {
    chMBInit(&mb[i], b[i], MAILBOX_SIZE);
    chThdCreateStatic(waWorkerThread[i], sizeof waWorkerThread[i],
                      NORMALPRIO - 20, WorkerThread, (void *)i);
  }

  /*
   * Test procedure.
   */
  println("");
  println("*** ChibiOS/RT IRQ-STORM long duration test");
  println("***");
  print("*** Kernel:       ");
  println(CH_KERNEL_VERSION);
#ifdef __GNUC__
  print("*** GCC Version:  ");
  println(__VERSION__);
#endif
  print("*** Architecture: ");
  println(CH_ARCHITECTURE_NAME);
#ifdef CH_CORE_VARIANT_NAME
  print("*** Core Variant: ");
  println(CH_CORE_VARIANT_NAME);
#endif
#ifdef PLATFORM_NAME
  print("*** Platform:     ");
  println(PLATFORM_NAME);
#endif
#ifdef BOARD_NAME
  print("*** Test Board:   ");
  println(BOARD_NAME);
#endif
  println("***");
  print("*** System Clock: ");
  printn(STM32_SYSCLK);
  println("");
  print("*** Iterations:   ");
  printn(ITERATIONS);
  println("");
  print("*** Randomize:    ");
  printn(RANDOMIZE);
  println("");
  print("*** Threads:      ");
  printn(NUM_THREADS);
  println("");
  print("*** Mailbox size: ");
  printn(MAILBOX_SIZE);
  println("");

  println("");
  worst = 0;

  for (i = 1; i <= ITERATIONS; i++){
    print("Iteration ");
    printn(i);
    println("");
    saturated = FALSE;
    threshold = 0;
    //defaults: max interval == 2000, min interval == 20, divider == 10
    for (interval = 1000; interval >= 20; interval -= interval / 10) {
      gptStartContinuous(&IRQSTROM_GPTD1, interval - 1); /* Slightly out of phase.*/
      gptStartContinuous(&IRQSTROM_GPTD2, interval + 1); /* Slightly out of phase.*/
      chThdSleepMilliseconds(1000);
      gptStopTimer(&IRQSTROM_GPTD1);
      gptStopTimer(&IRQSTROM_GPTD2);
      if (!saturated){
        print(".");
        printn(interval);
        println("");
      }
      else {
        print("#");
        println("");
        if (threshold == 0)
          threshold = interval;
      }
    }
    /* Gives the worker threads a chance to empty the mailboxes before next
       cycle.*/
    chThdSleepMilliseconds(20);
    println("");
    print("Saturated at ");
    printn(threshold);
    println(" uS");
    println("");
    if (threshold > worst)
      worst = threshold;
  }
  gptStopTimer(&IRQSTROM_GPTD1);
  gptStopTimer(&IRQSTROM_GPTD2);

  print("Worst case at ");
  printn(worst);
  println(" uS");
  println("");
  println("Test Complete");

  /*
   * Normal main() thread activity, nothing in this test.
   */
  while (TRUE) {
    chThdSleepMilliseconds(5000);
  }
  return 0;
}


void IRQStormInit(void){
  sdStart(&IRQSTORM_SD, &sercfg);

  gptStart(&IRQSTROM_GPTD1, &gpt1cfg);
  gptStart(&IRQSTROM_GPTD2, &gpt2cfg);

  chThdCreateStatic(StormTreadWA,
          sizeof(StormTreadWA),
          NORMALPRIO,
          StormTread,
          NULL);
}
#endif /* ENABLE_IRQ_STORM */


