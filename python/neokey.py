#!/usr/bin/env python3
import time
from typing import Optional, List

from smbus2 import SMBus, i2c_msg

i2c_bus = SMBus(1)

DEFAULT_NEOKEY_I2C_ADDRESS = 0x30


class Neokey1x4:
    _GPIO_BASE = 0x01
    _GPIO_DIRSET_BULK = 0x02
    _GPIO_DIRCLR_BULK = 0x03
    _GPIO_BULK = 0x04
    _GPIO_BULK_SET = 0x05
    _GPIO_BULK_CLR = 0x06
    _GPIO_BULK_TOGGLE = 0x07
    _GPIO_INTENSET = 0x08
    _GPIO_INTENCLR = 0x09
    _GPIO_INTFLAG = 0x0A
    _GPIO_PULLENSET = 0x0B
    _GPIO_PULLENCLR = 0x0C

    _KEYPAD_BASE = 0x10

    _KEYPAD_STATUS = 0x00
    _KEYPAD_EVENT = 0x01
    _KEYPAD_INTENSET = 0x02
    _KEYPAD_INTENCLR = 0x03
    _KEYPAD_COUNT = 0x04
    _KEYPAD_FIFO = 0x10

    _BUTTON_A = 4
    _BUTTON_B = 5
    _BUTTON_C = 6
    _BUTTON_D = 7
    _BUTTON_MASK = (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7)

    _NEOPIXEL_BASE = 0x0E

    _NEOPIXEL_STATUS = 0x00
    _NEOPIXEL_PIN = 0x01
    _NEOPIXEL_SPEED = 0x02
    _NEOPIXEL_BUF_LENGTH = 0x03
    _NEOPIXEL_BUF = 0x04
    _NEOPIXEL_SHOW = 0x05

    def __init__(self, i2c_bus: Optional[SMBus] = None, i2c_address = DEFAULT_NEOKEY_I2C_ADDRESS):
        self.i2c_bus = i2c_bus if i2c_bus is not None else SMBus(1)
        self.i2c_address = i2c_address
        self.leds = [[0, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 0]]

    def write(self, buf: List[int]) -> None:
        i2c_bus.i2c_rdwr(i2c_msg.write(self.i2c_address, buf))
        print(f"write: {buf}")

    def read(self, reg_base: int, reg: int, number_of_bytes_to_read: int) -> List[int]:
        buf = [reg_base, reg]
        read_data = i2c_msg.read(self.i2c_address, number_of_bytes_to_read)
        i2c_bus.i2c_rdwr(i2c_msg.write(self.i2c_address, buf))
        i2c_bus.i2c_rdwr(read_data)
        data = list(read_data)
        print(f"read: {buf} -> {data}")
        return list(read_data)

    def write_byte(self, reg_base: int, reg: int, value: int) -> None:
        buf = [reg_base, reg, value]
        print(f"write_byte: {buf}")
        i2c_bus.i2c_rdwr(i2c_msg.write(self.i2c_address, buf))

    def read_byte(self, reg_base: int, reg: int) -> int:
        buf = [reg_base, reg]
        read_data = i2c_msg.read(self.i2c_address, 1)
        i2c_bus.i2c_rdwr(i2c_msg.write(self.i2c_address, buf), read_data)

        print(f"read_byte: {buf}")

        res = list(read_data)
        if len(res) > 0:
            return res[0]

        return 0

    def init(self) -> None:
        self.write([self._NEOPIXEL_BASE, self._NEOPIXEL_PIN, 3])
        self.write([self._NEOPIXEL_BASE, self._NEOPIXEL_BUF_LENGTH, 0, 12])

        pins = self._BUTTON_MASK
        cmd = [(pins & 0xff000000) >> 24, (pins & 0xff0000) >> 16, (pins & 0xff00) >> 8, pins]
        print(f"pins={bin(pins)}; cmd={cmd}")

        self.write([self._GPIO_BASE, self._GPIO_DIRCLR_BULK] + cmd)
        self.write([self._GPIO_BASE, self._GPIO_PULLENSET] + cmd)
        self.write([self._GPIO_BASE, self._GPIO_BULK_SET] + cmd)

        self.write([self._GPIO_BASE, self._GPIO_INTENSET] + cmd)

        # self.write([self._KEYPAD_BASE, self._KEYPAD_EVENT, self._BUTTON_A, 1])
        # self.write([self._KEYPAD_BASE, self._KEYPAD_EVENT, self._BUTTON_B, 1])
        # self.write([self._KEYPAD_BASE, self._KEYPAD_EVENT, self._BUTTON_C, 1])
        # self.write([self._KEYPAD_BASE, self._KEYPAD_EVENT, self._BUTTON_D, 1])

    def set_led(self, pos: int, r: int, g: int, b: int) -> None:
        if 0 <= pos < 4:
            self.leds[pos][0] = g
            self.leds[pos][1] = r
            self.leds[pos][2] = b

    def update_leds(self) -> None:
        # self.write([self._NEOPIXEL_BASE, self._NEOPIXEL_BUF, 0, 0, self.leds[0][0], self.leds[0][1], self.leds[0][2]])
        # self.write([self._NEOPIXEL_BASE, self._NEOPIXEL_BUF, 0, 3, self.leds[1][0], self.leds[1][1], self.leds[1][2]])
        # self.write([self._NEOPIXEL_BASE, self._NEOPIXEL_BUF, 0, 6, self.leds[2][0], self.leds[2][1], self.leds[2][2]])
        # self.write([self._NEOPIXEL_BASE, self._NEOPIXEL_BUF, 0, 9, self.leds[3][0], self.leds[3][1], self.leds[3][2]])

        self.write(
            [self._NEOPIXEL_BASE, self._NEOPIXEL_BUF, 0, 0,
             self.leds[0][0], self.leds[0][1], self.leds[0][2],
             self.leds[1][0], self.leds[1][1], self.leds[1][2],
             self.leds[2][0], self.leds[2][1], self.leds[2][2],
             self.leds[3][0], self.leds[3][1], self.leds[3][2]])
        self.write([self._NEOPIXEL_BASE, self._NEOPIXEL_SHOW])

    def read_keys(self, num: int) -> List[int]:
        return self.read(self._GPIO_BASE, self._GPIO_BULK, 4)


if __name__ == '__main__':
    neokey = Neokey1x4()
    neokey.init()

    colours = [(0x20, 0, 0), (0x20, 0x20, 0), (0, 0x20, 0), (0, 0x20, 0x20), (0, 0, 0x20), (0x20, 0, 0x20)]

    last_keys = [0, 0, 0, 0]
    p = 0
    while True:
        pp = p
        for i in range(4):
            neokey.set_led(i, colours[pp][0], colours[pp][1], colours[pp][2])
            pp = (pp + 1) if pp < len(colours) -1 else 0
        neokey.update_leds()

        p = (p + 1) if p < len(colours) -1 else 0
        time.sleep(0.01)
        keys = neokey.read_keys(4)
        if last_keys[3] != keys[3]:
            last_keys[3] = keys[3]
            print(f"Keys: {keys[0]:08b}, {keys[1]:08b}, {keys[2]:08b}, {keys[3]:08b}")
