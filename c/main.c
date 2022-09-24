#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "bsp/board.h"
#include "pico/multicore.h"
#include "joystick_hid.h"

#define FLAG_VALUE 123

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
static uint32_t next_report = 0;
static uint32_t last_button = 0;
static volatile int16_t angle = 0;
static volatile uint32_t reading_count = 1;
static uint32_t btn = false;


void read_angle(volatile int32_t* angle);
extern void start_second_core(volatile int16_t* angle_ptr, volatile uint32_t* reading_count);

void led_blinking_task();
void hid_task();
void local_i2c_init();
void i2c_scan();


bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

int main() {
    board_init();
    tusb_init();
    stdio_init_all();
    local_i2c_init();

    sleep_ms(10);
    start_second_core(&angle, &reading_count);
    sleep_ms(500);

    while (true) {
        // uint32_t const btn = board_button_read();
        // if (last_button != btn) {
        //   if (btn) {
        //     printf("Button pressed!\n");
        //     // i2c_scan();
        //     printf("Angle is %i (readings %i)\n", angle, reading_count);
        //   } else {
        //     printf("Button is released!\n");
        //   }
        //   last_button = btn;
        // }


        tud_task(); // tinyusb device task
        led_blinking_task();

        uint32_t const now = board_millis();
        if (now >= next_report) {
          next_report = now + 2000;
          printf("Angle is %i (readings %i)\n", angle, reading_count);
        }

        hid_task();

        // read_angle(&angle);
    }
}


#include "tusb.h"

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
         hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
         hid_report_type_t report_type, uint8_t const* buffer,
         uint16_t bufsize) {

    (void) instance;

    if (report_type == HID_REPORT_TYPE_OUTPUT) {
      // Set keyboard LED e.g Capslock, Numlock etc...
      if (report_id == REPORT_ID_KEYBOARD) {
        // bufsize should be (at least) 1
        if (bufsize < 1) { return; }

        uint8_t const kbd_leds = buffer[0];

        if (kbd_leds & KEYBOARD_LED_CAPSLOCK) {
          // Capslock On: disable blink, turn led on
          blink_interval_ms = 0;
          board_led_write(true);
        } else {
          // Caplocks Off: back to normal blink
          board_led_write(false);
          blink_interval_ms = BLINK_MOUNTED;
        }
      }
    }
  }


//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

static void send_hid_report(uint8_t report_id) {
  // skip if hid is not ready yet
  if (!tud_hid_ready()) { return; }

  switch(report_id) {
    case REPORT_ID_KEYBOARD:
    {
      // use to avoid send multiple consecutive zero report for keyboard
      static bool has_keyboard_key = false;

      // if (btn) {
      //   uint8_t keycode[6] = { 0 };
      //   keycode[0] = HID_KEY_A;

      //   tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
      //   has_keyboard_key = true;
      // } else {
      //   // send empty key report if previously has key pressed
      //   if (has_keyboard_key) tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
      //   has_keyboard_key = false;
      // }
    }
    break;

    case REPORT_ID_MOUSE:
    {
      int8_t const delta = btn ? 5 : 0;

      // no button, right + down, no scroll, no pan
      tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, delta, delta, 0, 0);
    }
    break;

    case REPORT_ID_CONSUMER:
    {
      // use to avoid send multiple consecutive zero report
      static bool has_consumer_key = false;

      // if (btn) {
      //   // volume down
      //   uint16_t volume_down = HID_USAGE_CONSUMER_VOLUME_DECREMENT;
      //   tud_hid_report(REPORT_ID_CONSUMER, &volume_down, 2);
      //   has_consumer_key = true;
      // } else {
      //   // send empty key report (release key) if previously has key pressed
      //   uint16_t empty_key = 0;
      //   if (has_consumer_key) { tud_hid_report(REPORT_ID_CONSUMER, &empty_key, 2); }
      //   has_consumer_key = false;
      // }
    }
    break;

    case REPORT_ID_GAMEPAD:
    {
      // use to avoid send multiple consecutive zero report for keyboard
      static bool has_gamepad_key = false;

      hid_gamepad_report_t report =
        {
          .x   = 0, .y = 0, .z = 0, .rz = 0, .rx = 0, .ry = 0,
          .hat = 0, .buttons = 0
        };

      // if (btn) {
      //   // printf("Gamepad: has button pressed...");
      //   report.hat = GAMEPAD_HAT_UP;
      //   report.buttons = GAMEPAD_BUTTON_A;
      //   tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));

      //   has_gamepad_key = true;
      // } else {
      //   report.hat = GAMEPAD_HAT_CENTERED;
      //   report.buttons = 0;
      //   if (has_gamepad_key) {
      //     printf("Gamepad: Sending REPORT_ID_GAMEPAD\n");
      //     tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
      //   }

      //   has_gamepad_key = false;
      // }
    }
    break;

    case REPORT_ID_JOYSTICK:
    {
      // use to avoid send multiple consecutive zero report for keyboard
      static bool has_gamepad_key = false;

      hid_joystick_report_t report =
        {
          .x   = 0, .y = 0, .z = 0,
          .buttons = 0
        };

        report.x = angle;
        report.buttons = GAMEPAD_BUTTON_A;
        tud_hid_report(REPORT_ID_JOYSTICK, &report, sizeof(report));

        // printf("Sending %i\n", report.x);

        has_gamepad_key = false;
    }
    break;

    default: break;
  }
}

// Every 10ms, we will sent 1 report for each HID profile (keyboard, mouse etc ..)
// tud_hid_report_complete_cb() is used to send the next report after previous one is complete
void hid_task() {
  // Poll every 10ms
  const uint32_t interval_ms = 10;
  static uint32_t start_ms = 0;

  if (btn) {
    printf("Button is pressed...\n");
  }

  if (board_millis() - start_ms < interval_ms) { return; } // not enough time
  start_ms += interval_ms;

  // Remote wakeup
  if (tud_suspended() && btn) {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    tud_remote_wakeup();
  } else {
    // Send the 1st of report chain, the rest will be sent by tud_hid_report_complete_cb()
    // send_hid_report(REPORT_ID_GAMEPAD, btn);
    send_hid_report(REPORT_ID_JOYSTICK);
  }
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task() {
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // blink is disabled
  if (!blink_interval_ms) return;

  // Blink every interval ms
  if (board_millis() - start_ms < blink_interval_ms) { return; } // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}

void local_i2c_init() {
  #if !defined(i2c_default) || !defined(PICO_DEFAULT_I2C_SDA_PIN) || !defined(PICO_DEFAULT_I2C_SCL_PIN)
  #warning i2c/bus_scan example requires a board with I2C pins
    printf("Default I2C pins were not defined\n");
  #else
    // This example will use I2C0 on the default SDA and SCL pins (GP4, GP5 on a Pico)
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));

    printf("\nI2C Initiated\n");
  #endif
}

// void i2c_scan() {
//   #if !defined(i2c_default) || !defined(PICO_DEFAULT_I2C_SDA_PIN) || !defined(PICO_DEFAULT_I2C_SCL_PIN)
//     printf("Default I2C pins were not defined - cannot scan\n");
//   #else
//     printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

//     for (int addr = 0; addr < (1 << 7); ++addr) {
//         if (addr % 16 == 0) {
//             printf("%02x ", addr);
//         }

//         // Perform a 1-byte dummy read from the probe address. If a slave
//         // acknowledges this address, the function returns the number of bytes
//         // transferred. If the address byte is ignored, the function returns
//         // -1.

//         // Skip over any reserved addresses.
//         int ret;
//         uint8_t rxdata;
//         if (reserved_addr(addr))
//             ret = PICO_ERROR_GENERIC;
//         else
//             ret = i2c_read_blocking(i2c_default, addr, &rxdata, 1, false);

//         printf(ret < 0 ? "." : "@");
//         printf(addr % 16 == 15 ? "\n" : "  ");
//     }
//     printf("Done.\n");
//   #endif
// }
