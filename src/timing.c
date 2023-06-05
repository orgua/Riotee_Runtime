#include "nrf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "runtime.h"

void RTC0_IRQHandler(void) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if ((NRF_RTC0->INTENSET & RTC_INTENSET_COMPARE0_Msk) && (NRF_RTC0->EVENTS_COMPARE[0] == 1)) {
    NRF_RTC0->EVENTS_COMPARE[0] = 0;
    NRF_RTC0->EVTENCLR = RTC_EVTENCLR_COMPARE0_Msk;
    NRF_RTC0->INTENCLR = RTC_INTENCLR_COMPARE0_Msk;

    xTaskNotifyIndexedFromISR(usr_task_handle, 1, USR_EVT_RTC, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
  }
  if ((NRF_RTC0->INTENSET & RTC_INTENSET_COMPARE1_Msk) && (NRF_RTC0->EVENTS_COMPARE[1] == 1)) {
    NRF_RTC0->EVENTS_COMPARE[1] = 0;
    NRF_RTC0->EVTENCLR = RTC_EVTENCLR_COMPARE1_Msk;
    NRF_RTC0->INTENCLR = RTC_INTENCLR_COMPARE1_Msk;

    xTaskNotifyIndexedFromISR(sys_task_handle, 1, SYS_EVT_RTC, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
  }

  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void sleep_ticks(unsigned int ticks) {
  unsigned long notification_value;
  taskENTER_CRITICAL();
  xTaskNotifyStateClearIndexed(usr_task_handle, 1);
  NRF_RTC0->CC[0] = (NRF_RTC0->COUNTER + ticks) % (1 << 24);

  NRF_RTC0->EVENTS_COMPARE[0] = 0;
  NRF_RTC0->EVTENSET = RTC_EVTENSET_COMPARE0_Msk;
  NRF_RTC0->INTENSET = RTC_INTENSET_COMPARE0_Msk;

  taskEXIT_CRITICAL();
  xTaskNotifyWaitIndexed(1, 0xFFFFFFFF, 0xFFFFFFFF, &notification_value, portMAX_DELAY);
}

void sleep_ms(unsigned int ms) {
  /* Roughly ms*32768/1000 */
  sleep_ticks((ms * 33554UL) >> 10);
}

void sys_setup_timer(unsigned int ticks) {
  NRF_RTC0->CC[1] = (NRF_RTC0->COUNTER + ticks) % (1 << 24);
  NRF_RTC0->EVENTS_COMPARE[1] = 0;
  NRF_RTC0->EVTENSET = RTC_EVTENSET_COMPARE1_Msk;
  NRF_RTC0->INTENSET = RTC_INTENSET_COMPARE1_Msk;
}

void sys_cancel_timer(void) {
  NRF_RTC0->EVTENCLR = RTC_EVTENSET_COMPARE1_Msk;
  NRF_RTC0->INTENCLR = RTC_INTENCLR_COMPARE1_Msk;

  NRF_RTC0->EVENTS_COMPARE[1] = 0;
}

int timing_init(void) {
  NRF_CLOCK->LFCLKSRC = CLOCK_LFCLKSRC_SRC_Xtal;

  NRF_CLOCK->TASKS_LFCLKSTART = 1;

  NVIC_EnableIRQ(RTC0_IRQn);

  NRF_RTC0->PRESCALER = 0;
  NRF_RTC0->TASKS_CLEAR = 1;
  NRF_RTC0->TASKS_START = 1;
  return 0;
}