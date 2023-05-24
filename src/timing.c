#include "nrf.h"
#include "FreeRTOS.h"
#include "task.h"

#include "runtime.h"

/* This points to the task currently blocking on GPINT event */
static TaskHandle_t waiting_task;

void RTC0_IRQHandler(void) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if (NRF_RTC0->EVENTS_COMPARE[0] == 1) {
    NRF_RTC0->EVENTS_COMPARE[0] = 0;

    xTaskNotifyIndexedFromISR(waiting_task, 1, USR_EVT_RTC, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
  }
  if (NRF_RTC0->EVENTS_COMPARE[1] == 1) {
    NRF_RTC0->EVENTS_COMPARE[1] = 0;

    xTaskNotifyIndexedFromISR(sys_task_handle, 1, SYS_EVT_RTC, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
  }

  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void sleep_ticks(unsigned int ticks) {
  unsigned long notification_value;
  waiting_task = xTaskGetCurrentTaskHandle();
  NRF_RTC0->CC[0] = (NRF_RTC0->COUNTER + ticks) % (1 << 24);
  NRF_RTC0->EVTENSET = RTC_EVTENSET_COMPARE0_Msk;
  xTaskNotifyWaitIndexed(1, 0xFFFFFFFF, 0xFFFFFFFF, &notification_value, portMAX_DELAY);
}

void sleep_ms(unsigned int ms) {
  /* Roughly ms*32768/1000 */
  sleep_ticks((ms * 33554UL) >> 10);
}

void sys_setup_timer(unsigned int ticks) {
  NRF_RTC0->CC[1] = (NRF_RTC0->COUNTER + ticks) % (1 << 24);
  NRF_RTC0->EVTENSET = RTC_EVTENSET_COMPARE1_Msk;
}

void sys_cancel_timer(void) {
  NRF_RTC0->EVTENCLR = RTC_EVTENSET_COMPARE1_Msk;
}

int timing_init(void) {
  NRF_CLOCK->LFCLKSRC = CLOCK_LFCLKSRC_SRC_Xtal;

  NRF_CLOCK->TASKS_LFCLKSTART = 1;

  NRF_RTC0->INTENSET = RTC_INTENSET_COMPARE0_Msk | RTC_INTENSET_COMPARE1_Msk;
  NVIC_EnableIRQ(RTC0_IRQn);

  NRF_RTC0->PRESCALER = 0;
  NRF_RTC0->TASKS_CLEAR = 1;
  NRF_RTC0->TASKS_START = 1;
  return 0;
}