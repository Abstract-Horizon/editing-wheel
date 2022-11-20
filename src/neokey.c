
#include <string.h>
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "hardware/i2c.h"
#include "neokey.h"

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
    buf[0] = NEOPIXEL_BASE;
    buf[1] = NEOPIXEL_PIN;
    buf[2] = 3;
    int ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 3, false);
    if (ret != 3) {
        printf("ERROR: neokey_init(1) %i\n", ret);
        return;
    }
    // sleep_ms(10);

    buf[0] = NEOPIXEL_BASE;
    buf[1] = NEOPIXEL_BUF_LENGTH;
    buf[2] = 0;
    buf[3] = 12;

    ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 4, false);
    if (ret != 4) {
        printf("ERROR: neokey_init(2) %i\n", ret);
        return;
    }
    // sleep_ms(10);

    buf[0] = GPIO_BASE;
    buf[1] = GPIO_DIRCLR_BULK;
    buf[2] =(uint8_t)(BUTTON_MASK >> 24);
    buf[3] =(uint8_t)(BUTTON_MASK >> 16);
    buf[4] =(uint8_t)(BUTTON_MASK >> 8);
    buf[5] =(uint8_t)(BUTTON_MASK);

    ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 6, false);
    if (ret != 6) {
        printf("ERROR: neokey_init(3) %i\n", ret);
        return;
    }
    // sleep_ms(10);

    // buf[0] = GPIO_BASE;
    // buf[1] = GPIO_PULLENSET;
    // ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 6, false);
    // if (ret != 6) {
    //     printf("ERROR: neokey_init(4) %i\n", ret);
    //     return;
    // }
    // // sleep_ms(10);

    // buf[0] = GPIO_BASE;
    // buf[1] = GPIO_BULK_SET;
    // ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 6, false);
    // if (ret != 6) {
    //     printf("ERROR: neokey_init(5) %i\n", ret);
    //     return;
    // }
    // // sleep_ms(10);

    // buf[0] = GPIO_BASE;
    // buf[1] = GPIO_INTENSET;
    // ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 6, false);
    // if (ret != 6) {
    //     printf("ERROR: neokey_init(6) %i\n", ret);
    //     return;
    // }
    // // sleep_ms(10);

    initialised = true;
    printf("Neokey initialised\n");
}

void write_leds() {
    if (initialised) {

        buf[0] = NEOPIXEL_BASE;
        buf[1] = NEOPIXEL_BUF;
        buf[2] = 0;
        buf[3] = 0;

        for (int i = 0; i < 12; i++) { buf[i + 4] = leds[i]; }

        int ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 16, false);
        if (ret != 16) {
            printf("ERROR: write_leds %i\n", ret);
            initialised = false;
            return;
        }
    }
}

void show_leds() {
    if (initialised) {
        buf[0] = NEOPIXEL_BASE;
        buf[1] = NEOPIXEL_SHOW;

        int ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 2, false);
        if (ret != 2) {
            printf("ERROR: show_leds %i\n", ret);
            initialised = false;
            return;
        }
    }
}

uint8_t read_keys_raw() {
    if (initialised) {
        buf[0] = GPIO_BASE;
        buf[1] = GPIO_BULK;
        int ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 2, false);
        if (ret < 0) {
            printf("ERROR: read_keys_raw write: %i\n", ret);
            initialised = false;
            return 0xfe;
        }
        ret = i2c_read_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 4, false);
        if (ret < 0) {
            printf("ERROR: read_keys_raw read: %i\n", ret);
            initialised = false;
            return 0xfd;
        }
        return buf[2] & 0xf0;
    }
    return 0xff;
}

void process_keys(uint8_t buttons_state, uint32_t now) {
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