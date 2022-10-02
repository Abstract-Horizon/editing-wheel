#include <stdio.h>
#include "bsp/board.h"

static float set_point = 0.0;
static float p = 0.0;
static float i = 0.0;
static float d = 0.0;
static float kp = 0.0;
static float ki = 0.0;
static float kd = 0.0;
static float kg = 0.0;
static float dead_band = 0.0;
static float last_error = 0.0;
static float last_output = 0.0;
static float last_delta = 0.0;
static uint32_t last_time = 0;
static uint32_t first = true;


float abs_f(float in) {
  if (in < 0) {
    return -in;
  }
  return in;
}

void init_pid(
  float kp_in, float ki_in, float kd_in,
  float gain_in, float dead_band_in) {

  p = 0.0;
  i = 0.0;
  d = 0.0;
  kp = kp_in;
  ki = ki_in;
  kd = kd_in,
  kg = gain_in;
  dead_band = dead_band_in;
  last_time = 0;
  last_error = 0.0;
  last_output = 0.0;
  last_delta = 0.0;
  first = true;
}

float process_pid(float error) {
    uint32_t now = board_millis();

    if (abs_f(error) <= dead_band) {
        error = 0.0;
    }

    if (first) {
        first = false;
        last_error = error;
        last_time = now;
      return 0.0;
    }

    uint32_t delta_time = now - last_time;

    p = error;

    float delta_time_f = (float)delta_time / 1000.0;

    if ((last_error < 0 && error > 0) || (last_error > 0 && error < 0)) {
        i = 0.0;
    } else if (abs_f(error) < 0.1) {
        i = 0.0;
    } else {
        i = i + error * delta_time_f;
    }

    if (delta_time > 0) {
        d = (error - last_error) / delta_time_f;
    }

    float output = (p * kp + i * ki + d * kd) * kg;

    last_output = output;
    last_error = error;
    last_time = now;
    last_delta = delta_time;

    return output;
}
