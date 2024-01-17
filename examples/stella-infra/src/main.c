#include "riotee.h"
#include "riotee_timing.h"
#include "printf.h"
#include "riotee_uart.h"
#include "riotee_stella.h"
#include "riotee_adc.h"
#include "riotee_gpio.h"

#include "shtc3.h"
#include "vm1010.h"

#include "arm_math.h"
#include "arm_const_structs.h"

/* Pin D10 enables/disables microphone on the Riotee Sensor Shield (low active) */
#define PIN_MICROPHONE_DISABLE PIN_D5

#define FFT_SIZE (1024)
#define PEAK_COUNT (10)


void startup_callback(void) {
  /* Call this early to put SHTC3 into low power mode */
  shtc3_init();
  /* Disable microphone to reduce current consumption. */
  riotee_gpio_cfg_output(PIN_MICROPHONE_DISABLE);
  riotee_gpio_set(PIN_MICROPHONE_DISABLE);
}

/* This gets called after every reset */
void reset_callback(void) {
  riotee_stella_init();
  /* Required for VM1010 */
  riotee_adc_init();

  vm1010_cfg_t cfg = {.pin_vout = PIN_D2, .pin_vbias = PIN_D3, .pin_mode = PIN_D10, .pin_dout = PIN_D4};
  vm1010_init(&cfg);
}

void turnoff_callback(void) {
  /* Disable the microphone */
  riotee_gpio_set(PIN_MICROPHONE_DISABLE);
  vm1010_exit();
}


static void statistic(const float32_t *const data, const size_t n) {
  float32_t max = data[0];
  float32_t min = data[0];
  float32_t sum = data[0];
  for (uint32_t i = 1; i < n; i++) {
    if (data[i] > max) max = data[i];
    if (data[i] < min) min = data[i];
    sum += data[i];
  }
  float32_t mean = sum / n;
  printf("MinMeanMax=[%f,%f,%f]\r\n",min, mean, max);
}

static void int2float(float32_t *const dst, const int16_t *const src, const size_t n) {
  /* also scale and balance */
  float32_t sum = 0.0;

  for (uint32_t i = 0; i < n; i++) {
    dst[i] = src[i];
    sum = dst[i];
  }
  const float32_t mean = sum / n;

  for (uint32_t i = 0; i < n; i++) {
    dst[i] = (dst[i] - mean) / 512.0;
  }
}

static uint32_t array_find_global_max(const float32_t *const data, const size_t n) {
  float32_t max = data[0];
  uint32_t pos = 0;

  for (uint32_t i=1; i < n; i++) {
    if (data[i] > max) {
        max = data[i];
        pos = i;
    }
  }
  return pos;
}

static float32_t mean(const float32_t *const data, const size_t n) {
  float32_t sum = 0.0;
  for (uint32_t i=0; i < n; i++) {
    sum += fabs(data[i]);
  }
  return sum/n;
}

typedef struct {
  uint32_t fft_size;
  uint32_t sample_rate_Hz;
  float_t mean;
  uint32_t peak_pos[PEAK_COUNT];
  float32_t peak_val[PEAK_COUNT];
  /* F_bin = peak_pos * sample_rate_Hz / fft_size */
} fft_peak;

static void analyze_fft(float32_t *const data, const size_t n, fft_peak *const result) {
  uint32_t pos;
  printf("\r\n");

  result->mean = mean(data,n);
  printf("mean=%f\r\n", result->mean);

  printf("PEAK.pos [");
  for (uint32_t i=0; i < 10; i++) {
    pos = array_find_global_max(data, n);
    result->peak_pos[i] = pos;
    result->peak_val[i] = data[pos];
    data[pos] = -512.0;
    printf("%d, ",pos);
  }
  printf("]\r\n");

  printf("PEAK.val [");
  for (uint32_t i=0; i < 10; i++) {
    printf("%f, ",result->peak_val[i]);
  }
  printf("]\r\n");
}

int main(void) {
  int rc;
  static int16_t samples_i16[FFT_SIZE];
  static float32_t samples_f32[FFT_SIZE];
  static float32_t samples_fft[FFT_SIZE];
  static arm_rfft_fast_instance_f32 fft_instance;
  fft_peak result_fft = {.fft_size = FFT_SIZE, .sample_rate_Hz = 32768/2};
  printf("Startup!\r\n");
  arm_rfft_fast_init_f32(&fft_instance, FFT_SIZE);

  for (;;) {

    riotee_wait_cap_charged();

    printf("Wait, ");
    /* Switch on microphone */
    riotee_gpio_clear(PIN_MICROPHONE_DISABLE);
    /* Wait for 2ms for V_BIAS to come up */
    riotee_sleep_ticks(70);
    /* Wait for wake-on-sound signal from microphone */
    rc = vm1010_wait4sound();
    if (rc == 0) {
      /* Wait until microphone can be sampled (200 us, see VM1010 datasheet)*/
      riotee_sleep_ticks(50);  // TODO: 5 was too low, 30 is almost ok, 50 seems safe (>1.5 ms?!?)
      printf("Sample");
      rc = vm1010_sample(samples_i16, FFT_SIZE, 2);
      printf("=%d, ", rc);
    }
    /* Disable the microphone */
    riotee_gpio_set(PIN_MICROPHONE_DISABLE);


    /* only process when sampling was successful */
    if (rc == 0) {
      riotee_wait_cap_charged();
      printf("Convert, ");
      int2float(samples_f32, samples_i16, FFT_SIZE);
      statistic(samples_f32, FFT_SIZE);

      riotee_wait_cap_charged();
      printf("FFT, ");
      arm_rfft_fast_f32(&fft_instance, samples_f32, samples_fft, 0);
      riotee_wait_cap_charged();
      analyze_fft(samples_fft, FFT_SIZE/2, &result_fft);


      riotee_wait_cap_charged();
      printf("Sending");
      rc = riotee_stella_send((uint8_t *)&result_fft, sizeof(result_fft));
      printf("=%d\r\n", rc);
    }
    else
    {
      printf("\r\n");
    }
  }
}

/*
removed mean and normed to [-1;+1]

click (just trigger)
PEAK.pos [3, 5, 7, 2, 19, 9, 11, 13, 4, 6, ]
PEAK.val [69.370682, 30.643692, 16.074652, 13.049604, 12.046340, 11.338672, 9.684170, 6.735381, 6.257684, 6.121381, ]

talking
PEAK.pos [3, 243, 218, 222, 245, 226, 5, 230, 252, 240, ]
PEAK.val [18.483202, 12.055593, 11.887209, 10.241501, 9.840650, 9.020041, 8.544738, 7.363227, 7.346690, 6.998050, ]

removed mean and scaled by 1/512

click (just trigger)
PEAK.pos [3, 5, 7, 9, 17, 6, 18, 2, 4, 11, ]
PEAK.val [0.806136, 0.398069, 0.207820, 0.169790, 0.116713, 0.105364, 0.102634, 0.096763, 0.095675, 0.090096, ]

talking
PEAK.pos [6, 5, 3, 8, 64, 4, 206, 54, 102, 225, ]
PEAK.val [1.160508, 1.111708, 0.950884, 0.855775, 0.768262, 0.757286, 0.666614, 0.661448, 0.635810, 0.602467, ]


Scale & Balance

MinMeanMax=[-82.000000,-75.328125,-69.000000]
MinMeanMax=[-0.013031,0.000000,0.012360]
PEAK.pos [3, 5, 2, 7, 13, 9, 17, 6, 4, 15, ]
PEAK.val [0.907001, 0.335224, 0.176746, 0.162336, 0.157170, 0.156757, 0.136671, 0.125990, 0.109361, 0.089939, ]

MinMeanMax=[-81.000000,-69.125000,-57.000000]
MinMeanMax=[-0.023193,0.000000,0.023682]
PEAK.pos [3, 253, 128, 196, 5, 63, 2, 251, 134, 192, ]
PEAK.val [0.635550, 0.480197, 0.359375, 0.304189, 0.254202, 0.224117, 0.213716, 0.202552, 0.197669, 0.183445, ]



Only Scale

MinMeanMax=[-76.000000,-70.984375,-64.000000]
MinMeanMax=[-0.148438,-0.138641,-0.125000]
PEAK.pos [3, 5, 2, 7, 9, 18, 6, 4, 13, 20, ]
PEAK.val [0.711176, 0.296141, 0.203036, 0.173813, 0.156337, 0.145886, 0.145802, 0.123264, 0.109788, 0.105506, ]


MinMeanMax=[-101.000000,-78.765625,-60.000000]
MinMeanMax=[-0.197266,-0.153839,-0.117188]
PEAK.pos [3, 193, 5, 194, 147, 195, 7, 219, 144, 223, ]
PEAK.val [1.092306, 0.571193, 0.540024, 0.464895, 0.420604, 0.418033, 0.405213, 0.363521, 0.320398, 0.281918, ]

MinMeanMax=[-0.195312,-0.184526,-0.173828]
PEAK.pos [3, 10, 2, 5, 11, 4, 12, 43, 9, 6, ]
PEAK.val [2.046899, 0.992891, 0.844393, 0.778878, 0.599618, 0.502896, 0.387703, 0.290018, 0.283419, 0.243672, ]

MinMeanMax=[-0.269531,-0.205570,-0.138672]
PEAK.pos [153, 3, 152, 114, 191, 38, 5, 266, 154, 77, ]
PEAK.val [18.224449, 4.698376, 4.113214, 3.757985, 3.686346, 2.880109, 2.117929, 2.007427, 1.818992, 1.723123, ]


 */