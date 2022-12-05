
#ifndef NEOKEY_H__
#define NEOKEY_H__

#define NEOKEY_I2C_ADDRESS 0x30

#define BUTTON_A 4
#define BUTTON_B 5
#define BUTTON_C 6
#define BUTTON_D 7
#define BUTTON_MASK (1 << BUTTON_A) | (1 << BUTTON_B) | (1 << BUTTON_C) | (1 << BUTTON_D)


#define STATUS_BASE 0x00
#define STATUS_HW_ID 0x01
#define STATUS_VERSION 0x02
#define STATUS_OPTIONS 0x03
#define STATUS_TEMP 0x04
#define STATUS_SWRST 0x7F


#define GPIO_BASE 0x01
#define GPIO_DIRSET_BULK 0x02
#define GPIO_DIRCLR_BULK 0x03
#define GPIO_BULK 0x04
#define GPIO_BULK_SET 0x05
#define GPIO_BULK_CLR 0x06
#define GPIO_BULK_TOGGLE 0x07
#define GPIO_INTENSET 0x08
#define GPIO_INTENCLR 0x09
#define GPIO_INTFLAG 0x0A
#define GPIO_PULLENSET 0x0B
#define GPIO_PULLENCLR 0x0C

#define NEOPIXEL_BASE 0x0E
#define NEOPIXEL_STATUS 0x00
#define NEOPIXEL_PIN 0x01
#define NEOPIXEL_SPEED 0x02
#define NEOPIXEL_BUF_LENGTH 0x03
#define NEOPIXEL_BUF 0x04
#define NEOPIXEL_SHOW 0x05

#define KEY_DEBOUNCE_TIME 150
#define KEY_LONG_PRESS_TIME 750

enum {
    KEY_DOWN,
    KEY_RELEASED,
    KEY_UP,
    KEY_PRESSED,
    KEY_LONG_PRESSED,
};

typedef struct TU_ATTR_PACKED
{
  uint32_t time;
  uint8_t bt_mask;
  uint8_t bt_prev_state;
  uint8_t st_current_state;
  uint8_t has_change;
  int key_state;
} neokey_key_t;

enum {
  ST_RELEASED = 1,
  ST_PRESSED_DEBOUNCE,
  ST_PRESSED,
  ST_LONG_PRESS,
  ST_RELEASED_DEBOUNCE,
  ST_RELEASED_DEBOUNCE_LONG_PRESS
};

#endif /* NEOKEY_H__ */
