
#include <string.h>
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "hardware/i2c.h"
#include "neokey.h"

#define DEBUG_INIT 1
// #define DEBUG_READ_KEYS 1
// #define DEBUG_WRITE_LEDS 1

uint8_t buttons[4] = {0, 0, 0, 0};
uint8_t buttons_state = 0;
uint32_t buttons_read_count = 0;
uint8_t leds[12] = {0x20, 0, 0, 0x20, 0x20, 0, 0, 0x20, 0, 0, 0x20, 0x20};

static uint8_t buf[20];

static uint32_t initialised = false;

neokey_key_t keys[4] = {
  {  .bt_mask = (1 << BUTTON_A), .bt_prev_state = (1 << BUTTON_A), .st_current_state = ST_RELEASED, .has_change = 0, .key_state = KEY_RELEASED },
  {  .bt_mask = (1 << BUTTON_B), .bt_prev_state = (1 << BUTTON_B), .st_current_state = ST_RELEASED, .has_change = 0, .key_state = KEY_RELEASED },
  {  .bt_mask = (1 << BUTTON_C), .bt_prev_state = (1 << BUTTON_C), .st_current_state = ST_RELEASED, .has_change = 0, .key_state = KEY_RELEASED },
  {  .bt_mask = (1 << BUTTON_D), .bt_prev_state = (1 << BUTTON_D), .st_current_state = ST_RELEASED, .has_change = 0, .key_state = KEY_RELEASED }
};

void neokey_init() {
    int ret;

    buf[0] = STATUS_BASE;
    buf[1] = STATUS_HW_ID;
    #ifdef DEBUG_INIT
        printf("write: [%i, %i]\n", buf[0], buf[1]);
    #endif
    ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 2, false);
    if (ret != 2) {
        printf("ERROR: neokey_init(hd id) %i\n", ret);
        return;
    }
    uint8_t rec[4];
    ret = i2c_read_blocking(i2c_default, NEOKEY_I2C_ADDRESS, rec, 1, false);
    if (ret != 1) {
        printf("ERROR: neokey_init(hd id read) %i\n", ret);
        return;
    }
    printf("Neokey chip_id %i", rec[0]);

    buf[0] = STATUS_BASE;
    buf[1] = STATUS_VERSION;
    #ifdef DEBUG_INIT
        printf("\nwrite: [%i, %i]\n", buf[0], buf[1]);
    #endif
    ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 2, false);
    if (ret != 2) {
        printf("\nERROR: neokey_init(ver) %i\n", ret);
        return;
    }
    ret = i2c_read_blocking(i2c_default, NEOKEY_I2C_ADDRESS, rec, 4, false);
    if (ret != 4) {
        printf("\nERROR: neokey_init(ver read) %i\n", ret);
        return;
    }
    #ifdef DEBUG_INIT
        printf("Neokey ");
    #else
        printf(", ");
    #endif
    printf("version %i, (%i, %i)\n", rec[0] << 8 | rec[1], rec[2], rec[3]);


    buf[0] = NEOPIXEL_BASE;
    buf[1] = NEOPIXEL_PIN;
    buf[2] = 3;
    #ifdef DEBUG_INIT
        printf("write: [%i, %i, %i]\n", buf[0], buf[1], buf[2]);
    #endif
    ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 3, false);
    if (ret != 3) {
        printf("ERROR: neokey_init(neopixel pin) %i\n", ret);
        return;
    }

    buf[0] = NEOPIXEL_BASE;
    buf[1] = NEOPIXEL_BUF_LENGTH;
    buf[2] = 0;
    buf[3] = 12;
    #ifdef DEBUG_INIT
        printf("write: [%i, %i, %i, %i]\n", buf[0], buf[1], buf[2], buf[3]);
    #endif
    ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 4, false);
    if (ret != 4) {
        printf("ERROR: neokey_init(neopixel buf len) %i\n", ret);
        return;
    }

    buf[0] = GPIO_BASE;
    buf[1] = GPIO_DIRCLR_BULK;
    buf[2] = 0;
    buf[3] = 0;
    buf[4] = 0;
    buf[5] = (uint8_t)(BUTTON_MASK);
    #ifdef DEBUG_INIT
        printf("neokey_init write: [%i, %i, %i, %i, %i, %i]\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
    #endif
    ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 6, false);
    if (ret != 6) {
        printf("ERROR: neokey_init(gpio dirclr) %i\n", ret);
        return;
    }

    buf[1] = GPIO_PULLENSET;
    ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 6, false);
    #ifdef DEBUG_INIT
        printf("neokey_init write: [%i, %i, %i, %i, %i, %i]\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
    #endif
    if (ret != 6) {
        printf("ERROR: neokey_init(gpio pullenset) %i\n", ret);
        return;
    }

    buf[1] = GPIO_BULK_SET;
    ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 6, false);
    #ifdef DEBUG_INIT
        printf("neokey_init write: [%i, %i, %i, %i, %i, %i]\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
    #endif
    if (ret != 6) {
        printf("ERROR: neokey_init(gpio bulkset) %i\n", ret);
        return;
    }

    buf[1] = GPIO_INTENSET;
    ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 6, false);
    #ifdef DEBUG_INIT
        printf("neokey_init write: [%i, %i, %i, %i, %i, %i]\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
    #endif
    if (ret != 6) {
        printf("ERROR: neokey_init(gpio intenset) %i\n", ret);
        return;
    }

    initialised = true;
    printf("Neokey initialised\n");
}

void write_leds() {
    if (initialised) {
        int ret;

        buf[0] = NEOPIXEL_BASE;
        buf[1] = NEOPIXEL_BUF;
        buf[2] = 0;
        buf[3] = 0;

        for (int i = 0; i < 12; i++) { buf[i + 4] = leds[i]; }

        #ifdef DEBUG_WRITE_LEDS
            printf("write_leds: [%i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i]\n",
                buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
                buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]
            );
        #endif
        ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 16, false);
        if (ret != 16) {
            printf("ERROR: write_leds %i\n", ret);
            initialised = false;
            return;
        }
    }
}

void show_leds() {
    if (initialised) {
        int ret;

        buf[0] = NEOPIXEL_BASE;
        buf[1] = NEOPIXEL_SHOW;
        #ifdef DEBUG_WRITE_LEDS
            printf("write_leds: [%i, %i]\n", buf[0], buf[1]);
        #endif

        ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 2, false);
        if (ret != 2) {
            printf("ERROR: write_leds (show) %i\n", ret);
            initialised = false;
            return;
        }
    }
}

void read_keys_raw() {
    if (initialised) {
        int ret;

        buf[0] = GPIO_BASE;
        buf[1] = GPIO_BULK;
        ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 2, false);
        if (ret < 0) {
            printf("ERROR: read_keys_raw write: %i\n", ret);
            initialised = false;
            buttons_state = 0xfe;
            return;
        }
        #ifdef DEBUG_READ_KEYS
            printf("read_keys_raw: [%i, %i] -> ", buf[0], buf[1]);
        #endif

        sleep_ms(2);
        uint8_t rec[4];
        ret = i2c_read_blocking(i2c_default, NEOKEY_I2C_ADDRESS, rec, 4, false);
        if (ret != 4) {
            #ifdef DEBUG_READ_KEYS
                printf("\n");
            #endif
            printf("ERROR: read_keys_raw read: %i\n", ret);
            initialised = false;
            buttons_state = 0xfd;
            return;
        }
        #ifdef DEBUG_READ_KEYS
            printf("[%i, %i, %i, %i]\n", rec[0], rec[1], rec[2], rec[3]);
        #endif
        buttons_state = rec[3] & 0xf0;
        for (int i = 0; i < 4; i++) { buttons[i] = rec[i]; }
        buttons_read_count = buttons_read_count + 1;
    } else {
        buttons_state = 0xff;
    }
}

void process_keys(uint32_t now) {
    for (int i = 0; i < 4; i++) {
        neokey_key_t key = keys[i];
        uint8_t new_bt_state = buttons_state & key.bt_mask;
        switch (key.st_current_state) {
            case (ST_RELEASED): {
                if (!new_bt_state) {
                    key.time = now;
                    key.st_current_state = ST_PRESSED_DEBOUNCE;
                }
            }
            break;
            case (ST_PRESSED_DEBOUNCE): {
                if (new_bt_state) {
                    key.st_current_state = ST_RELEASED;
                } else {
                    if (now - key.time > KEY_DEBOUNCE_TIME) {
                        key.has_change = 1;
                        key.key_state = KEY_PRESSED;
                        key.st_current_state = ST_PRESSED;
                    }
                }
            }
            break;
            case (ST_PRESSED): {
                if (new_bt_state) {
                    key.time = now;
                    key.st_current_state = ST_RELEASED_DEBOUNCE;
                } else {
                    if (now - key.time > KEY_LONG_PRESS_TIME) {
                        key.has_change = 1;
                        key.key_state = KEY_LONG_PRESSED;
                        key.st_current_state = ST_LONG_PRESS;
                    }
                }
            }
            break;
            case (ST_LONG_PRESS): {
                if (new_bt_state) {
                    key.time = now;
                    key.st_current_state = ST_RELEASED_DEBOUNCE_LONG_PRESS;
                }
            }
            break;
            case (ST_RELEASED_DEBOUNCE_LONG_PRESS): {
                if (!new_bt_state) {
                    key.st_current_state = ST_LONG_PRESS;
                } else {
                    if (now - key.time > KEY_DEBOUNCE_TIME) {
                        key.has_change = 1;
                        key.key_state = KEY_RELEASED;
                        key.st_current_state = ST_RELEASED;
                    }
                }
            }
            break;
            case (ST_RELEASED_DEBOUNCE): {
                if (!new_bt_state) {
                    key.st_current_state = ST_PRESSED;
                } else {
                    if (now - key.time > KEY_DEBOUNCE_TIME) {
                        key.has_change = 1;
                        key.key_state = KEY_RELEASED;
                        key.st_current_state = ST_RELEASED;
                    }
                }
            }
            break;
            default: break;
        }

        if (key.bt_prev_state != new_bt_state) {
            key.bt_prev_state = new_bt_state;
        }
    }
}

int get_key_state(uint32_t key_num) {
    if (key_num >= 0 && key_num <= 3) {
        neokey_key_t key = keys[key_num];
        if (key.has_change) {
            return key.key_state;
        }
    }
    return -1;
}
