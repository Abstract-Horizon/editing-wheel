

typedef struct TU_ATTR_PACKED
{
    uint8_t  type;
    uint8_t  sub_type;
    int16_t  value;
} key_action_t;

typedef struct TU_ATTR_PACKED
{
    uint32_t     direction;
    int32_t      zero;
    uint32_t     dividers;
    float        expo;
    float        gain_factor;
    float        dead_band;
    uint8_t      full_resolution;
    uint8_t      padding;
    key_action_t wheel_main;
    key_action_t wheel_alt;
    key_action_t key1;
    key_action_t key2;
    key_action_t key3;
} profile_t;

