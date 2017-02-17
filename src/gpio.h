//  GPIO support

#ifndef GPIO_H__
#define GPIO_H__

void gpio_init();
void gpio_pin_set (uint16_t pin, bool fOn);
void gpio_power_init(uint16_t pin, bool fOn);
void gpio_power_set(uint16_t pin, bool fOn);
bool gpio_power_overcurrent_sensed();

#define MOTION_ARM          0
#define MOTION_DISARM       1
#define MOTION_UPDATE       2
#define MOTION_QUERY        3
#define MOTION_QUERY_PIN    4
bool gpio_motion_sense(uint16_t command);

// UART Selector.  Note that the baud rate is hard-coded for each in gpio.c
#define UART_NONE   0
#define UART_LORA   1   // Sodaq RN2483/RN2409
#define UART_FONA   2   // Adafruit Fona 3G
#define UART_PMS    3   // Plantower PMS3003
#define UART_GPS    4   // Adafruit Ultimate GPS
void gpio_uart_select(uint16_t which_comm);
uint16_t gpio_current_uart();

// Color LEDs, and the two states that alternately flash A/B
#define GPS                 0x00000001
#define COMM                0x00000002
#define MODE                0x000000FF
#define RED_SOLID           0x00000300
#define RED_FLASH           0x00000100
#define YEL_SOLID           0x00030000
#define YEL_FLASH           0x00010000
#define BOTH_SOLID          0x00030300
#define BOTH_FLASH          0x00010100
#define BOTH_ALTERNATE      0x00020100
#define BLACK               0x00000000
#define INDICATE_CHARGING               (COMM | YEL_SOLID)
#define INDICATE_GPS_CONNECTING         (GPS | BOTH_ALTERNATE)
#define INDICATE_GPS_CONNECTED          (GPS | BLACK)
#define INDICATE_CELL_INITIALIZING      (COMM | RED_FLASH)
#define INDICATE_CELL_NO_SERVICE        (COMM | RED_SOLID)
#define INDICATE_CELL_CONNECTED         (COMM | BLACK)
#define INDICATE_COMMS_STATE_UNKNOWN    (COMM | BOTH_SOLID)
#define INDICATE_GPS_STATE_UNKNOWN      (GPS | BOTH_SOLID)
#define INDICATE_LORA_INITIALIZING      (COMM | YEL_FLASH)
#define INDICATE_LORA_CONNECTED         (COMM | BLACK)
#define INDICATE_LORAWAN_INITIALIZING   (COMM | BOTH_FLASH)
#define INDICATE_LORAWAN_CONNECTED      (COMM | BLACK)
#define INDICATE_BLINKY                 0xFFFFFFFF
void gpio_indicate(uint32_t what);
void gpio_indicators_off();
void gpio_indicator_no_longer_needed(uint32_t what);

#endif // GPIO_H__
