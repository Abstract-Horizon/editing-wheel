#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "bsp/board.h"
#include "pico/multicore.h"
#include "joystick_hid.h"
#include "profile.h"

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
static volatile int16_t last_angle = -0;
static volatile uint32_t reading_count = 1;

static volatile float debug_float1 = 1.1;
static volatile float debug_float2 = 2.2;
static volatile float debug_float3 = 3.4;

static uint32_t btn = false;

const uint32_t direction = 1;
const float zero = 235.0;

static profile_t profiles[9] = {
    { .direction = direction, .zero = zero, .axis=0, .dividers = 1, .expo = 0.8, .gain_factor = 2, .dead_band = 0.4},
    { .direction = direction, .zero = zero, .axis=0, .dividers = 1, .expo = -0.8, .gain_factor = 2, .dead_band = 0.4},
    { .direction = direction, .zero = zero, .axis=0, .dividers = 2, .expo = 1, .gain_factor = 1, .dead_band = 1},
    { .direction = direction, .zero = zero, .axis=0, .dividers = 32, .expo = 0.9, .gain_factor = 1, .dead_band = 0.4},
    { .direction = direction, .zero = zero, .axis=0, .dividers = 32, .expo = -0.8, .gain_factor = 1, .dead_band = 1.8},
    { .direction = direction, .zero = zero, .axis=0, .dividers = 24, .expo = 0.9, .gain_factor = 1, .dead_band = 0.4},
    { .direction = direction, .zero = zero, .axis=0, .dividers = 24, .expo = -0.8, .gain_factor = 1, .dead_band = 1.8},
    { .direction = direction, .zero = zero, .axis=0, .dividers = 12, .expo = 0.9, .gain_factor = 1, .dead_band = 0.4},
    { .direction = direction, .zero = zero, .axis=0, .dividers = 12, .expo = -0.8, .gain_factor = 1, .dead_band = 1.8}
};


static profile_t* profile = &profiles[1];


extern void start_second_core(
    volatile int16_t* angle_ptr,
    profile_t* selected_profile,
    volatile float* debug_float1
);
extern void init_msc(volatile int16_t* angle_in, profile_t (*profiles_in)[9]);


void led_blinking_task();
void hid_task();

void local_i2c_init() {
  #if !defined(i2c_default) || !defined(PICO_DEFAULT_I2C_SDA_PIN) || !defined(PICO_DEFAULT_I2C_SCL_PIN)
  #warning i2c/bus_scan example requires a board with I2C pins
    printf("Default I2C pins were not defined\n");
  #else
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));

    printf("\nI2C Initiated\n");
  #endif
}

bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

void setup_profile(profile_t* profile_in) {
    profile = profile_in;
}

int main() {
    setup_profile(&profiles[0]);
    board_init();
    tusb_init();
    stdio_init_all();
    local_i2c_init();
    init_msc(&angle, &profiles);

    sleep_ms(10);
    start_second_core(&angle, profile, &debug_float1);
    sleep_ms(500);

    while (true) {
        tud_task();
        led_blinking_task();

        uint32_t const now = board_millis();
        if (now >= next_report) {
          next_report = now + 2000;
          if (last_angle != angle) {
            printf("Angle is %i, d1=%.2f, d2=%.2f, d3=%.2f\n", angle, debug_float1, debug_float2, debug_float3);
            last_angle = angle;
          }
        }

        hid_task();
    }
}


#include "tusb.h"

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
         hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
         hid_report_type_t report_type, uint8_t const* buffer,
         uint16_t bufsize) {
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
  printf("Mounted...");
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
  printf("Unmounted...");
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
  printf("Suspended...");
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
  printf("Resumed...");
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

static void send_joystick_hid_report() {

    hid_joystick_report_t report = {
        .x = 0, .y = 0, .z = 0,
        .buttons = 0
    };


    switch (profile->axis) {
        case 0: {
            report.x = angle;
        }
        break;
        case 1: {
            report.y = angle;
        }
        break;
        case 2: {
            report.z = angle;
        }
        break;

        default: break;
    }
    tud_hid_report(REPORT_ID_JOYSTICK, &report, sizeof(report));
}

void hid_task() {
    const uint32_t interval_ms = 10;
    static uint32_t start_ms = 0;
    static int16_t previous_angle = -1;
    static uint32_t first_time = true;

    if (board_millis() - start_ms < interval_ms) { return; }
    start_ms += interval_ms;

    if (!tud_hid_ready()) { return; }

    if (previous_angle != angle) {
        send_joystick_hid_report();
        previous_angle = angle;
    }

    if (first_time) {
        // blink_interval_ms = BLINK_CONNECTED;
        first_time = false;
    }
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task() {
  static uint32_t start_ms = 0;
  static bool led_state = false;

  if (!blink_interval_ms) return;

  if (board_millis() - start_ms < blink_interval_ms) { return; }
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state;
}
