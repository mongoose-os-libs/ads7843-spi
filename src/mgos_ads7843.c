/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mgos.h"
#include "mgos_spi.h"
#include "mgos_ads7843.h"

static uint16_t s_max_x = 0;
static uint16_t s_max_y = 0;

static float initial_down_seconds = 0.0;

static uint8_t screen_orentation = PORTRAIT;

static mgos_ads7843_event_t s_event_handler = NULL;
static struct mgos_ads7843_event_data event_data;
static void ads7843_irh(int pin, void *arg);
static int8_t get_touch_xpos(bool x);
static void ads7843_irh(int pin, void *arg);

static float min_adc_x;
static float max_adc_x;
static float adc_x_range;

static float min_adc_y;
static float max_adc_y;
static float adc_y_range;

/**
 * @brief Get the touch position on the x or y axis.
 * @param x If true then the x position is read. If false then the y position is read
 * @return An 8 bit ADC value that changes with the position on the selected axis that the user touches.
           Given the size of displays that this library is used with, 8 bit resolution is enough and it's
           quicker to read the data.
 **/
static int8_t get_touch_xpos(bool x) {
    int8_t rx_data = 0;
    int8_t a2a1a0 = 0;
    struct mgos_spi *spi = mgos_spi_get_global();

    if (!spi) {
      LOG(LL_ERROR, ("Cannot get global SPI bus"));
      return -1;
    }

    int8_t cmd_byte = 1<<START_REG_BIT | 0<<ADC_MODE_12BIT_REG_BIT | 0<<SINGLE_ENDED_ADC_REG_BIT | 0<<POWER_DOWN_MODE_REG_BIT;
    cmd_byte&=0xff;

    if( x ) {
      a2a1a0 = 1;
    }
    else {
      a2a1a0 = 5;
    }
    cmd_byte |= a2a1a0<<CHNL_REG_BIT;

    struct mgos_spi_txn txn = {
      .cs   = mgos_sys_config_get_ads7843_cs_index(),
      .mode = 3,
      .freq =500000,
    };
    txn.hd.tx_data   = &cmd_byte;
    txn.hd.tx_len    = 1;
    txn.hd.rx_data   = &rx_data;
    txn.hd.rx_len    = 1;
    if (!mgos_spi_run_txn(spi, false, &txn)) {
      LOG(LL_ERROR, ("SPI transaction failed"));
      return -2;
    }

    return rx_data;
}

/**
 * @brief Called if the touch screen is down to ensure we catch the release.
 * @param arg Not used.
 **/
static void ads7843_down_cb(void *arg) {
  ads7843_irh(mgos_sys_config_get_ads7843_irq_pin(), NULL);
}

/**
 * @brief called to dispatch a call to the touch event handler function
 * @param arg Not used.
 **/
static void dispatch_s_event_handler(void *arg) {

  if( s_event_handler ) {
    s_event_handler(&event_data);
  }
}

/**
 * @brief Get the screen orientation.
 **/
uint16_t get_screen_orientation(void) {
  if( s_max_x >= s_max_y ) {
    return LANDSCAPE;
  }
  return PORTRAIT;
}

/**
 * @brief Called when the ads7843 irq pin goes low or high
 * @param The pin that created the event
 * @param arg Not used.
 **/
static void ads7843_irh(int pin, void *arg) {
  int loop_count      = 0;
  uint8_t x_pos       = 0;
  uint8_t y_pos       = 0;
  float x_factor      = 0.0;
  float y_factor      = 0.0;
  uint16_t x_pixels   = 0;
  uint16_t y_pixels   = 0;

  mgos_gpio_disable_int( mgos_sys_config_get_ads7843_irq_pin() );

  //We don't expect to loop through all itterations but this ensures we do exit
  //even if we only get max adc values.
  while( loop_count < 200 ) {
      //These values are ADC values with the top left of the screen in landcape mode
      x_pos = get_touch_xpos(true);
      y_pos = get_touch_xpos(false);

      //We may get spurious max readings particularly for higher SPI clock rates.
      //so ignore these.
      if( (x_pos != MAX_ADC_VALUE && y_pos != MAX_ADC_VALUE) ) {

        if( screen_orentation == PORTRAIT ) {

          uint8_t tmp = x_pos;
          x_pos = y_pos;
          y_pos = MAX_ADC_VALUE-tmp;

        }

        if( mgos_sys_config_get_ads7843_flip_x_y() ) {
          x_pos = MAX_ADC_VALUE-x_pos;
          y_pos = MAX_ADC_VALUE-y_pos;
        }

        if( x_pos >= min_adc_x ) {
          x_factor = ((float)x_pos-min_adc_x)/adc_x_range;
        }
        else {
          x_factor=0.0;
        }
        x_pixels = s_max_x*x_factor;

        if( y_pos >= min_adc_y ) {
          y_factor = ((float)y_pos-min_adc_y)/adc_y_range;
        }
        else {
          y_factor=0.0;
        }
        y_pixels = s_max_y*y_factor;

        break;
      }
  }

  bool irq_high = mgos_gpio_read(mgos_sys_config_get_ads7843_irq_pin());
  if( !irq_high ) {
    event_data.direction = TOUCH_DOWN;
    event_data.x_adc=x_pos;
    event_data.y_adc=y_pos;
    event_data.x = x_pixels;
    event_data.y = y_pixels;
    if( initial_down_seconds <= 0.0 ) {
      initial_down_seconds=mgos_uptime();
    }
    else {
      event_data.down_seconds = mgos_uptime()-initial_down_seconds;
    }
    // To avoid DOWN events without an UP event, set a timer
    mgos_set_timer(100, 0, ads7843_down_cb, NULL);
  }
  else {
    event_data.direction = TOUCH_UP;
    initial_down_seconds = 0.0;
    //We don't reset the event_data.down_seconds so that
    //we preserve the histor of the last down time which
    //may be needed.
    mgos_gpio_enable_int(mgos_sys_config_get_ads7843_irq_pin());
  }
  event_data.orientation = get_screen_orientation();
  //callback to to ensure we do not call the handler from the interrupt context
  mgos_set_timer(0, 0, dispatch_s_event_handler, NULL);

  (void) pin;
  (void) arg;
}

/**
 * @brief Perform all required initialisation actions.
 **/
bool mgos_ads7843_spi_init(void) {

  mgos_gpio_set_mode(mgos_sys_config_get_ads7843_irq_pin(), MGOS_GPIO_MODE_INPUT);
  mgos_gpio_set_pull(mgos_sys_config_get_ads7843_irq_pin(), MGOS_GPIO_PULL_UP);
  mgos_gpio_set_int_handler(mgos_sys_config_get_ads7843_irq_pin(), MGOS_GPIO_INT_EDGE_ANY, ads7843_irh, NULL);
  mgos_gpio_enable_int(mgos_sys_config_get_ads7843_irq_pin());

  event_data.down_seconds = 0.0;
  initial_down_seconds = 0.0;

  s_max_x = mgos_sys_config_get_ads7843_x_pixels();
  s_max_y = mgos_sys_config_get_ads7843_y_pixels();
  screen_orentation = get_screen_orientation();

  min_adc_x = mgos_sys_config_get_ads7843_min_x_adc();
  max_adc_x = mgos_sys_config_get_ads7843_max_x_adc();
  adc_x_range = max_adc_x-min_adc_x;

  min_adc_y = mgos_sys_config_get_ads7843_min_y_adc();
  max_adc_y = mgos_sys_config_get_ads7843_max_y_adc();
  adc_y_range = max_adc_y-min_adc_y;

  LOG(LL_INFO, ("s_max_x=%d, s_max_y=%d, screen_orentation=%s", s_max_x, s_max_y, screen_orentation ? "PORTRAIT" : "LANDSCAPE"));
  LOG(LL_INFO, ("min_adc_x=%.f, max_adc_x=%.f, adc_x_range=%.f", min_adc_x, max_adc_x, adc_x_range));
  LOG(LL_INFO, ("min_adc_y=%.f, max_adc_y=%.f, adc_y_range=%.f", min_adc_y, max_adc_y, adc_y_range));

  return true;
}

/**
 * @return true if the touch screen is currently touched.
 **/
bool mgos_ads7843_is_touching() {
  return event_data.direction == TOUCH_DOWN;
}

/**
 * @brief Set the handler function that will be called when touch down/up events occur.
 **/
void mgos_ads7843_set_handler(mgos_ads7843_event_t handler) {
  s_event_handler = handler;
}

/**
 * @brief Set the dimensions of the display that the touch screen is connected to
 * @param x The horizontal axis pixel count.
 * @param y The vertical axis pixel count.
 **/
void mgos_ads7843_set_dimensions(uint16_t x, uint16_t y) {
  s_max_x = x;
  s_max_y = y;
  screen_orentation = get_screen_orientation();
}
