
#include <math.h>
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico/multicore.h"
#include "profile.h"

extern void init_pid(float kp_in, float ki_in, float kd_in, float gain_in, float dead_band_in);
extern float process_pid(float error);

#define PIN_AIN2 2
#define PIN_AIN1 3
#define PIN_PWM 0

const uint8_t ADDRESS = 0x36;

static volatile float* debug_float1;
static volatile float* debug_float2;
static volatile float* debug_float3;

static profile_t* profile;

static uint32_t stop = false;

volatile int16_t* angle;

static uint32_t start_ms = 0;

static float desired_angle;
static float angle_of_retch = 0.0;
static float half_angle = 0.0;
static float half_distance_tension_factor = 0.0;

static float distance = 0.0;
static float tension = 0.0;

static float last_angle = 0.0;
static uint32_t last_status = 0;

static uint pwm_slice_num = 0;
static uint pwn_channel = 0;
static uint32_t pwm_frequency = 1000;

int ret;
uint8_t buf[5];

uint8_t reg = 0x0B;


float max_f(float a, float b) {
    return a > b ? a : b;
}

float min_f(float a, float b) {
    return a < b ? a : b;
}

uint32_t pwm_set_freq_duty(uint slice_num, uint chan, uint32_t f, int d) {
    uint32_t clock = 125000000;
    uint32_t divider16 = clock / f / 4096 + (clock % (f * 4096) != 0);
    if (divider16 / 16 == 0) {
        divider16 = 16;
    }
    uint32_t wrap = clock * 16 / divider16 / f - 1;
    pwm_set_clkdiv_int_frac(slice_num, divider16/16, divider16 & 0xF);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, chan, wrap * d / 100);
    return wrap;
}


float angle_difference(float a1, float a2) {
    float diff = a1 - a2;
    if (diff >= 180.0) {
        return diff - 360.0;
    } else if (diff <= -180.0) {
        return diff + 360.0;
    }
    return diff;
}

float apply_expo(float value, float expo_percentage) {
    if (value >= 0.0) {
        return value * value * expo_percentage + value * (1.0 - expo_percentage);
    } else {
        return - value * value * expo_percentage + value * (1.0 - expo_percentage);
    }
}

void read_angle(volatile int16_t* angle) {
    ret = i2c_write_blocking(i2c_default, ADDRESS, &reg, 1, true);
    if (ret < 0) {
      *angle = ret - 2000;
    } else {
      ret = i2c_read_blocking(i2c_default, ADDRESS, buf, 5, false);
      if (ret < 0) {
        *angle = ret - 3000;
      } else {
          float new_angle = (buf[3] * 256 + buf[4]) * 360 / 4096;
          new_angle += profile->zero;
          if (new_angle >= 360.0) {
            new_angle -= 360.0;
          } else if (new_angle < 0.0) {
            new_angle += 360.0;
          }
          *angle = new_angle;
      }
    }
}

void run_cycle() {
        desired_angle =  floor(((float)*angle) / angle_of_retch) * angle_of_retch + half_angle;

        float error = angle_difference(desired_angle, *angle);

        error = apply_expo(error / angle_of_retch, profile->expo) * angle_of_retch;

        tension = process_pid(error);
        *debug_float1 = tension;

        // if (tension < 0) {
        //     tension = max(-1.0, tension) * 100;
        // } else {
        //     tension = min(1.0, tension) * 100;
        // }

        if (tension < 0) {
            tension = max_f(-100.0, tension);
        } else {
            tension = min_f(100.0, tension);
        }

        pwm_set_freq_duty(pwm_slice_num, pwn_channel, pwm_frequency, (int)((tension >= 0) ? tension : -tension));

        int32_t tension_direction = ((tension > 0) ? 1 : -1) * profile->direction;

        if (tension_direction >= 0) {
            gpio_put(PIN_AIN1, 0);
            gpio_put(PIN_AIN2, 1);
        } else if (tension_direction < 0) {
            gpio_put(PIN_AIN1, 1);
            gpio_put(PIN_AIN2, 0);
        } else {
            gpio_put(PIN_AIN1, 1);
            gpio_put(PIN_AIN2, 1);
        }
}


void core1_entry() {
    start_ms = board_millis();
    while (true) {
        read_angle(angle);
        uint32_t now = board_millis();
        if (now >= start_ms + 10) {
          run_cycle();
          start_ms = now;
        }
    }
}

void set_profile(profile_t* profile_in) {
    profile = profile_in;
    float gain = profile->gain_factor * ((float)profile->dividers) / 1.25;
    init_pid(0.7, 0.0, 0.01, gain, profile->dead_band);

    angle_of_retch = (360.0 / (float)profile->dividers);
    half_angle = angle_of_retch / 2.0;
    half_distance_tension_factor = 100.0 / half_angle;
}

void start_second_core(
    volatile int16_t* angle_ptr,
    profile_t* profile,
    volatile float* debug_float1_in) {

    debug_float1 = debug_float1_in;

    angle = angle_ptr;
    set_profile(profile);

    gpio_init(PIN_AIN1);
    gpio_set_dir(PIN_AIN1, GPIO_OUT);
    gpio_put(PIN_AIN1, 0);
    gpio_init(PIN_AIN2);
    gpio_set_dir(PIN_AIN2, GPIO_OUT);
    gpio_put(PIN_AIN2, 0);

    gpio_set_function(PIN_PWM, GPIO_FUNC_PWM);

    pwm_slice_num = pwm_gpio_to_slice_num(PIN_PWM);
    pwn_channel = pwm_gpio_to_channel(PIN_PWM);
    pwm_set_freq_duty(pwm_slice_num, pwn_channel, pwm_frequency, 100);
    pwm_set_enabled(pwm_slice_num, true);

    multicore_launch_core1(core1_entry);
}
