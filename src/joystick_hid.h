#include "common/tusb_common.h"


typedef struct TU_ATTR_PACKED
{
  int16_t  x;        ///< Delta x  movement of left analog-stick
  int16_t  y;        ///< Delta y  movement of left analog-stick
  int16_t  z;        ///< Delta z  movement of right analog-joystick
  uint32_t buttons;  ///< Buttons mask for currently pressed buttons
}hid_joystick_report_t;

