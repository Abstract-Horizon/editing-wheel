
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "pico/multicore.h"

const uint8_t ADDRESS = 0x36;

static uint32_t start_ms = 0;
volatile int16_t* angle;
volatile uint32_t* reading_count;

int ret;
uint8_t buf[5];

uint8_t reg = 0x0B;


void read_angle(volatile int16_t* angle) {
    ret = i2c_write_blocking(i2c_default, ADDRESS, &reg, 1, true);
    if (ret < 0) {
      *angle = ret - 2000;
    } else {
      ret = i2c_read_blocking(i2c_default, ADDRESS, buf, 5, false);
      if (ret < 0) {
        *angle = ret - 3000;
      } else {
          *angle = (buf[3] * 256 + buf[4]) * 360 / 4096;
          // *angle = (buf[3] * 256 + buf[4]) * 360;
          // *angle = buf[4];
          // status = pos[0] & 0b00111000 | EditingWheel.STATUS_ERROR_MAGNET_NOT_DETECTED

          // angle += self.zero
          // angle = angle if 0 <= angle < 360 else angle - 360
      }
    }
}

void core1_entry() {
  start_ms = board_millis();
  while (true) {
    read_angle(angle);
    *reading_count = board_millis() - start_ms;
    // sleep_ms(250);

    // uint32_t k = 0;
    // uint32_t j;
    // for (j = 0; j < 1000000; j++) {
    //   k += 1;
    // }
  }
  *angle = -9000;
}

void start_second_core(volatile int16_t* angle_ptr, volatile uint32_t* reading_count_ptr) {
  angle = angle_ptr;
  reading_count = reading_count_ptr;
  // *angle = 22;
  multicore_launch_core1(core1_entry);
}
