#define FW_VERSION "127"

//#define DEBUG

#ifdef DEBUG
#define debug(fmt, ...) printf(fmt, ## __VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define SYSTIME_DIFF(past, now) ((now < past) ? ((((uint32_t)0xFFFFFFFF) - past) + now + ((uint32_t)1)) : (now - past))

#define SCL_PIN 4
#define SDA_PIN 5
#define LED_R_PIN 12
#define LED_G_PIN 14
#define BTN_PIN 13
#define DE_PIN 15

#define BAUDRATE 115200

#define WIFI_AP_SSID "SACI_AP"
#define WIFI_AP_DEFAULT_PASSWORD "SACI1234"

extern char ap_mode;
