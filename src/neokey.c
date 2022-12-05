
#include <string.h>
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "hardware/i2c.h"
#include "neokey.h"

#define DEBUG_INIT 0
#define DEBUG_READ_KEYS 0
#define DEBUG_WRITE_LEDS 0
#define DEBUG_PROCESS_KEYS 0

uint8_t buttons[4] = {0, 0, 0, 0};
volatile uint8_t buttons_state = 0;
volatile uint8_t leds[12] = {0x20, 0, 0x20, 0x20, 0, 0, 0x20, 0x20, 0, 0, 0x20, 0};
// volatile uint8_t leds[12] = {0, 0x20, 0, 0x20, 0x20, 0, 0x20, 0, 0, 0x20, 0, 0x20};

static uint8_t buf[20];

static uint32_t initialised = false;

static neokey_key_t keys[4] = {
  {  .bt_mask = (1 << BUTTON_A), .bt_prev_state = (1 << BUTTON_A), .st_current_state = ST_RELEASED, .has_change = 0, .key_state = KEY_RELEASED },
  {  .bt_mask = (1 << BUTTON_B), .bt_prev_state = (1 << BUTTON_B), .st_current_state = ST_RELEASED, .has_change = 0, .key_state = KEY_RELEASED },
  {  .bt_mask = (1 << BUTTON_C), .bt_prev_state = (1 << BUTTON_C), .st_current_state = ST_RELEASED, .has_change = 0, .key_state = KEY_RELEASED },
  {  .bt_mask = (1 << BUTTON_D), .bt_prev_state = (1 << BUTTON_D), .st_current_state = ST_RELEASED, .has_change = 0, .key_state = KEY_RELEASED }
};

static neokey_key_t akey = { .time = 0, .bt_mask = (1 << BUTTON_A), .bt_prev_state = (1 << BUTTON_A), .st_current_state = ST_RELEASED, .has_change = 0, .key_state = KEY_RELEASED };

void neokey_init() {
    int ret;

    buf[0] = STATUS_BASE;
    buf[1] = STATUS_HW_ID;
    #if (DEBUG_INIT)
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
    #if (DEBUG_INIT)
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
    #if (DEBUG_INIT)
        printf("Neokey ");
    #else
        printf(", ");
    #endif
    printf("version %i, (%i, %i)\n", rec[0] << 8 | rec[1], rec[2], rec[3]);


    buf[0] = NEOPIXEL_BASE;
    buf[1] = NEOPIXEL_PIN;
    buf[2] = 3;
    #if (DEBUG_INIT)
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
    #if (DEBUG_INIT)
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
    #if (DEBUG_INIT)
        printf("neokey_init write: [%i, %i, %i, %i, %i, %i]\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
    #endif
    ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 6, false);
    if (ret != 6) {
        printf("ERROR: neokey_init(gpio dirclr) %i\n", ret);
        return;
    }

    buf[1] = GPIO_PULLENSET;
    ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 6, false);
    #if (DEBUG_INIT)
        printf("neokey_init write: [%i, %i, %i, %i, %i, %i]\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
    #endif
    if (ret != 6) {
        printf("ERROR: neokey_init(gpio pullenset) %i\n", ret);
        return;
    }

    buf[1] = GPIO_BULK_SET;
    ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 6, false);
    #if (DEBUG_INIT)
        printf("neokey_init write: [%i, %i, %i, %i, %i, %i]\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
    #endif
    if (ret != 6) {
        printf("ERROR: neokey_init(gpio bulkset) %i\n", ret);
        return;
    }

    buf[1] = GPIO_INTENSET;
    ret = i2c_write_blocking(i2c_default, NEOKEY_I2C_ADDRESS, buf, 6, false);
    #if (DEBUG_INIT)
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

        #if (DEBUG_WRITE_LEDS)
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
        #if (DEBUG_WRITE_LEDS)
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
        #if (DEBUG_READ_KEYS)
            printf("read_keys_raw: [%i, %i] -> ", buf[0], buf[1]);
        #endif

        sleep_ms(2);
        uint8_t rec[4];
        ret = i2c_read_blocking(i2c_default, NEOKEY_I2C_ADDRESS, rec, 4, false);
        if (ret != 4) {
        #if (DEBUG_READ_KEYS)
                printf("\n");
            #endif
            printf("ERROR: read_keys_raw read: %i\n", ret);
            initialised = false;
            buttons_state = 0xfd;
            return;
        }
        #if (DEBUG_READ_KEYS)
            printf("[%i, %i, %i, %i]\n", rec[0], rec[1], rec[2], rec[3]);
        #endif
        buttons_state = rec[3] & 0xf0;
        for (int i = 0; i < 4; i++) { buttons[i] = rec[i]; }
    } else {
        buttons_state = 0xff;
    }
}

void set_key_state(neokey_key_t* key, int key_state) {
    if (key->key_state != key_state) {
        key->key_state = key_state;
        key->has_change = 1;
    }
}

void process_keys(uint32_t now) {
    for (int i = 0; i < 4; i++) {
        neokey_key_t* key = &keys[i];
        uint8_t new_bt_state = buttons_state & key->bt_mask;
        #if (DEBUG_PROCESS_KEYS)
            if (i == 0) {
                if (key->bt_prev_state != new_bt_state) {
                    printf("process_keys (key=%i): %i != %i, state=%i\n", i, key->bt_prev_state, new_bt_state, key->st_current_state);
                }
            }
        #endif
        switch (key->st_current_state) {
            case (ST_RELEASED): {
                if (new_bt_state == 0) {
                    key->time = now;
                    key->st_current_state = ST_PRESSED_DEBOUNCE;
                    set_key_state(key, KEY_DOWN);
                }
            }
            break;
            case (ST_PRESSED_DEBOUNCE): {
                if (new_bt_state != 0) {
                    key->st_current_state = ST_RELEASED;
                    set_key_state(key, KEY_UP);
                } else {
                    if (now - key->time > KEY_DEBOUNCE_TIME) {
                        key->st_current_state = ST_PRESSED;
                        set_key_state(key, KEY_PRESSED);
                    }
                }
            }
            break;
            case (ST_PRESSED): {
                if (new_bt_state != 0) {
                    key->time = now;
                    key->st_current_state = ST_RELEASED_DEBOUNCE;
                    set_key_state(key, KEY_UP);
                } else {
                    if (now - key->time > KEY_LONG_PRESS_TIME) {
                        set_key_state(key, KEY_LONG_PRESSED);
                        key->st_current_state = ST_LONG_PRESS;
                    }
                }
            }
            break;
            case (ST_LONG_PRESS): {
                if (new_bt_state != 0) {
                    key->time = now;
                    key->st_current_state = ST_RELEASED_DEBOUNCE_LONG_PRESS;
                    set_key_state(key, KEY_UP);
                }
            }
            break;
            case (ST_RELEASED_DEBOUNCE_LONG_PRESS): {
                if (new_bt_state == 0) {
                    key->st_current_state = ST_LONG_PRESS;
                    set_key_state(key, KEY_DOWN);
                } else {
                    if (now - key->time > KEY_DEBOUNCE_TIME) {
                        key->st_current_state = ST_RELEASED;
                        set_key_state(key, KEY_RELEASED);
                    }
                }
            }
            break;
            case (ST_RELEASED_DEBOUNCE): {
                if (new_bt_state == 0) {
                    key->st_current_state = ST_PRESSED;
                    set_key_state(key, KEY_DOWN);
                } else {
                    if (now - key->time > KEY_DEBOUNCE_TIME) {
                        key->st_current_state = ST_RELEASED;
                        set_key_state(key, KEY_RELEASED);
                    }
                }
            }
            break;
            default: break;
        }

        if (key->bt_prev_state != new_bt_state) {
            key->bt_prev_state = new_bt_state;
        }
    }
}

int get_key_state(uint32_t key_num) {
    if (key_num >= 0 && key_num <= 3) {
        neokey_key_t* key = &keys[key_num];
        if (key->has_change) {
            key->has_change = 0;
            return key->key_state;
        }
    }
    return -1;
}
