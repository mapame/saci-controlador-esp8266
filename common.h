#define FW_VERSION "161"

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

#define MAX_MODULE_QTY 32

#define WIFI_AP_SSID "SACI_AP"

extern const char custom_code_version[];

extern int ap_mode;

extern int config_diagnostic_mode;
