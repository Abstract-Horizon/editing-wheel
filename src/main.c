#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "bsp/board.h"
#include "pico/multicore.h"
#include "joystick_hid.h"
#include "profile.h"
#include "neokey.h"


#define DEBUG_ANGLE 0
#define DEBUG_BUTTON_STATE 0
#define DEBUG_KEYS 0
#define DEBUG_MENU 1

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

enum {
  STATE_BOOTING = 0,
  STATE_INITIALISE,
  STATE_START_SECOND_CORE,
  STATE_PREPARE_TO_RUN,
  STATE_DELAY_RUNNING,
  STATE_RUNNING,
  STATE_STOPPED,
};

volatile int16_t angle = 0;
volatile int16_t last_angle = -1;

extern void neokey_init();
extern void write_leds();
extern uint8_t buttons_state;
extern uint8_t buttons[4];
#if (DEBUG_BUTTON_STATE)
uint8_t last_buttons[4];
uint8_t last_buttons_state = 0x55;
#endif

extern uint32_t current_millis;
extern uint32_t overrun_millis;


extern void start_second_core();
extern void process_keys(uint32_t now);
extern int get_key_state(uint32_t key_num);
extern void set_profile(uint32_t selected_profile_number);

extern uint8_t leds[12];

const uint32_t direction = 1;
const float zero = 235.0;

profile_t profiles[9] = {
    { .direction = direction, .zero = zero, .axis=0, .dividers = 1,  .expo = -0.9,  .gain_factor = 2,    .dead_band = 0.4},
    { .direction = direction, .zero = zero, .axis=0, .dividers = 8, .expo = -0.25, .gain_factor = 1,    .dead_band = 0.4},
    { .direction = direction, .zero = zero, .axis=0, .dividers = 16, .expo = 0.9,   .gain_factor = 1,    .dead_band = 0.4},
    { .direction = direction, .zero = zero, .axis=0, .dividers = 32, .expo = -0.8,  .gain_factor = 0.5, .dead_band = 0.8},
    { .direction = direction, .zero = zero, .axis=0, .dividers = 12, .expo = -0.9,  .gain_factor = 1,    .dead_band = 0.8},
    { .direction = direction, .zero = zero, .axis=0, .dividers = 24, .expo = -0.8,  .gain_factor = 1,    .dead_band = 0.4},
    { .direction = direction, .zero = zero, .axis=0, .dividers = 32, .expo = 0.9,   .gain_factor = 1,    .dead_band = 0.4},
    { .direction = direction, .zero = zero, .axis=0, .dividers = 48, .expo = 0.9,   .gain_factor = 1,    .dead_band = 0.4},
    { .direction = direction, .zero = zero, .axis=0, .dividers = 48, .expo = -0.8,  .gain_factor = 0.5, .dead_band = 0.8},
};

uint32_t selected_profile = 2;

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
static uint32_t next_report = 0;
static uint32_t last_button = 0;

static uint32_t btn = false;
static uint16_t initialise_state = STATE_BOOTING;

enum {
    KEYS_STATE_MENU_PROFILE_SELECT_BANK_0 = 0,
    KEYS_STATE_MENU_PROFILE_SELECT_BANK_1,
    KEYS_STATE_MENU_PROFILE_SELECT_BANK_2,
    KEYS_STATE_WORKING,
};

static int keys_state = KEYS_STATE_WORKING;

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

void set_leds(int i, uint8_t r, uint8_t g, uint8_t b) {
    i = 3 - i;
    leds[i * 3] = g;
    leds[i * 3 + 1] = r;
    leds[i * 3 + 2] = b;
}

void set_leds_to_selected_profile() {
    if (keys_state < KEYS_STATE_WORKING) {
        if (keys_state == KEYS_STATE_MENU_PROFILE_SELECT_BANK_0) {
            set_leds(0, 32, 0, 0);
            set_leds(1, 32, 32, 0);
            set_leds(2, 0, 32, 0);
        } else if (keys_state == KEYS_STATE_MENU_PROFILE_SELECT_BANK_1) {
            set_leds(0, 0, 12, 32);
            set_leds(1, 0, 0, 32);
            set_leds(2, 32, 0, 32);
        } else {
            set_leds(0, 32, 16, 0);
            set_leds(1, 0, 16, 16);
            set_leds(2, 16, 16, 16);
        }
        set_leds(3, 32, 16, 0);
    } else {
        set_leds(0, 0, 0, 0);
        set_leds(1, 0, 0, 0);
        set_leds(2, 0, 0, 0);
        set_leds(3, 32, 16, 0);
        switch (selected_profile) {
            case (0): {
                set_leds(0, 32, 0, 0);
            }
            break;
            case (1): {
                set_leds(1, 32, 32, 0);
            }
            break;
            case (2): {
                set_leds(2, 0, 32, 0);
            }
            break;
            case (3): {
                set_leds(0, 0, 32, 32);
            }
            break;
            case (4): {
                set_leds(1, 0, 12, 32);
            }
            break;
            case (5): {
                set_leds(2, 32, 0, 32);
            }
            break;
            case (6): {
                set_leds(0, 32, 16, 0);
            }
            break;
            case (7): {
                set_leds(1, 0, 16, 16);
            }
            break;
            case (8): {
                set_leds(2, 16, 16, 16);
            }
            break;
            default: break;        
        }
    }
}

void keys_task(uint32_t now) {
    #if (DEBUG_BUTTON_STATE)
        if (last_buttons_state != buttons_state
            || last_buttons[0] != buttons[0]
            || last_buttons[1] != buttons[1]
            || last_buttons[2] != buttons[2]
            || last_buttons[3] != buttons[3]) {
            printf("Button state %i; [%i, %i, %i, %i]\n", buttons_state, buttons[0], buttons[1], buttons[2], buttons[3]);
            last_buttons_state = buttons_state;
            for (int i = 0; i < 4; i++) { last_buttons[i] = buttons[i]; }
        }
    #endif
    process_keys(now);

    for (int i = 0; i < 4; i++) {
        int key_no = 3 - i;
        int key_state = get_key_state(i);
        switch (key_state) {
            case (KEY_DOWN): {
                #if (DEBUG_KEYS)
                printf("Key %d down.\n", key_no);
                #endif
                set_leds(key_no, 32, 32, 32);
            }
            break;
            case (KEY_PRESSED): {
                #if (DEBUG_KEYS)
                printf("Key %d pressed.\n", key_no);
                #endif
                set_leds(key_no, 32, 32, 0);
                if (keys_state < KEYS_STATE_WORKING) {
                    if (key_no < 3) {
                        set_profile(key_no + keys_state * 3);
                        keys_state = KEYS_STATE_WORKING;
                    } else {
                        keys_state += 1;
                    }
                    if (keys_state == KEYS_STATE_WORKING) {
                        #if (DEBUG_MENU)
                        printf("Menu finished, bank %i\n", keys_state);
                        #endif
                    } else {
                        #if (DEBUG_MENU)
                        printf("Menu next, bank %i\n", keys_state);
                        #endif
                    }
                    set_leds_to_selected_profile();
                }
            }
            break;
            case (KEY_LONG_PRESSED): {
                #if (DEBUG_KEYS)
                printf("Key %d long pressed.\n", key_no);
                #endif
                if (key_no == 3) {
                    keys_state = KEYS_STATE_MENU_PROFILE_SELECT_BANK_0;
                    #if (DEBUG_MENU)
                    printf("Menu started, bank %i\n", keys_state);
                    #endif
                    set_leds_to_selected_profile();
                }
            }
            break;
            case (KEY_UP): {
                #if (DEBUG_KEYS)
                printf("Key %d up.\n", key_no);
                #endif
                set_leds_to_selected_profile();
            }
            break;
            case (KEY_RELEASED): {
                #if (DEBUG_KEYS)
                printf("Key %d released.\n", key_no);
                #endif
                set_leds_to_selected_profile();
            }
            break;
            default: break;
        }
    }
}

int main() {
    board_init();
    tusb_init();
    stdio_init_all();

    initialise_state = STATE_INITIALISE;

    uint32_t next_event = board_millis() + 2000;
    while (true) {
        uint32_t const now = board_millis();

        tud_task();
        led_blinking_task();

        if (initialise_state == STATE_INITIALISE && now > next_event) {
            printf("\033c\033[2J\033[;HInitialising...\n");
            next_event = now + 100;
            initialise_state = STATE_STOPPED;
            initialise_state = STATE_START_SECOND_CORE;
            local_i2c_init();
            neokey_init();
            set_leds(0, 0x20, 0, 0);
            set_leds(1, 0x20, 0x20, 0);
            set_leds(2, 0, 0x20, 0);
            set_leds(3, 0, 0x20, 0x20);
        }
        if (initialise_state == STATE_START_SECOND_CORE && now > next_event) {
            start_second_core();
            next_event = now + 100;
            initialise_state = STATE_PREPARE_TO_RUN;
        }
        if (initialise_state == STATE_PREPARE_TO_RUN && now > next_event) {
            initialise_state = STATE_DELAY_RUNNING;
            next_event = now + 2000;
        }
        if (initialise_state == STATE_DELAY_RUNNING && now > next_event) {
            set_leds_to_selected_profile();
            initialise_state = STATE_RUNNING;
        }
        if (initialise_state == STATE_RUNNING) {
            keys_task(now);

            #if (DEBUG_ANGLE)
                if (now >= next_report) {
                    next_report = now + 2000;
                    if (last_angle != angle) {
                        printf("Angle is %i\n", angle);
                        last_angle = angle;
                    }
                }
            #endif

            hid_task();

            if (overrun_millis != 0) {
                printf("Overrun %i  ", overrun_millis);
                overrun_millis = 0;
            }
        }
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


    switch (profiles[selected_profile].axis) {
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
