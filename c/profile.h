

typedef struct TU_ATTR_PACKED
{
  uint32_t direction;
  int32_t  zero;
  uint32_t dividers;
  float    expo;
  float    gain_factor;
  float    dead_band;
} profile_t;

