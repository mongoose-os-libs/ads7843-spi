#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define START_REG_BIT 7
#define CHNL_REG_BIT 4
#define ADC_MODE_12BIT_REG_BIT 3
#define SINGLE_ENDED_ADC_REG_BIT 2
#define POWER_DOWN_MODE_REG_BIT 0

enum mgos_ads7843_touch_t {
  TOUCH_DOWN = 0,
  TOUCH_UP = 1,
};

enum mgos_ads7843_orientation_t {
  PORTRAIT = 0,
  LANDSCAPE = 1,
};

#define MAX_ADC_VALUE 127

struct mgos_ads7843_event_data {
  enum mgos_ads7843_touch_t direction;  // Either TOUCH_DOWN or TOUCH_UP
  float down_seconds;  // The time in seconds that the touch screen has been
                       // held down.
  uint16_t x;           // The x position in screen pixels
  uint16_t y;           // The y position in screen pixels
  uint8_t x_adc;        // The x pos (ADC value not pixels)
  uint8_t y_adc;        // The y pos (ADC value not pixels)
  uint8_t orientation;  // Either PORTRAIT or LANDSCAPE
};

typedef void (*mgos_ads7843_event_t)(struct mgos_ads7843_event_data *);

bool mgos_ads7843_spi_init(void);
bool mgos_ads7843_is_touching();
void mgos_ads7843_set_handler(mgos_ads7843_event_t handler);
void mgos_ads7843_set_dimensions(uint16_t x, uint16_t y);

uint16_t get_min_landscape_width_cal_value(void);
uint16_t get_min_landscape_height_cal_value(void);
uint16_t get_max_landscape_width_cal_value(void);
uint16_t get_max_landscape_height_cal_value(void);

void set_min_landscape_width_cal_value(uint16_t _min_landscape_width_cal_value);
void set_min_landscape_height_cal_value(
    uint16_t _min_landscape_height_cal_value);
void set_max_landscape_width_cal_value(uint16_t _min_landscape_width_cal_value);
void set_max_landscape_height_cal_value(
    uint16_t _min_landscape_height_cal_value);

uint16_t get_screen_rotation(void);
void set_screen_rotation(uint16_t _screen_rotation);

#ifdef __cplusplus
}
#endif
