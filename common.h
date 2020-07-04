#define FW_VERSION "0.1.4"

#define DEBUG

#ifdef DEBUG
#define debug(fmt, ...) printf(fmt, ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define SCL_PIN 4
#define SDA_PIN 5
#define LED_R_PIN 12
#define LED_G_PIN 14
#define BTN_PIN 13
#define DE_PIN 15

#define BAUDRATE 115200

#define I2C_BUS 0

#define RTC_READ_PERIOD_US 60U * 60U * 1000000U

#define RTC_MAX_READ_PERIOD_US 70U * 60U * 1000000U

#define WIFI_AP_SSID "CMA_AP"
#define WIFI_AP_DEFAULT_PASSWORD "CMA00000"

//extern uint8_t status_sampling_running;
