/************************************************************************
 *                                                                      *
 *  This source code needs to be compiled for the board                 *
 *  NodeMCU 1.0 (ESP-12E Module)                                        *
 *                                                                      *
 ************************************************************************
 *                                                                      *
 *    airRohr firmware                                                  *
 *    Copyright (C) 2016-2018  Code for Stuttgart a.o.                  *
 *                                                                      *
 * This program is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program. If not, see <http://www.gnu.org/licenses/>. *
 *                                                                      *
 ************************************************************************
 * OK LAB Particulate Matter Sensor                                     *
 *      - nodemcu-LoLin board                                           *
 *      - Nova SDS0111                                                  *
 *  http://inovafitness.com/en/Laser-PM2-5-Sensor-SDS011-35.html        *
 *                                                                      *
 * Wiring Instruction:                                                  *
 *      - SDS011 Pin 1  (TX)   -> Pin D1 / GPIO5                        *
 *      - SDS011 Pin 2  (RX)   -> Pin D2 / GPIO4                        *
 *      - SDS011 Pin 3  (GND)  -> GND                                   *
 *      - SDS011 Pin 4  (2.5m) -> unused                                *
 *      - SDS011 Pin 5  (5V)   -> VU                                    *
 *      - SDS011 Pin 6  (1m)   -> unused                                *
 *                                                                      *
 ************************************************************************
 *                                                                      *
 * Alternative                                                          *
 *      - nodemcu-LoLin board                                           *
 *      - Shinyei PPD42NS                                               *
 *      http://www.sca-shinyei.com/pdf/PPD42NS.pdf                      *
 *                                                                      *
 * Wiring Instruction:                                                  *
 *      Pin 2 of dust sensor PM2.5 -> Digital 6 (PWM)                   *
 *      Pin 3 of dust sensor       -> +5V                               *
 *      Pin 4 of dust sensor PM1   -> Digital 3 (PMW)                   *
 *                                                                      *
 *      - PPD42NS Pin 1 (grey or green)  => GND                         *
 *      - PPD42NS Pin 2 (green or white)) => Pin D5 /GPIO14             *
 *        counts particles PM25                                         *
 *      - PPD42NS Pin 3 (black or yellow) => Vin                        *
 *      - PPD42NS Pin 4 (white or black) => Pin D6 / GPIO12             *
 *        counts particles PM10                                         *
 *      - PPD42NS Pin 5 (red)   => unused                               *
 *                                                                      *
 ************************************************************************
 * Extension: DHT22 (AM2303)                                            *
 *  http://www.aosong.com/en/products/details.asp?id=117                *
 *                                                                      *
 * DHT22 Wiring Instruction                                             *
 * (left to right, front is perforated side):                           *
 *      - DHT22 Pin 1 (VDD)     -> Pin 3V3 (3.3V)                       *
 *      - DHT22 Pin 2 (DATA)    -> Pin D7 (GPIO13)                      *
 *      - DHT22 Pin 3 (NULL)    -> unused                               *
 *      - DHT22 Pin 4 (GND)     -> Pin GND                              *
 *                                                                      *
 ************************************************************************
 * Extensions connected via I2C:                                        *
 * HTU21D (https://www.sparkfun.com/products/13763),                    *
 * BMP180, BMP280, BME280, OLED Display with SSD1306 (128x64 px)        *
 *                                                                      *
 * Wiring Instruction                                                   *
 * (see labels on display or sensor board)                              *
 *      VCC       ->     Pin 3V3                                        *
 *      GND       ->     Pin GND                                        *
 *      SCL       ->     Pin D4 (GPIO2)                                 *
 *      SDA       ->     Pin D3 (GPIO0)                                 *
 *                                                                      *
 ************************************************************************
 *                                                                      *
 *  Powersave mode                                                      *
 *
 *    TODO:
 *     - do measurements
 *     - fix problem in normal mode (activating the SDS sensor fan fails in current version)
 *     - testing
 *     - add a switch to disable powersafe mode in the WebUI
 *     - reduce the number of update checks to once a day (move counter to rtcMemory)
 *     - modularize loop() and reuse the code
 *     - fix mDNS crash or remove mDNS (see https://github.com/esp8266/Arduino/issues/4417)
 *     - clean up debug messages and log levels
 *     - remove dependencies of sensor reading functions to global data
 *       (which will be cleared by deepSleep): starttime, act_milli
 *     - add brownout detection to protect LiIon battery
 *       for LOLIN D32 PRO board: batteryVoltage = analogRead(35) / 4096.0 * 7.445;
 *       see also https://www.esp32.com/viewtopic.php?t=2462
 *         #include "soc/soc.h"
 *         #include "soc/rtc_cntl_reg.h"
 *         WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
 *     - use wifi_status_led_uninstall() to disable status led?
 *     - use system_get_rtc_time() after deep sleep to get a time reference
 *       when using deep sleep?
 *     - use sntp only every several hours
 *
 * State Machine
 *   - current step stored in RTC memory
 * 
 *   step 0 (initial power on): // WiFi off
 *     if(confPowerSave && !checkPowerSafeDisabledJumper())
 *       nextStep=101 temporary normal mode, then powersave mode
 *     else
 *       nextStep=100 (normal mode)
 *     deepSleep(1000, RF_DEFAULT)
 *
 *   state 1: Sensor Warmup // WiFi off, 
 *     if(checkPowerSafeDisabledJumper())
 *       nextStep=100 (normal mode)
 *       deepSleep(1000, RF_DEFAULT)
 *     start sensors
 *     nextStep=2
 *     deepSleep(WARMUPTIME_SDS_MS + READINGTIME_SDS_MS, RF_DEFAULT)
 *
 *   state 2: Read sensors and send data // requires WiFi
 *     if(checkPowerSafeDisabledJumper())
 *       nextStep=100 (normal mode)
 *       deepSleep(1000, RF_DEFAULT)
 *     read sensors
 *     stop sensors
 *     activate WiFi
 *     send values
 *     deactivate WiFi
 *     nextStep=1
 *     deepSleep(cfg::sending_intervall_ms - (WARMUPTIME_SDS_MS + READINGTIME_SDS_MS), RF_DISABLED)
 *
 *   state 100: Normal mode // requires WiFi
 *     normal mode (for unlimited time)
 *
 *   state 101: Temporary normal mode // requires WiFi
 *     normal mode for 5 to 10 minutes // make it possible to access the web interface
 *     if(checkPowerSafeDisabledJumper())
 *       nextStep=100 (normal mode)
 *       deepSleep(1000, RF_DEFAULT)
 *     after 5 minutes
 *       nextStep=1
 *       deepSleep(1000, RF_DEFAULT)
 ************************************************************************
 *                                                                      *
 * Please check Readme.md for other sensors and hardware                *
 *                                                                      *
 ************************************************************************
 *
 * latest mit lib 2.4.2
 * Der Sketch verwendet 511608 Bytes (48%) des Programmspeicherplatzes. Das Maximum sind 1044464 Bytes.
 * Globale Variablen verwenden 37264 Bytes (45%) des dynamischen Speichers, 44656 Bytes für lokale Variablen verbleiben. Das Maximum sind 81920 Bytes.
 *
 * Der Sketch verwendet 506304 Bytes (48%) des Programmspeicherplatzes. Das Maximum sind 1044464 Bytes.
 * Globale Variablen verwenden 37128 Bytes (45%) des dynamischen Speichers, 44792 Bytes für lokale Variablen verbleiben. Das Maximum sind 81920 Bytes.
 * 
 * Der Sketch verwendet 505504 Bytes (48%) des Programmspeicherplatzes. Das Maximum sind 1044464 Bytes.
 * Globale Variablen verwenden 37128 Bytes (45%) des dynamischen Speichers, 44792 Bytes für lokale Variablen verbleiben. Das Maximum sind 81920 Bytes.
 * 
 * Attention:
 * the driver for the Sensirion PM Sensor SPS30 needs I²C lransmission of 64 Bytes
 *
 * in Wire.h set BUFFER_LENGTH to 64 !!!!
 *
 * #define BUFFER_LENGTH 64
 *
 *
 *
 ************************************************************************/
// increment on change
#define SOFTWARE_VERSION "NRZ-2019-124-B6"

/*****************************************************************
 * Includes                                                      *
 *****************************************************************/

#ifndef INTL_DE
#define INTL_DE
#endif

#include <Arduino.h>

#if defined(ESP8266)
#include <FS.h>                     // must be first
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266httpUpdate.h>
#include <SoftwareSerial.h>
#include "./oledfont.h"				// avoids including the default Arial font, needs to be included before SSD1306.h
#include <SSD1306.h>
#include <SH1106.h>
#include <LiquidCrystal_I2C.h>
#include <base64.h>
#include <ArduinoJson.h>
#include "./DHT.h"
#include <Adafruit_HTU21DF.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_BME280.h>
#include <DallasTemperature.h>
#include <TinyGPS++.h>
#include <time.h>
#include <coredecls.h>
#include <assert.h>
#include <Hash.h>
#include "./sps30_i2c.h"
#include "./dnms_i2c.h"
#endif

#if defined(ESP32)
#define FORMAT_SPIFFS_IF_FAILED true
#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HardwareSerial.h>
#include <hwcrypto/sha.h>
#include <HTTPUpdate.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <SSD1306.h>
#include <SH1106.h>
#include <LiquidCrystal_I2C.h>
#include <base64.h>
#include <ArduinoJson.h>
#include "./DHT.h"
#include <Adafruit_HTU21DF.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_BME280.h>
#include <DallasTemperature.h>
#include <TinyGPS++.h>
#include "./sps30_i2c.h"
#include "./dnms_i2c.h"

#endif

#if defined(INTL_BG)
#include "intl_bg.h"
#elif defined(INTL_CZ)
#include "intl_cz.h"
#elif defined(INTL_EN)
#include "intl_en.h"
#elif defined(INTL_ES)
#include "intl_es.h"
#elif defined(INTL_FR)
#include "intl_fr.h"
#elif defined(INTL_IT)
#include "intl_it.h"
#elif defined(INTL_LU)
#include "intl_lu.h"
#elif defined(INTL_NL)
#include "intl_nl.h"
#elif defined(INTL_PL)
#include "intl_pl.h"
#elif defined(INTL_PT)
#include "intl_pt.h"
#elif defined(INTL_RU)
#include "intl_ru.h"
#elif defined(INTL_SE)
#include "intl_se.h"
#else
#include "intl_de.h"
#endif

#include "defines.h"
#include "ext_def.h"
#include "html-content.h"

/******************************************************************
 * Constants                                                      *
 ******************************************************************/
const unsigned long SAMPLETIME_MS = 30000;									// time between two measurements of the PPD42NS
const unsigned long SAMPLETIME_SDS_MS = 1000;								// time between two measurements of the SDS011, PMSx003, Honeywell PM sensor
const unsigned long WARMUPTIME_SDS_MS = 15000;								// time needed to "warm up" the sensor before we can take the first measurement
const unsigned long READINGTIME_SDS_MS = 5000;								// how long we read data from the PM sensors
const unsigned long SAMPLETIME_GPS_MS = 50;
const unsigned long DISPLAY_UPDATE_INTERVAL_MS = 5000;						// time between switching display to next "screen"
const unsigned long ONE_DAY_IN_MS = 24 * 60 * 60 * 1000;
const unsigned long PAUSE_BETWEEN_UPDATE_ATTEMPTS_MS = ONE_DAY_IN_MS;		// check for firmware updates once a day
const unsigned long DURATION_BEFORE_FORCED_RESTART_MS = ONE_DAY_IN_MS * 28;	// force a reboot every ~4 weeks

/******************************************************************
 * The variables inside the cfg namespace are persistent          *
 * configuration values. They have defaults which can be          *
 * configured at compile-time via the ext_def.h file              *
 * They can be changed by the user via the web interface, the     *
 * changes are persisted to the flash and read back after reboot. *
 * Note that the names of these variables can't be easily changed *
 * as they are part of the json format used to persist the data.  *
 ******************************************************************/
namespace cfg {
	char current_lang[3] = "DE";

	// wifi credentials
	char wlanssid[LEN_WLANSSID] = WLANSSID;
	char wlanpwd[LEN_WLANPWD] = WLANPWD;

	// credentials for basic auth of internal web server
	char www_username[LEN_WWW_USERNAME] = WWW_USERNAME;
	char www_password[LEN_WWW_PASSWORD] = WWW_PASSWORD;
	bool www_basicauth_enabled = WWW_BASICAUTH_ENABLED;

	// credentials of the sensor in access point mode
	char fs_ssid[LEN_FS_SSID] = FS_SSID;
	char fs_pwd[LEN_FS_PWD] = FS_PWD;

	char version_from_local_config[20] = "";

	// (in)active sensors
	bool dht_read = DHT_READ;
	bool htu21d_read = HTU21D_READ;
	bool ppd_read = PPD_READ;
	bool sds_read = SDS_READ;
	bool pms_read = PMS_READ;
	bool hpm_read = HPM_READ;
	bool sps30_read = SPS30_READ;
	bool bmp_read = BMP_READ;
	bool bmp280_read = BMP280_READ;
	bool bme280_read = BME280_READ;
	bool ds18b20_read = DS18B20_READ;
	bool dnms_read = DNMS_READ;
	char dnms_correction[LEN_DNMS_CORRECTION] = DNMS_CORRECTION;
	bool gps_read = GPS_READ;

	// send to "APIs"
	bool send2dusti = SEND2DUSTI;
	bool send2madavi = SEND2MADAVI;
	bool send2sensemap = SEND2SENSEMAP;
	bool send2fsapp = SEND2FSAPP;
	bool send2aircms = SEND2AIRCMS;
	bool send2custom = SEND2CUSTOM;
	bool send2lora = SEND2LORA;
	bool send2influx = SEND2INFLUX;
	bool send2csv = SEND2CSV;

	bool auto_update = AUTO_UPDATE;
	bool use_beta = USE_BETA;

	// (in)active displays
	bool has_display = HAS_DISPLAY;											// OLED with SSD1306 and I2C
	bool has_sh1106 = HAS_SH1106;
	bool has_flipped_display = HAS_FLIPPED_DISPLAY;
	bool has_lcd1602 = HAS_LCD1602;
	bool has_lcd1602_27 = HAS_LCD1602_27;
	bool has_lcd2004_27 = HAS_LCD2004_27;
	
	bool display_wifi_info = DISPLAY_WIFI_INFO;
	bool display_device_info = DISPLAY_DEVICE_INFO;

	int  debug = DEBUG;

	// API settings
	bool ssl_madavi = SSL_MADAVI;
	bool ssl_dusti = SSL_DUSTI;
	char senseboxid[LEN_SENSEBOXID] = SENSEBOXID;

	char host_influx[LEN_HOST_INFLUX] = HOST_INFLUX;
	char url_influx[LEN_URL_INFLUX] = URL_INFLUX;
	int port_influx = PORT_INFLUX;
	char user_influx[LEN_USER_INFLUX] = USER_INFLUX;
	char pwd_influx[LEN_PWD_INFLUX] = PWD_INFLUX;
	char measurement_name_influx[LEN_MEASUREMENT_NAME_INFLUX] = MEASUREMENT_NAME_INFLUX;
	bool ssl_influx = SSL_INFLUX;

	char host_custom[LEN_HOST_CUSTOM] = HOST_CUSTOM;
	char url_custom[LEN_URL_CUSTOM] = URL_CUSTOM;
	int port_custom = PORT_CUSTOM;
	char user_custom[LEN_USER_CUSTOM] = USER_CUSTOM;
	char pwd_custom[LEN_PWD_CUSTOM] = PWD_CUSTOM;
	bool ssl_custom = SSL_CUSTOM;

	unsigned long time_for_wifi_config = 600000;
	unsigned long sending_intervall_ms = 145000;

	void initNonTrivials(const char* id) {
		strcpy(cfg::current_lang, CURRENT_LANG);
		if (fs_ssid[0] == '\0') {
			strcpy(fs_ssid, "airRohr-");
			strcat(fs_ssid, id);
		}
	}
}

#define HOST_MADAVI "api-rrd.madavi.de"
#define URL_MADAVI "/data.php"
#define PORT_MADAVI 80

#define HOST_DUSTI "api.sensor.community"
#define URL_DUSTI "/v1/push-sensor-data/"
#define PORT_DUSTI 80

// IMPORTANT: NO MORE CHANGES TO VARIABLE NAMES NEEDED FOR EXTERNAL APIS

#define HOST_SENSEMAP "ingress.opensensemap.org"
#define URL_SENSEMAP "/boxes/{v}/data?luftdaten=1"
#define PORT_SENSEMAP 443

#define HOST_FSAPP "www.h2801469.stratoserver.net"
#define URL_FSAPP "/data.php"
#define PORT_FSAPP 80

#define HOST_AIRCMS "doiot.ru"
#define URL_AIRCMS "/php/sensors.php?h="
#define PORT_AIRCMS 443

#define UPDATE_HOST "firmware.sensor.community"
#define UPDATE_URL "/airrohr/firmware.php"
#define UPDATE_PORT 80

#define JSON_BUFFER_SIZE 2300

String empty_String;

enum class PmSensorCmd {
	Start,
	Stop,
	ContinuousMode,
	VersionDate,
	QueryData
};

String basic_auth_influx;
String basic_auth_custom;

long int sample_count = 0;
bool bmp_init_failed = false;
bool bmp280_init_failed = false;
bool bme280_init_failed = false;

#if defined(ESP8266)
ESP8266WebServer server(80);
#endif
#if defined(ESP32)
WebServer server(80);
#endif

int TimeZone = 1;

/*****************************************************************
 * Function prototypes (to make it cpp compatible in VS Code)    *
 *****************************************************************/
String Float2String(const double value);
String Float2String(const double value, uint8_t digits);
void writeConfig();
String add_sensor_type(const String& sensor_text);
void webserver_not_found();
void deepSleep(uint32_t us, bool switchOnWiFiOnRestart = true);
static bool acquireNetworkTime();
static void autoUpdate();
static void powerOnTestSensors(bool dontInitPmSensors);
static unsigned long sendDataToOptionalApis(const String &data);

/*****************************************************************
 * Variables for Noise Measurement DNMS                          *
 *****************************************************************/
bool dnms_init_failed = 0;
float last_value_dnms_laeq = -1.0;
float last_value_dnms_la_min = -1.0;
float last_value_dnms_la_max = -1.0;
float dnms_leq_a = -1.0;
float dnms_leq_a_min = -1.0;
float dnms_leq_a_max = -1.0;
String dnms_sensor;
char dnms_version[DNMS_MAX_VERSION_LEN];
uint16_t data_ready;
bool dnms_error = false;
struct dnms_measurements dnms_values;
int16_t ret_dnms;
float dnms_corr_value = 0;

/*****************************************************************
 * Display definitions                                           *
 *****************************************************************/
SSD1306 display(0x3c, I2C_PIN_SDA, I2C_PIN_SCL);
SH1106 display_sh1106(0x3c, I2C_PIN_SDA, I2C_PIN_SCL);
LiquidCrystal_I2C lcd_1602_27(0x27, 16, 2);
LiquidCrystal_I2C lcd_1602_3f(0x3F, 16, 2);
LiquidCrystal_I2C lcd_2004_27(0x27, 20, 4);

/*****************************************************************
 * SDS011 declarations                                           *
 *****************************************************************/
#if defined(ESP8266)
SoftwareSerial serialSDS(PM_SERIAL_RX, PM_SERIAL_TX, false, 128);
SoftwareSerial serialGPS(GPS_SERIAL_RX, GPS_SERIAL_TX, false, 512);
#endif
#if defined(ESP32)
HardwareSerial serialSDS(2);
HardwareSerial serialGPS(3);
#endif

/*****************************************************************
 * DHT declaration                                               *
 *****************************************************************/
DHT dht(ONEWIRE_PIN, DHT_TYPE);

/*****************************************************************
 * HTU21D declaration                                            *
 *****************************************************************/
Adafruit_HTU21DF htu21d;

/*****************************************************************
 * BMP declaration                                               *
 *****************************************************************/
Adafruit_BMP085 bmp;

/*****************************************************************
 * BMP280 declaration                                               *
 *****************************************************************/
Adafruit_BMP280 bmp280;

/*****************************************************************
 * BME280 declaration                                            *
 *****************************************************************/
Adafruit_BME280 bme280;

/*****************************************************************
 * DS18B20 declaration                                            *
 *****************************************************************/
OneWire oneWire(ONEWIRE_PIN);
DallasTemperature ds18b20(&oneWire);

/*****************************************************************
 * GPS declaration                                               *
 *****************************************************************/
TinyGPSPlus gps;

/*****************************************************************
 * Variable Definitions for PPD24NS                              *
 * P1 for PM10 & P2 for PM25                                     *
 *****************************************************************/

unsigned long durationP1;
unsigned long durationP2;

boolean trigP1 = false;
boolean trigP2 = false;
unsigned long trigOnP1;
unsigned long trigOnP2;

unsigned long lowpulseoccupancyP1 = 0;
unsigned long lowpulseoccupancyP2 = 0;

bool send_now = false;
unsigned long starttime;
unsigned long time_point_device_start_ms;
unsigned long starttime_SDS;
unsigned long starttime_GPS;
unsigned long act_micro;
unsigned long act_milli;
unsigned long last_micro = 0;
unsigned long min_micro = 1000000000;
unsigned long max_micro = 0;

bool is_SDS_running = true;
bool is_PMS_running = true;
bool is_HPM_running = true;

unsigned long sending_time = 0;
unsigned long last_update_attempt;

float last_value_BMP_T = -128.0;
float last_value_BMP_P = -1.0;
float last_value_BMP280_T = -128.0;
float last_value_BMP280_P = -1.0;
float last_value_BME280_T = -128.0;
float last_value_BME280_H = -1.0;
float last_value_BME280_P = -1.0;
float last_value_DHT_T = -128.0;
float last_value_DHT_H = -1.0;

int sds_pm10_sum = 0;
int sds_pm25_sum = 0;
int sds_val_count = 0;
int sds_pm10_max = 0;
int sds_pm10_min = 20000;
int sds_pm25_max = 0;
int sds_pm25_min = 20000;

int pms_pm1_sum = 0;
int pms_pm10_sum = 0;
int pms_pm25_sum = 0;
int pms_val_count = 0;
int pms_pm1_max = 0;
int pms_pm1_min = 20000;
int pms_pm10_max = 0;
int pms_pm10_min = 20000;
int pms_pm25_max = 0;
int pms_pm25_min = 20000;

int hpm_pm10_sum = 0;
int hpm_pm25_sum = 0;
int hpm_val_count = 0;
int hpm_pm10_max = 0;
int hpm_pm10_min = 20000;
int hpm_pm25_max = 0;
int hpm_pm25_min = 20000;

float last_value_SPS30_P0 = -1.0;
float last_value_SPS30_P1 = -1.0;
float last_value_SPS30_P2 = -1.0;
float last_value_SPS30_P3 = -1.0;
float last_value_SPS30_N0 = -1.0;
float last_value_SPS30_N1 = -1.0;
float last_value_SPS30_N2 = -1.0;
float last_value_SPS30_N3 = -1.0;
float last_value_SPS30_N4 = -1.0;
float last_value_SPS30_TS = -1.0;
float value_SPS30_P0 = 0.0;
float value_SPS30_P1 = 0.0;
float value_SPS30_P2 = 0.0;
float value_SPS30_P3 = 0.0;
float value_SPS30_N0 = 0.0;
float value_SPS30_N1 = 0.0;
float value_SPS30_N2 = 0.0;
float value_SPS30_N3 = 0.0;
float value_SPS30_N4 = 0.0;
float value_SPS30_TS = 0.0;


uint16_t  SPS30_measurement_count = 0;
unsigned long SPS30_read_counter = 0;
unsigned long SPS30_read_error_counter = 0;
struct sps30_measurement sps30_values;
unsigned long SPS30_read_timer = 0;
bool sps30_init_failed = false;

double last_value_PPD_P1 = -1.0;
double last_value_PPD_P2 = -1.0;
double last_value_SDS_P1 = -1.0;
double last_value_SDS_P2 = -1.0;
double last_value_PMS_P0 = -1.0;
double last_value_PMS_P1 = -1.0;
double last_value_PMS_P2 = -1.0;
double last_value_HPM_P1 = -1.0;
double last_value_HPM_P2 = -1.0;
double last_value_HTU21D_T = -128.0;
double last_value_HTU21D_H = -1.0;
double last_value_DS18B20_T = -1.0;
double last_value_GPS_lat = -200.0;
double last_value_GPS_lon = -200.0;
double last_value_GPS_alt = -1000.0;
String last_value_GPS_date;
String last_value_GPS_time;
String last_data_string;

String esp_chipid;

String mac_addr_sta;

unsigned long last_page_load = millis();

bool wificonfig_loop = false;

bool first_cycle = true;

bool sntp_time_is_set = false;

bool got_ntp = false;
bool networkInitialized = false;

unsigned long count_sends = 0;
unsigned long next_display_millis = 0;
unsigned long next_display_count = 0;

struct struct_wifiInfo {
	char ssid[35];
	uint8_t encryptionType;
	int32_t RSSI;
	int32_t channel;
#if defined(ESP8266)
	bool isHidden;
#endif
};

struct struct_wifiInfo *wifiInfo;
uint8_t count_wifiInfo;

template<typename T, std::size_t N> constexpr std::size_t array_num_elements(const T(&)[N]) {
	return N;
}

template<typename T, std::size_t N> constexpr std::size_t capacity_null_terminated_char_array(const T(&)[N]) {
	return N - 1;
}

#define msSince(timestamp_before) (act_milli - (timestamp_before))

const char data_first_part[] PROGMEM = "{\"software_version\": \"{v}\", \"sensordatavalues\":[";

/*****************************************************************
 * Debug output                                                  *
 *****************************************************************/

#define debug_level_check if(level > cfg::debug) return;

void debug_out(const String& text, const int level) { debug_level_check; Serial.print(text); }
void debug_out_bool(const bool text, const int level) { debug_level_check; Serial.print(String(text)); }
void debug_out(const __FlashStringHelper* text, const int level) { debug_level_check; Serial.print(text); }

void debug_outln(const String& text, const int level) { debug_level_check; Serial.println(text); }
void debug_outln_bool(const bool text, const int level) { debug_level_check; Serial.println(String(text)); }
void debug_outln(const __FlashStringHelper* text, const int level) { debug_level_check; Serial.println(text); }

#undef debug_level_check

/*****************************************************************
 * display values                                                *
 *****************************************************************/
void display_debug(const String& text1, const String& text2) {
	debug_outln(F("output debug text to displays..."), DEBUG_MIN_INFO);
	debug_outln(text1 + "\n" + text2, DEBUG_MAX_INFO);
	if (cfg::has_display) {
		display.clear();
		display.displayOn();
		display.setTextAlignment(TEXT_ALIGN_LEFT);
		display.drawString(0, 12, text1);
		display.drawString(0, 24, text2);
		display.display();
	}
	if (cfg::has_sh1106) {
		display_sh1106.clear();
		display_sh1106.displayOn();
		display_sh1106.setTextAlignment(TEXT_ALIGN_LEFT);
		display_sh1106.drawString(0, 12, text1);
		display_sh1106.drawString(0, 24, text2);
		display_sh1106.display();
	}
	if (cfg::has_lcd1602) {
		lcd_1602_3f.clear();
		lcd_1602_3f.setCursor(0, 0);
		lcd_1602_3f.print(text1);
		lcd_1602_3f.setCursor(0, 1);
		lcd_1602_3f.print(text2);
	}
	if (cfg::has_lcd1602_27) {
		lcd_1602_27.clear();
		lcd_1602_27.setCursor(0, 0);
		lcd_1602_27.print(text1);
		lcd_1602_27.setCursor(0, 1);
		lcd_1602_27.print(text2);
	}
	if (cfg::has_lcd2004_27) {
		lcd_2004_27.clear();
		lcd_2004_27.setCursor(0, 0);
		lcd_2004_27.print(text1);
		lcd_2004_27.setCursor(0, 1);
		lcd_2004_27.print(text2);
	}
}

/*****************************************************************
 * convert float to string with a                                *
 * precision of two (or a given number of) decimal places        *
 *****************************************************************/
String Float2String(const double value, uint8_t digits) {
	// Convert a float to String with two decimals.
	char temp[15];

	dtostrf(value, 13, digits, temp);
	String s = temp;
	s.trim();
	return s;
}

String Float2String(const double value) {
	return Float2String(value, 2);
}

/*****************************************************************
 * check display values, return '-' if undefined                 *
 *****************************************************************/
String check_display_value(double value, double undef, uint8_t len, uint8_t str_len) {
	String s = (value != undef ? Float2String(value, len) : "-");
	while (s.length() < str_len) {
		s = " " + s;
	}
	return s;
}

/*****************************************************************
 * convert value to json string                                  *
 *****************************************************************/
String Value2Json(const __FlashStringHelper* type, const String& value) {
	String s = F("{\"value_type\":\"{t}\",\"value\":\"{v}\"},");
	s.replace("{t}", String(type));
	s.replace("{v}", value);
	return s;
}

String Value2Json(const __FlashStringHelper* type, const float& value) {
	return Value2Json(type, Float2String(value));
}

/*****************************************************************
 * convert string value to json string                           *
 *****************************************************************/
String Var2Json(const String& name, const String& value) {
	String s = FPSTR(WEB_REPLN_REPLV);
	String tmp = value;
	tmp.replace("\\", "\\\\"); tmp.replace("\"", "\\\"");
	s.replace("{n}", name);
	s.replace("{v}", tmp);
	return s;
}

/*****************************************************************
 * convert boolean value to json string                          *
 *****************************************************************/
String Var2Json(const String& name, const bool value) {
	return Var2Json(name, String(value ? "true" : "false"));
}

/*****************************************************************
 * convert boolean value to json string                          *
 *****************************************************************/
String Var2Json(const String& name, const int value) {
	return Var2Json(name, String(value));
}

#define rtcOffset (0) // this range is cleared after an OTA update which is good!
#define rtcLen (sizeof(RTCData))
class RTCData {
private:

	// when adding new members, set the initial values here
	// this is also used when the RTC didn't contain a valid crc
	void initValues() {
		stateMachine = 101;
	}

public:
	// all data members are saved to the RTC, so only add members which need to be saved
	uint32_t crc32;
	uint16_t stateMachine;

	void save() {
		updateCRC();
		ESP.rtcUserMemoryWrite(rtcOffset, (uint32_t*) this, rtcLen);
	}

	void load() {
		if(ESP.rtcUserMemoryRead(rtcOffset, (uint32_t*) this, rtcLen)) {
			if(!checkCRC()) {
				initValues();
				debug_outln(FPSTR("Saved RTC data invalid"), DEBUG_MIN_INFO);
			}
		}		
	}

private:
	bool checkCRC() {
		return calcRtcDataCRC() == crc32;
	}

	void updateCRC() {
		crc32 = calcRtcDataCRC();
	}

	uint32_t calcRtcDataCRC() {
		uint32_t crc32_read = crc32; // save for later
		crc32 = 0; // this is part of the calculated value, set it to a defined value
		uint32_t crc32_calc = crc32Buffer(reinterpret_cast<const uint8_t*>(this), rtcLen);
		crc32 = crc32_read; // restore
		return crc32_calc;
	}

	uint32_t crc32Buffer(const uint8_t *data, size_t length) {
		uint32_t crc = 0xffffffff;
		while (length--) {
			uint8_t c = *data++;
			for (uint32_t i = 0x80; i > 0; i >>= 1) {
				bool bit = crc & 0x80000000;
				if (c & i) {
					bit = !bit;
				}
				crc <<= 1;
				if (bit) {
					crc ^= 0x04c11db7;
				}
			}
		}
		return crc;
	}
};
// TODO: check the data size against the size of the usable RTC memory (excluding system area and depending on ESP8266/ESP32)
//#if (rtcLen > 512)
//#error "RTC data size too large"
//#endif
RTCData rtcData;

void switchSensors(bool on, bool force)
{
	if (cfg::sds_read && (force || is_SDS_running != on))
		SDS_cmd(on ? PmSensorCmd::Start : PmSensorCmd::Stop);

	if (cfg::pms_read && (force || is_PMS_running != on))
		PMS_cmd(on ? PmSensorCmd::Start : PmSensorCmd::Stop);

	if (cfg::hpm_read && (force || is_HPM_running != on))
		HPM_cmd(on ? PmSensorCmd::Start : PmSensorCmd::Stop);
}

void deepSleep(uint32_t us, bool switchOnWiFiOnRestart) {
	wdt_disable();
	WiFi.disconnect(true);
	WiFi.mode(WIFI_OFF);

	// Setting any global variables in this function would only makes sense when
	// ESP.deepSleep() would not completely reset the CPU. Nevertheless for
	// the sake of code readability those variables are set here:
	//networkInitialized = false;
	//got_ntp = false;

	rtcData.save();

	// for issues with sleep mode RF_DISABLED refer to these links:
	//   https://github.com/esp8266/Arduino/issues/3072#issuecomment-348692479
	//   https://blog.creations.de/?p=149
	ESP.deepSleep(us, switchOnWiFiOnRestart ? RF_DEFAULT : RF_DISABLED);
}

extern "C" void loop_StateMeasureAndSend() {
	String result_PPD, result_SDS, result_PMS, result_HPM, result_SPS30;
	String result_DHT, result_HTU21D, result_BMP, result_BMP280;
	String result_BME280, result_DS18B20, result_GPS, result_DNMS;
	int16_t ret_SPS30;

	unsigned long sum_send_time = 0;

	act_micro = micros();
	send_now = true;
	sample_count++;

	// fake act_milli so the sensor reading can start
	// because the sensor functions contain the following check:
	//if (msSince(starttime) > (cfg::sending_intervall_ms - READINGTIME_SDS_MS))
	act_milli = starttime + ((cfg::sending_intervall_ms - READINGTIME_SDS_MS) + 1);

#if defined(ESP8266)
	wdt_reset(); // nodemcu is alive
#endif

	debug_outln(FPSTR("loop_StateMeasureAndSend"), DEBUG_MIN_INFO);

	// the PM sensors have been initialized in the previous states of the state machine
	powerOnTestSensors(false);
	is_SDS_running = cfg::sds_read;
	is_PMS_running = cfg::pms_read;
	is_HPM_running = cfg::hpm_read;

	if (cfg::sps30_read && ( !sps30_init_failed)) {
		//if ((msSince(starttime) - SPS30_read_timer) > SPS30_WAITING_AFTER_LAST_READ)
		{
			SPS30_read_timer = msSince(starttime);
			ret_SPS30 = sps30_read_measurement(&sps30_values);
			++SPS30_read_counter;
			if (ret_SPS30 < 0) {
				debug_outln(F("SPS30 error reading measurement"), DEBUG_MIN_INFO);
				SPS30_read_error_counter++;
			} else {
				if (SPS_IS_ERR_STATE(ret_SPS30)) {
					debug_outln(F("SPS30 measurements may not be accurate"), DEBUG_MIN_INFO);
					SPS30_read_error_counter++;
				}
				value_SPS30_P0 += sps30_values.mc_1p0;
				value_SPS30_P1 += sps30_values.mc_2p5;
				value_SPS30_P2 += sps30_values.mc_4p0;
				value_SPS30_P3 += sps30_values.mc_10p0;
				value_SPS30_N0 += sps30_values.nc_0p5;
				value_SPS30_N1 += sps30_values.nc_1p0;
				value_SPS30_N2 += sps30_values.nc_2p5;
				value_SPS30_N3 += sps30_values.nc_4p0;
				value_SPS30_N4 += sps30_values.nc_10p0;
				value_SPS30_TS += sps30_values.tps;
				++SPS30_measurement_count;
			}
		}
	}

	if (cfg::ppd_read) {
		debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + "PPD", DEBUG_MAX_INFO);
		result_PPD = sensorPPD();
	}

	if ((msSince(starttime_SDS) > SAMPLETIME_SDS_MS) || send_now) {
		if (cfg::sds_read) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + "SDS", DEBUG_MAX_INFO);
			result_SDS = sensorSDS();
			starttime_SDS = act_milli;
		}

		if (cfg::pms_read) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + "PMS", DEBUG_MAX_INFO);
			result_PMS = sensorPMS();
			starttime_SDS = act_milli;
		}

		if (cfg::hpm_read) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + "HPM", DEBUG_MAX_INFO);
			result_HPM = sensorHPM();
			starttime_SDS = act_milli;
		}
	}
	switchSensors(false, false); // turn sensors off as early as possible

	if (send_now) {
		if (cfg::dht_read) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + FPSTR(SENSORS_DHT22), DEBUG_MAX_INFO);
			result_DHT = sensorDHT();						// getting temperature and humidity (optional)
		}

		if (cfg::htu21d_read) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + FPSTR(SENSORS_HTU21D), DEBUG_MAX_INFO);
			result_HTU21D = sensorHTU21D();					// getting temperature and humidity (optional)
		}

		if (cfg::bmp_read && (! bmp_init_failed)) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + FPSTR(SENSORS_BMP180), DEBUG_MAX_INFO);
			result_BMP = sensorBMP();						// getting temperature and pressure (optional)
		}

		if (cfg::bmp280_read && (! bmp280_init_failed)) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + FPSTR(SENSORS_BMP280), DEBUG_MAX_INFO);
			result_BMP280 = sensorBMP280();					// getting temperature, humidity and pressure (optional)
		}

		if (cfg::bme280_read && (! bme280_init_failed)) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + FPSTR(SENSORS_BME280), DEBUG_MAX_INFO);
			result_BME280 = sensorBME280();					// getting temperature, humidity and pressure (optional)
		}

		if (cfg::ds18b20_read) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + FPSTR(SENSORS_DS18B20), DEBUG_MAX_INFO);
			result_DS18B20 = sensorDS18B20();				// getting temperature (optional)
		}

		if (cfg::sps30_read && (! sps30_init_failed)) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + FPSTR(SENSORS_SPS30), DEBUG_MAX_INFO);
			result_SPS30 = sensorSPS30();               // getting PM values
		}

		if (cfg::dnms_read && (! dnms_init_failed)) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + FPSTR(SENSORS_DNMS), DEBUG_MAX_INFO);
			result_DNMS = sensorDNMS();                 // getting noise measurement values from dnms (optional)
		}
	}

	if (cfg::gps_read && ((msSince(starttime_GPS) > SAMPLETIME_GPS_MS) || send_now)) {
		debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + "GPS", DEBUG_MAX_INFO);
		result_GPS = sensorGPS();							// getting GPS coordinates
		starttime_GPS = act_milli;
	}

	if ((cfg::has_display || cfg::has_sh1106 || cfg::has_lcd2004_27 || cfg::has_lcd1602 ||
			cfg::has_lcd1602_27) && (act_milli > next_display_millis)) {
		display_values();
	}

	if (send_now) {
		debug_outln(F("Creating data string:"), DEBUG_MIN_INFO);
		String data = tmpl(FPSTR(data_first_part), SOFTWARE_VERSION);
		String data_sample_times = Value2Json(F("samples"), String(sample_count)); // TODO: 'sample_count' will always be 1 so it is useless
		//data_sample_times += Value2Json(F("min_micro"), String(min_micro));
		//data_sample_times += Value2Json(F("max_micro"), String(max_micro));

		const int HTTP_PORT_DUSTI = (cfg::ssl_dusti ? 443 : 80);
		if (cfg::ppd_read) {
			data += result_PPD;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(PPD42NS): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_PPD, PPD_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "PPD_");
			}
		}
		if (cfg::sds_read) {
			data += result_SDS;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(SDS): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_SDS, SDS_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "SDS_");
			}
		}
		if (cfg::pms_read) {
			data += result_PMS;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(PMS): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_PMS, PMS_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "PMS_");
			}
		}
		if (cfg::hpm_read) {
			data += result_HPM;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(HPM): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_HPM, HPM_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "HPM_");
			}
		}
		if (cfg::sps30_read && (! sps30_init_failed)) {
			data += result_SPS30;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(SPS30): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_SPS30, SPS30_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "SPS30_");
			}
		}
		if (cfg::dht_read) {
			data += result_DHT;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(DHT): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_DHT, DHT_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "DHT_");
			}
		}
		if (cfg::htu21d_read) {
			data += result_HTU21D;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(HTU21D): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_HTU21D, HTU21D_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "HTU21D_");
			}
		}
		if (cfg::bmp_read && (! bmp_init_failed)) {
			data += result_BMP;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(BMP): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_BMP, BMP_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "BMP_");
			}
		}
		if (cfg::bmp280_read && (! bmp280_init_failed)) {
			data += result_BMP280;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(BMP280): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_BMP280, BMP280_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "BMP280_");
			}
		}
		if (cfg::bme280_read && (! bme280_init_failed)) {
			data += result_BME280;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(BME280): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_BME280, BME280_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "BME280_");
			}
		}

		if (cfg::ds18b20_read) {
			data += result_DS18B20;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(DS18B20): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_DS18B20, DS18B20_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "DS18B20_");
			}
		}

		if (cfg::dnms_read && (! dnms_init_failed)) {
			data += result_DNMS;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(DNMS): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_DNMS, DNMS_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "DNMS_");
			}
		}

		if (cfg::gps_read) {
			data += result_GPS;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(GPS): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_GPS, GPS_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "GPS_");
			}
		}

		setup_network();
		String signal_strength = String(WiFi.RSSI());
		debug_out(F("WLAN signal strength: "), DEBUG_MIN_INFO);
		debug_out(signal_strength, DEBUG_MIN_INFO);
		debug_outln(" dBm", DEBUG_MIN_INFO);
		debug_outln("----", DEBUG_MIN_INFO);

		data_sample_times += Value2Json(F("signal"), signal_strength);
		data += data_sample_times;

		if ((unsigned)(data.lastIndexOf(',') + 1) == data.length()) {
			data.remove(data.length() - 1);
		}
		data += "]}";

		sum_send_time += sendDataToOptionalApis(data);


		if (msSince(last_update_attempt) > PAUSE_BETWEEN_UPDATE_ATTEMPTS_MS) {
			autoUpdate(); // TODO: this will never be called because the time will never be reached
		}

		sending_time = sum_send_time;
		debug_out(F("Time for sending data (ms): "), DEBUG_MIN_INFO);
		debug_outln(String(sending_time), DEBUG_MIN_INFO);
	}

	rtcData.stateMachine = 1;
	deepSleep(cfg::sending_intervall_ms * 1000, false);
}

extern "C" void loop_StateSensorWarmup() {
#if defined(ESP8266)
	wdt_reset(); // nodemcu is alive
#endif

	debug_outln(FPSTR("loop_StateSensorWarmup"), DEBUG_MIN_INFO);

	// make sure all the sensors are running to warm up
	// fake time sind last starttime so the sensor thinks it is time to start warm up
	//act_milli = starttime + (cfg::sending_intervall_ms - (WARMUPTIME_SDS_MS + READINGTIME_SDS_MS) + 1;
	switchSensors(true, true);
	rtcData.stateMachine = 2;
	//deepSleep((WARMUPTIME_SDS_MS + READINGTIME_SDS_MS) * 1000);
	deepSleep((WARMUPTIME_SDS_MS) * 1000); // this values has been 
	//deepSleep((7000) * 1000); // TODO: this small value seems to work but how about accuracy?
}

extern "C" void loop_StartupTemporaryWebServer() {
#if defined(ESP8266)
	wdt_reset(); // nodemcu is alive
#endif

	debug_outln(FPSTR("loop_StartupTemporaryWebServer"), DEBUG_MIN_INFO);
}

/*****************************************************************
 * send SDS011 command (start, stop, continuous mode, version    *
 *****************************************************************/
static void SDS_cmd(PmSensorCmd cmd) {
	static constexpr uint8_t start_cmd[] PROGMEM = {
		0xAA, 0xB4, 0x06, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x06, 0xAB
	};
	static constexpr uint8_t stop_cmd[] PROGMEM = {
		0xAA, 0xB4, 0x06, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x05, 0xAB
	};
	static constexpr uint8_t continuous_mode_cmd[] PROGMEM = {
		0xAA, 0xB4, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x07, 0xAB
	};
	static constexpr uint8_t version_cmd[] PROGMEM = {
		0xAA, 0xB4, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x05, 0xAB
	};
	/*
	static constexpr uint8_t data_reporting_set_active_cmd[] PROGMEM = {
		0xAA, 0xB4, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x01, 0xAB
	};
	static constexpr uint8_t data_reporting_set_query_cmd[] PROGMEM = {
		0xAA, 0xB4, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x02, 0xAB
	};
	*/
	static constexpr uint8_t query_data_cmd[] PROGMEM = {
		0xAA, 0xB4, 0x04, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x02, 0xAB
	};
	constexpr uint8_t cmd_len = array_num_elements(start_cmd);

	uint8_t buf[cmd_len];
	switch (cmd) {
	case PmSensorCmd::Start:
		memcpy_P(buf, start_cmd, cmd_len);
		is_SDS_running = true;
		break;
	case PmSensorCmd::Stop:
		memcpy_P(buf, stop_cmd, cmd_len);
		is_SDS_running = false;
		break;
	case PmSensorCmd::ContinuousMode:
		memcpy_P(buf, continuous_mode_cmd, cmd_len);
		break;
	case PmSensorCmd::VersionDate:
		memcpy_P(buf, version_cmd, cmd_len);
		break;
	case PmSensorCmd::QueryData:
		memcpy_P(buf, query_data_cmd, cmd_len);
		break;
	}
	serialSDS.write(buf, cmd_len);
}

/*****************************************************************
 * send Plantower PMS sensor command start, stop, cont. mode     *
 *****************************************************************/
static void PMS_cmd(PmSensorCmd cmd) {
	static constexpr uint8_t start_cmd[] PROGMEM = {
		0x42, 0x4D, 0xE4, 0x00, 0x01, 0x01, 0x74
	};
	static constexpr uint8_t stop_cmd[] PROGMEM = {
		0x42, 0x4D, 0xE4, 0x00, 0x00, 0x01, 0x73
	};
	static constexpr uint8_t continuous_mode_cmd[] PROGMEM = {
		0x42, 0x4D, 0xE1, 0x00, 0x01, 0x01, 0x71
	};
	constexpr uint8_t cmd_len = array_num_elements(start_cmd);

	uint8_t buf[cmd_len];
	switch (cmd) {
	case PmSensorCmd::Start:
		memcpy_P(buf, start_cmd, cmd_len);
		is_PMS_running = true;
		break;
	case PmSensorCmd::Stop:
		memcpy_P(buf, stop_cmd, cmd_len);
		is_PMS_running = false;
		break;
	case PmSensorCmd::ContinuousMode:
		memcpy_P(buf, continuous_mode_cmd, cmd_len);
		break;
	case PmSensorCmd::VersionDate:
	case PmSensorCmd::QueryData:
		assert(false && "not supported by this sensor");
		break;
	}
	serialSDS.write(buf, cmd_len);
}

/*****************************************************************
 * send Honeywell PMS sensor command start, stop, cont. mode     *
 *****************************************************************/
static void HPM_cmd(PmSensorCmd cmd) {
	static constexpr uint8_t start_cmd[] PROGMEM = {
		0x68, 0x01, 0x01, 0x96
	};
	static constexpr uint8_t stop_cmd[] PROGMEM = {
		0x68, 0x01, 0x02, 0x95
	};
	static constexpr uint8_t continuous_mode_cmd[] PROGMEM = {
		0x68, 0x01, 0x40, 0x57
	};
	constexpr uint8_t cmd_len = array_num_elements(start_cmd);

	uint8_t buf[cmd_len];
	switch (cmd) {
	case PmSensorCmd::Start:
		memcpy_P(buf, start_cmd, cmd_len);
		is_HPM_running = true;
		break;
	case PmSensorCmd::Stop:
		memcpy_P(buf, stop_cmd, cmd_len);
		is_HPM_running = false;
		break;
	case PmSensorCmd::ContinuousMode:
		memcpy_P(buf, continuous_mode_cmd, cmd_len);
		break;
	case PmSensorCmd::VersionDate:
	case PmSensorCmd::QueryData:
		assert(false && "not supported by this sensor");
		break;
	}
	serialSDS.write(buf, cmd_len);
}

/*****************************************************************
 * read SDS011 sensor serial and firmware date                   *
 *****************************************************************/
String SDS_version_date() {
	char buffer;
	int value;
	int len = 0;
	String s, version_date, device_id;
	int checksum_is = 0;
	int checksum_ok = 0;

	debug_outln(String(FPSTR(DBG_TXT_END_READING)) + FPSTR(DBG_TXT_SDS011_VERSION_DATE), DEBUG_MED_INFO);

	SDS_cmd(PmSensorCmd::Start);

	delay(100);

	SDS_cmd(PmSensorCmd::VersionDate);

	delay(500);

	while (serialSDS.available() > 0) {
		buffer = serialSDS.read();
		debug_outln(String(len) + " - " + String(buffer, DEC) + " - " + String(buffer, HEX) + " - " + int(buffer) + " .", DEBUG_MED_INFO);
//		"aa" = 170, "ab" = 171, "c0" = 192
		value = int(buffer);
		switch (len) {
		case (0):
			if (value != 170) {
				len = -1;
			};
			break;
		case (1):
			if (value != 197) {
				len = -1;
			};
			break;
		case (2):
			if (value != 7) {
				len = -1;
			};
			checksum_is = 7;
			break;
		case (3):
			version_date  = String(value);
			break;
		case (4):
			version_date += "-" + String(value);
			break;
		case (5):
			version_date += "-" + String(value);
			break;
		case (6):
			if (value < 0x10) {
				device_id  = "0" + String(value, HEX);
			} else {
				device_id  = String(value, HEX);
			};
			break;
		case (7):
			if (value < 0x10) {
				device_id += "0";
			};
			device_id += String(value, HEX);
			break;
		case (8):
			debug_out(FPSTR(DBG_TXT_CHECKSUM_IS), DEBUG_MED_INFO);
			debug_out(String(checksum_is % 256), DEBUG_MED_INFO);
			debug_out(FPSTR(DBG_TXT_CHECKSUM_SHOULD), DEBUG_MED_INFO);
			debug_outln(String(value), DEBUG_MED_INFO);
			if (value == (checksum_is % 256)) {
				checksum_ok = 1;
			} else {
				len = -1;
			};
			break;
		case (9):
			if (value != 171) {
				len = -1;
			};
			break;
		}
		if (len > 2) { checksum_is += value; }
		len++;
		if (len == 10 && checksum_ok == 1) {
			s = version_date + "(" + device_id + ")";
			debug_out(F("SDS version date : "), DEBUG_MIN_INFO);
			debug_outln(version_date, DEBUG_MIN_INFO);
			debug_out(F("SDS device ID: "), DEBUG_MIN_INFO);
			debug_outln(device_id, DEBUG_MIN_INFO);
			len = 0;
			checksum_ok = 0;
			version_date = "";
			device_id = "";
			checksum_is = 0;
		}
		yield();
	}

	debug_outln(String(FPSTR(DBG_TXT_END_READING)) + FPSTR(DBG_TXT_SDS011_VERSION_DATE), DEBUG_MED_INFO);

	return s;
}

/*****************************************************************
 * disable unneeded NMEA sentences, TinyGPS++ needs GGA, RMC     *
 *****************************************************************/
void disable_unneeded_nmea() {
	serialGPS.println(F("$PUBX,40,GLL,0,0,0,0*5C"));       // Geographic position, latitude / longitude
//	serialGPS.println(F("$PUBX,40,GGA,0,0,0,0*5A"));       // Global Positioning System Fix Data
	serialGPS.println(F("$PUBX,40,GSA,0,0,0,0*4E"));       // GPS DOP and active satellites
//	serialGPS.println(F("$PUBX,40,RMC,0,0,0,0*47"));       // Recommended minimum specific GPS/Transit data
	serialGPS.println(F("$PUBX,40,GSV,0,0,0,0*59"));       // GNSS satellites in view
	serialGPS.println(F("$PUBX,40,VTG,0,0,0,0*5E"));       // Track made good and ground speed
}

/*****************************************************************
 * read config from spiffs                                       *
 *****************************************************************/
void readConfig() {
	using namespace cfg;
	String json_string;
	bool pms24_read = 0;
	bool pms32_read = 0;

	debug_outln(F("mounting FS..."), DEBUG_MIN_INFO);

#if defined(ESP32)
	bool spiffs_begin_ok = SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED);
#else
	bool spiffs_begin_ok = SPIFFS.begin();
#endif
	if (spiffs_begin_ok) {
		debug_outln(F("mounted file system..."), DEBUG_MIN_INFO);
		if (SPIFFS.exists("/config.json")) {
			//file exists, reading and loading
			debug_outln(F("reading config file..."), DEBUG_MIN_INFO);
			File configFile = SPIFFS.open("/config.json", "r");
			if (configFile) {
				debug_outln(F("opened config file..."), DEBUG_MIN_INFO);
				DynamicJsonDocument json(JSON_BUFFER_SIZE);
				DeserializationError err = deserializeJson(json, configFile);
				if (!err) {
					debug_outln(F("parsed json..."), DEBUG_MIN_INFO);
					if (json.containsKey("SOFTWARE_VERSION")) {
						strcpy(version_from_local_config, json["SOFTWARE_VERSION"]);
					}

#define setFromJSON(key)    if (json.containsKey(#key)) key = json[#key];
#define strcpyFromJSON(key) if (json.containsKey(#key)) strcpy(key, json[#key]);
					strcpyFromJSON(current_lang);
					strcpyFromJSON(wlanssid);
					strcpyFromJSON(wlanpwd);
					strcpyFromJSON(www_username);
					strcpyFromJSON(www_password);
					strcpyFromJSON(fs_ssid);
					strcpyFromJSON(fs_pwd);

					setFromJSON(www_basicauth_enabled);
					setFromJSON(dht_read);
					setFromJSON(htu21d_read);
					setFromJSON(ppd_read);
					setFromJSON(sds_read);
					setFromJSON(pms_read);
					setFromJSON(pms24_read);
					setFromJSON(pms32_read);
					setFromJSON(hpm_read);
					setFromJSON(sps30_read);
					setFromJSON(bmp_read);
					setFromJSON(bmp280_read);
					setFromJSON(bme280_read);
					setFromJSON(ds18b20_read);
					setFromJSON(dnms_read);
					strcpyFromJSON(dnms_correction);
					setFromJSON(gps_read);
					setFromJSON(send2dusti);
					setFromJSON(ssl_dusti);
					setFromJSON(send2madavi);
					setFromJSON(ssl_madavi);
					setFromJSON(send2sensemap);
					setFromJSON(send2fsapp);
					setFromJSON(send2aircms);
					setFromJSON(send2lora);
					setFromJSON(send2csv);
					setFromJSON(auto_update);
					setFromJSON(use_beta);
					setFromJSON(has_display);
					setFromJSON(has_sh1106);
					setFromJSON(has_flipped_display);
					setFromJSON(has_lcd1602);
					setFromJSON(has_lcd1602_27);
					setFromJSON(has_lcd2004_27);
					setFromJSON(display_wifi_info);
					setFromJSON(display_device_info);
					setFromJSON(debug);
					setFromJSON(sending_intervall_ms);
					setFromJSON(time_for_wifi_config);
					strcpyFromJSON(senseboxid);
					if (strcmp(senseboxid, "00112233445566778899aabb") == 0) {
						strcpy(senseboxid, "");
						send2sensemap = 0;
					}
					setFromJSON(send2custom);
					strcpyFromJSON(host_custom);
					strcpyFromJSON(url_custom);
					setFromJSON(port_custom);
					strcpyFromJSON(user_custom);
					strcpyFromJSON(pwd_custom);
					setFromJSON(ssl_custom);
					setFromJSON(send2influx);
					strcpyFromJSON(host_influx);
					strcpyFromJSON(url_influx);
					setFromJSON(port_influx);
					strcpyFromJSON(user_influx);
					strcpyFromJSON(pwd_influx);
					strcpyFromJSON(measurement_name_influx);
					if (strlen(measurement_name_influx) == 0) {
						strcpy(measurement_name_influx, MEASUREMENT_NAME_INFLUX);
					}
					setFromJSON(ssl_influx);
					if (strcmp(host_influx, "api.luftdaten.info") == 0) {
						strcpy(host_influx, "");
						send2influx = 0;
					}
					configFile.close();
					if (pms24_read || pms32_read) {
						pms_read = 1;
						writeConfig();
					}
#undef setFromJSON
#undef strcpyFromJSON
				} else {
					debug_outln(F("failed to load json config"), DEBUG_ERROR);
				}
			}
		} else {
			debug_outln(F("config file not found ..."), DEBUG_ERROR);
		}
	} else {
		debug_outln(F("failed to mount FS"), DEBUG_ERROR);
	}
}

/*****************************************************************
 * write config to spiffs                                        *
 *****************************************************************/
void writeConfig() {
	using namespace cfg;
	String json_string = "{";
	debug_outln(F("saving config..."), DEBUG_MIN_INFO);

#define copyToJSON_Bool(varname) json_string += Var2Json(#varname, varname);
#define copyToJSON_Int(varname) json_string += Var2Json(#varname, varname);
#define copyToJSON_String(varname) json_string += Var2Json(#varname, String(varname));
	copyToJSON_String(current_lang);
	copyToJSON_String(SOFTWARE_VERSION);
	copyToJSON_String(wlanssid);
	copyToJSON_String(wlanpwd);
	copyToJSON_String(www_username);
	copyToJSON_String(www_password);
	copyToJSON_String(fs_ssid);
	copyToJSON_String(fs_pwd);
	copyToJSON_Bool(www_basicauth_enabled);
	copyToJSON_Bool(dht_read);
	copyToJSON_Bool(htu21d_read);
	copyToJSON_Bool(ppd_read);
	copyToJSON_Bool(sds_read);
	copyToJSON_Bool(pms_read);
	copyToJSON_Bool(hpm_read);
	copyToJSON_Bool(sps30_read);
	copyToJSON_Bool(bmp_read);
	copyToJSON_Bool(bmp280_read);
	copyToJSON_Bool(bme280_read);
	copyToJSON_Bool(ds18b20_read);
	copyToJSON_Bool(dnms_read);
	copyToJSON_String(dnms_correction);
	copyToJSON_Bool(gps_read);
	copyToJSON_Bool(send2dusti);
	copyToJSON_Bool(ssl_dusti);
	copyToJSON_Bool(send2madavi);
	copyToJSON_Bool(ssl_madavi);
	copyToJSON_Bool(send2sensemap);
	copyToJSON_Bool(send2fsapp);
	copyToJSON_Bool(send2aircms);
	copyToJSON_Bool(send2lora);
	copyToJSON_Bool(send2csv);
	copyToJSON_Bool(auto_update);
	copyToJSON_Bool(use_beta);
	copyToJSON_Bool(has_display);
	copyToJSON_Bool(has_sh1106);
	copyToJSON_Bool(has_flipped_display);
	copyToJSON_Bool(has_lcd1602);
	copyToJSON_Bool(has_lcd1602_27);
	copyToJSON_Bool(has_lcd2004_27);
	copyToJSON_Bool(display_wifi_info);
	copyToJSON_Bool(display_device_info);
	copyToJSON_String(debug);
	copyToJSON_String(sending_intervall_ms);
	copyToJSON_String(time_for_wifi_config);
	copyToJSON_String(senseboxid);
	copyToJSON_Bool(send2custom);
	copyToJSON_String(host_custom);
	copyToJSON_String(url_custom);
	copyToJSON_Int(port_custom);
	copyToJSON_String(user_custom);
	copyToJSON_String(pwd_custom);
	copyToJSON_Bool(ssl_custom);

	copyToJSON_Bool(send2influx);
	copyToJSON_String(host_influx);
	copyToJSON_String(url_influx);
	copyToJSON_Int(port_influx);
	copyToJSON_String(user_influx);
	copyToJSON_String(pwd_influx);
	copyToJSON_String(measurement_name_influx);
	copyToJSON_Bool(ssl_influx);
#undef copyToJSON_Bool
#undef copyToJSON_Int
#undef copyToJSON_String

	json_string.remove(json_string.length() - 1);
	json_string += "}";

	File configFile = SPIFFS.open("/config.json", "w");
	if (configFile) {
		configFile.print(json_string);
		debug_out(F("Config written: "), DEBUG_MIN_INFO);
		debug_outln(json_string, DEBUG_MAX_INFO);
		configFile.close();
	} else {
		debug_outln(F("failed to open config file for writing"), DEBUG_ERROR);
	}
}

/*****************************************************************
 * Base64 encode user:password                                   *
 *****************************************************************/
void create_basic_auth_strings() {
	basic_auth_custom = "";
	if (cfg::user_custom[0] != '\0' || cfg::pwd_custom[0] != '\0') {
		basic_auth_custom = base64::encode(String(cfg::user_custom) + ":" + String(cfg::pwd_custom));
	}
	basic_auth_influx = "";
	if (cfg::user_influx[0] != '\0' || cfg::pwd_influx[0] != '\0') {
		basic_auth_influx = base64::encode(String(cfg::user_influx) + ":" + String(cfg::pwd_influx));
	}
}

/*****************************************************************
 * aircms.online helper functions                                *
 *****************************************************************/
String sha1Hex(const String& s) {
#if defined(ESP8266)
	return sha1(s);
#endif
#if defined(ESP32)
	char sha1sum_output[21];
	esp_sha(SHA1, (const unsigned char*) s.c_str(), s.length(), (unsigned char*)sha1sum_output);
	return String(sha1sum_output);
#endif
}

String hmac1(const String& secret, const String& s) {
	debug_outln(F("Hashing string: "), DEBUG_MIN_INFO);
	debug_outln(s, DEBUG_MIN_INFO);
	String str = sha1Hex(s);
	debug_outln(secret + str, DEBUG_MIN_INFO);
	str = secret + str;
	return sha1Hex(str);
}

/*****************************************************************
 * html helper functions                                         *
 *****************************************************************/

String make_header(const String& title) {
	String s = FPSTR(WEB_PAGE_HEADER);
	s.replace("{tt}", FPSTR(INTL_PM_SENSOR));
	s.replace("{h}", FPSTR(INTL_HOME));
	if(title != " ") {
		s.replace("{n}", F("&raquo;"));
	} else {
		s.replace("{n}", empty_String);
	}
	s.replace("{t}", title);
	s.replace("{id}", esp_chipid);
	s.replace("{mac}", mac_addr_sta);
	s.replace("{fwt}", FPSTR(INTL_FIRMWARE));
	s.replace("{fw}", SOFTWARE_VERSION);
	return s;
}

String make_footer() {
	String s = FPSTR(WEB_PAGE_FOOTER);
	s.replace("{t}", FPSTR(INTL_BACK_TO_HOME));
	return s;
}

String form_input(const char* name, const String& info, const String& value, const int length) {
	String s = F(	"<tr>"
					"<td>{i} </td>"
					"<td style='width:90%;'>"
					"<input type='text' name='{n}' id='{n}' placeholder='{i}' value='{v}' maxlength='{l}'/>"
					"</td>"
					"</tr>");
	String t_value = value;
	t_value.replace("'", "&#39;");
	s.replace("{i}", info);
	s.replace("{n}", String(name));
	s.replace("{v}", t_value);
	s.replace("{l}", String(length));
	return s;
}

String form_password(const char* name, const String& info, const String& value, const int length) {
	String s = F(	"<tr>"
					"<td>{i} </td>"
					"<td style='width:90%;'>"
					"<input type='password' name='{n}' id='{n}' placeholder='{i}' value='{v}' maxlength='{l}'/>"
					"</td>"
					"</tr>");
	String password;
	for (uint8_t i = 0; i < value.length(); i++) {
		password += "*";
	}
	s.replace("{i}", info);
	s.replace("{n}", name);
	s.replace("{v}", password);
	s.replace("{l}", String(length));
	return s;
}

String form_checkbox(const char* name, const String& info, const bool checked, const bool linebreak) {
	String s = F("<label for='{n}'><input type='checkbox' name='{n}' value='1' id='{n}' {c}/> {i}</label><br/>");
	if (checked) {
		s.replace("{c}", F(" checked='checked'"));
	} else {
		s.replace("{c}", empty_String);
	};
	s.replace("{i}", info);
	s.replace("{n}", name);
	if (! linebreak) {
		s.replace("<br/>", empty_String);
	}
	return s;
}

String form_checkbox(const char* name, const String& info, const bool checked) {
	return form_checkbox(name, info, checked, true);
}

String form_checkbox_sensor(const char* name, const String& info, const bool checked) {
	return form_checkbox(name, add_sensor_type(info), checked);
}

String form_submit(const String& value) {
	String s = F(	"<tr>"
					"<td>&nbsp;</td>"
					"<td>"
					"<input type='submit' name='submit' value='{v}' />"
					"</td>"
					"</tr>");
	s.replace("{v}", value);
	return s;
}

String form_select_lang() {
	String s_select = F(" selected='selected'");
	String s = F(	"<tr>"
					"<td>{t}</td>"
					"<td>"
					"<select name='current_lang'>"
					"<option value='DE'>Deutsch (DE)</option>"
					"<option value='BG'>Bulgarian (BG)</option>"
					"<option value='CZ'>Český (CZ)</option>"
					"<option value='EN'>English (EN)</option>"
					"<option value='ES'>Español (ES)</option>"
					"<option value='FR'>Français (FR)</option>"
					"<option value='IT'>Italiano (IT)</option>"
					"<option value='LU'>Lëtzebuergesch (LU)</option>"
					"<option value='NL'>Nederlands (NL)</option>"
					"<option value='PL'>Polski (PL)</option>"
					"<option value='PT'>Português (PT)</option>"
					"<option value='RU'>Русский (RU)</option>"
					"<option value='SE'>Svenska (SE)</option>"
					"</select>"
					"</td>"
					"</tr>");

	s.replace("{t}", FPSTR(INTL_LANGUAGE));
	s.replace("'" + String(cfg::current_lang) + "'>", "'" + String(cfg::current_lang) + "'" + s_select + ">");
	return s;
}

static String tmpl(const String& patt, const String& value) {
	String s = patt;
	s.replace("{v}", value);
	return s;
}

static String tmpl(const String& patt, const String& value1, const String& value2, const String& value3) {
	String s = patt;
	s.replace("{v1}", value1);
	s.replace("{v2}", value2);
	s.replace("{v3}", value3);
	return s;
}

String line_from_value(const String& name, const String& value) {
	String s = F("<br/>{n}: {v}");
	s.replace("{n}", String(name));
	s.replace("{v}", value);
	return s;
}

String line_from_value_bool(const String& name, const bool value) {
	return line_from_value(name, String(value));
}

String table_row_from_value(const String& sensor, const String& param, const String& value, const String& unit) {
	String s = F(	"<tr>"
					"<td>{s}</td>"
					"<td>{p}</td>"
					"<td class='r'>{v}&nbsp;{u}</td>"
					"</tr>");
	s.replace("{s}", sensor);
	s.replace("{p}", param);
	s.replace("{v}", value);
	s.replace("{u}", unit);
	return s;
}

static int32_t calcWiFiSignalQuality(int32_t rssi) {
	if (rssi > -50) {
		rssi = -50;
	}
	if (rssi < -100) {
		rssi = -100;
	}
	return (rssi + 100) * 2;
}

String wlan_ssid_to_table_row(const String& ssid, const String& encryption, int32_t rssi) {
	String s = F(	"<tr>"
					"<td>"
					"<a href='#wlanpwd' onclick='setSSID(this)' class='wifi'>{n}</a>&nbsp;{e}"
					"</td>"
					"<td style='width:80%;vertical-align:middle;'>"
					"{v}%"
					"</td>"
					"</tr>");
	s.replace("{n}", ssid);
	s.replace("{e}", encryption);
	s.replace("{v}", String(calcWiFiSignalQuality(rssi)));
	return s;
}

String warning_first_cycle() {
	String s = FPSTR(INTL_TIME_TO_FIRST_MEASUREMENT);
	unsigned long time_to_first = cfg::sending_intervall_ms - msSince(starttime);
	if (time_to_first > cfg::sending_intervall_ms) {
		time_to_first = 0;
	}
	s.replace("{v}", String((long)((time_to_first + 500) / 1000)));
	return s;
}

String age_last_values() {
	String s = "<b>";
	unsigned long time_since_last = msSince(starttime);
	if (time_since_last > cfg::sending_intervall_ms) {
		time_since_last = 0;
	}
	s += String((long)((time_since_last + 500) / 1000));
	s += FPSTR(INTL_TIME_SINCE_LAST_MEASUREMENT);
	s += FPSTR(WEB_B_BR_BR);
	return s;
}

String add_sensor_type(const String& sensor_text) {
	String s = sensor_text;
	s.replace("{pm}", FPSTR(INTL_PARTICULATE_MATTER));
	s.replace("{t}", FPSTR(INTL_TEMPERATURE));
	s.replace("{h}", FPSTR(INTL_HUMIDITY));
	s.replace("{p}", FPSTR(INTL_PRESSURE));
	s.replace("{l_a}", FPSTR(INTL_LEQ_A));
	return s;
}

/*****************************************************************
 * Webserver request auth: prompt for BasicAuth
 *
 * -Provide BasicAuth for all page contexts except /values and images
 *****************************************************************/
static bool webserver_request_auth() {
	debug_outln(F("validate request auth..."), DEBUG_MIN_INFO);
	if (cfg::www_basicauth_enabled && ! wificonfig_loop) {
		if (!server.authenticate(cfg::www_username, cfg::www_password)) {
			server.requestAuthentication(BASIC_AUTH, "Sensor Login", "Authentication failed");
			return false;
		}
	}
	return true;
}

static void sendHttpRedirect() {
	server.sendHeader(F("Location"), F("http://192.168.4.1/config"));
	server.send(302, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), empty_String);
}

/*****************************************************************
 * Webserver root: show all options                              *
 *****************************************************************/
void webserver_root() {
	if (WiFi.status() != WL_CONNECTED) {
		sendHttpRedirect();
	} else {
		if (!webserver_request_auth())
		{ return; }

		String page_content = make_header(" ");
		last_page_load = millis();
		debug_outln(F("output root page..."), DEBUG_MIN_INFO);
		page_content += FPSTR(WEB_ROOT_PAGE_CONTENT);
		page_content.replace("{t}", FPSTR(INTL_CURRENT_DATA));
		page_content.replace(F("{map}"), FPSTR(INTL_ACTIVE_SENSORS_MAP));
		page_content.replace(F("{conf}"), FPSTR(INTL_CONFIGURATION));
		page_content.replace(F("{conf_delete}"), FPSTR(INTL_CONFIGURATION_DELETE));
		page_content.replace(F("{restart}"), FPSTR(INTL_RESTART_SENSOR));
		page_content += make_footer();
		server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
	}
}

/*****************************************************************
 * Webserver config: show config page                            *
 *****************************************************************/

static void webserver_config_body_get(String& page_content) {
	using namespace cfg;


	debug_outln(F("begin webserver_config_body_get ..."), DEBUG_MIN_INFO);
	page_content += F("<form method='POST' action='/config' style='width:100%;'>\n<b>");
	page_content += FPSTR(INTL_WIFI_SETTINGS);
	page_content += FPSTR(WEB_B_BR);
	debug_outln(F("output config page 1"), DEBUG_MIN_INFO);
	if (wificonfig_loop) {  // scan for wlan ssids
		page_content += F("<div id='wifilist'>");
		page_content += FPSTR(INTL_WIFI_NETWORKS);
		page_content += F("</div><br/>");
	}
	page_content += FPSTR(TABLE_TAG_OPEN);
	page_content += form_input("wlanssid", FPSTR(INTL_FS_WIFI_NAME), wlanssid, LEN_WLANSSID);
	page_content += form_password("wlanpwd", FPSTR(INTL_PASSWORD), wlanpwd, LEN_WLANSSID);
	page_content += FPSTR(TABLE_TAG_CLOSE_BR);
	page_content += F("<hr/>\n<br/><b>");

	page_content += FPSTR(INTL_AB_HIER_NUR_ANDERN);
	page_content += F("</b><br/><br/>\n<b>");

	if (! wificonfig_loop) {
		page_content += FPSTR(INTL_BASICAUTH);
		page_content += FPSTR(WEB_B_BR);
		page_content += form_checkbox("www_basicauth_enabled", FPSTR(INTL_BASICAUTH), www_basicauth_enabled);
		page_content += FPSTR(TABLE_TAG_OPEN);
		page_content += form_input("www_username", FPSTR(INTL_USER), www_username, LEN_WWW_USERNAME);
		page_content += form_password("www_password", FPSTR(INTL_PASSWORD), www_password, LEN_WWW_PASSWORD);
		page_content += FPSTR(TABLE_TAG_CLOSE_BR);

		page_content += FPSTR(WEB_BR_LF_B);

		page_content += FPSTR(INTL_FS_WIFI);
		page_content += FPSTR(WEB_B_BR);
		page_content += FPSTR(INTL_FS_WIFI_DESCRIPTION);
		page_content += FPSTR(BR_TAG);
		page_content += FPSTR(TABLE_TAG_OPEN);
		page_content += form_input("fs_ssid", FPSTR(INTL_FS_WIFI_NAME), fs_ssid, LEN_FS_SSID);
		page_content += form_password("fs_pwd", FPSTR(INTL_PASSWORD), fs_pwd, LEN_FS_PWD);
		page_content += FPSTR(TABLE_TAG_CLOSE_BR);

		page_content += FPSTR(WEB_BR_LF_B);

		page_content += F("APIs");
		page_content += FPSTR(WEB_B_BR);
		page_content += form_checkbox("send2dusti", F("API Luftdaten.info"), send2dusti, false);
		page_content += FPSTR(WEB_NBSP_NBSP_BRACE);
		page_content += form_checkbox("ssl_dusti", FPSTR(WEB_HTTPS), ssl_dusti, false);
		page_content += FPSTR(WEB_BRACE_BR);
		page_content += form_checkbox("send2madavi", F("API Madavi.de"), send2madavi, false);
		page_content += FPSTR(WEB_NBSP_NBSP_BRACE);
		page_content += form_checkbox("ssl_madavi", FPSTR(WEB_HTTPS), ssl_madavi, false);
		page_content += FPSTR(WEB_BRACE_BR);

		page_content += FPSTR(WEB_BR_LF_B);

		page_content += FPSTR(INTL_SENSORS);
		page_content += FPSTR(WEB_B_BR);
		page_content += form_checkbox_sensor("sds_read", FPSTR(INTL_SDS011), sds_read);
		page_content += form_checkbox_sensor("pms_read", FPSTR(INTL_PMS), pms_read);
		page_content += form_checkbox_sensor("hpm_read", FPSTR(INTL_HPM), hpm_read);
		page_content += form_checkbox_sensor("sps30_read", FPSTR(INTL_SPS30), sps30_read);
		page_content += form_checkbox_sensor("ppd_read", FPSTR(INTL_PPD42NS), ppd_read);
		page_content += form_checkbox_sensor("dht_read", FPSTR(INTL_DHT22), dht_read);
		page_content += form_checkbox_sensor("htu21d_read", FPSTR(INTL_HTU21D), htu21d_read);
		page_content += form_checkbox_sensor("bmp_read", FPSTR(INTL_BMP180), bmp_read);
		page_content += form_checkbox_sensor("bmp280_read", FPSTR(INTL_BMP280), bmp280_read);
		page_content += form_checkbox_sensor("bme280_read", FPSTR(INTL_BME280), bme280_read);
		page_content += form_checkbox_sensor("ds18b20_read", FPSTR(INTL_DS18B20), ds18b20_read);
		page_content += form_checkbox_sensor("dnms_read", FPSTR(INTL_DNMS), dnms_read);
		page_content += FPSTR(TABLE_TAG_OPEN);
		page_content += form_input("dnms_correction", FPSTR(INTL_DNMS_CORRECTION), dnms_correction, LEN_DNMS_CORRECTION);
		page_content += FPSTR(TABLE_TAG_CLOSE_BR);
		page_content += form_checkbox("gps_read", FPSTR(INTL_NEO6M), gps_read);

		page_content += FPSTR(WEB_BR_LF_B);

	}

	page_content += FPSTR(INTL_MORE_SETTINGS);
	page_content += FPSTR(WEB_B_BR);
	page_content += form_checkbox("auto_update", FPSTR(INTL_AUTO_UPDATE), auto_update);
	page_content += form_checkbox("use_beta", FPSTR(INTL_USE_BETA), use_beta);
	page_content += form_checkbox("has_display", FPSTR(INTL_DISPLAY), has_display);
	page_content += form_checkbox("has_sh1106", FPSTR(INTL_SH1106), has_sh1106);
	page_content += form_checkbox("has_flipped_display", FPSTR(INTL_FLIP_DISPLAY), has_flipped_display);
	page_content += form_checkbox("has_lcd1602_27", FPSTR(INTL_LCD1602_27), has_lcd1602_27);
	page_content += form_checkbox("has_lcd1602", FPSTR(INTL_LCD1602_3F), has_lcd1602);
	page_content += form_checkbox("has_lcd2004_27", FPSTR(INTL_LCD2004_27), has_lcd2004_27);
	page_content += form_checkbox("display_wifi_info", FPSTR(INTL_DISPLAY_WIFI_INFO), display_wifi_info);
	page_content += form_checkbox("display_device_info", FPSTR(INTL_DISPLAY_DEVICE_INFO), display_device_info);

	if (! wificonfig_loop) {
		page_content += FPSTR(TABLE_TAG_OPEN);
		page_content += form_select_lang();
		page_content += form_input("debug", FPSTR(INTL_DEBUG_LEVEL), String(debug), 1);
		page_content += form_input("sending_intervall_ms", FPSTR(INTL_MEASUREMENT_INTERVAL), String(sending_intervall_ms / 1000), 5);
		page_content += form_input("time_for_wifi_config", FPSTR(INTL_DURATION_ROUTER_MODE), String(time_for_wifi_config / 1000), 5);
		page_content += FPSTR(TABLE_TAG_CLOSE_BR);
		page_content += FPSTR(WEB_BR_LF_B);

		page_content += FPSTR(INTL_MORE_APIS);
		page_content += FPSTR(WEB_B_BR);
		page_content += form_checkbox("send2csv", tmpl(FPSTR(INTL_SEND_TO), FPSTR(WEB_CSV)), send2csv);
		page_content += form_checkbox("send2fsapp", tmpl(FPSTR(INTL_SEND_TO), FPSTR(WEB_FEINSTAUB_APP)), send2fsapp);
		page_content += form_checkbox("send2aircms", tmpl(FPSTR(INTL_SEND_TO), F("aircms.online")), send2aircms);
		page_content += form_checkbox("send2sensemap", tmpl(FPSTR(INTL_SEND_TO), F("OpenSenseMap")), send2sensemap);
		page_content += FPSTR(TABLE_TAG_OPEN);
		page_content += form_input("senseboxid", "senseBox&nbsp;ID: ", senseboxid, LEN_SENSEBOXID);
		page_content += FPSTR(TABLE_TAG_CLOSE_BR);
		page_content += FPSTR(BR_TAG);
		page_content += form_checkbox("send2custom", FPSTR(INTL_SEND_TO_OWN_API), send2custom, false);
		page_content += FPSTR(WEB_NBSP_NBSP_BRACE);
		page_content += form_checkbox("ssl_custom", FPSTR(WEB_HTTPS), ssl_custom, false);
		page_content += FPSTR(WEB_BRACE_BR);
		page_content += FPSTR(TABLE_TAG_OPEN);
		page_content += form_input("host_custom", FPSTR(INTL_SERVER), host_custom, LEN_HOST_CUSTOM);
		page_content += form_input("url_custom", FPSTR(INTL_PATH), url_custom, LEN_URL_CUSTOM);
		page_content += form_input("port_custom", FPSTR(INTL_PORT), String(port_custom), MAX_PORT_DIGITS);
		page_content += form_input("user_custom", FPSTR(INTL_USER), user_custom, LEN_USER_CUSTOM);
		page_content += form_password("pwd_custom", FPSTR(INTL_PASSWORD), pwd_custom, LEN_PWD_CUSTOM);
		page_content += FPSTR(TABLE_TAG_CLOSE_BR);

		page_content += FPSTR(BR_TAG);

		page_content += form_checkbox("send2influx", tmpl(FPSTR(INTL_SEND_TO), F("InfluxDB")), send2influx, false);
		page_content += FPSTR(WEB_NBSP_NBSP_BRACE);
		page_content += form_checkbox("ssl_influx", FPSTR(WEB_HTTPS), ssl_influx, false);
		page_content += FPSTR(WEB_BRACE_BR);
		page_content += FPSTR(TABLE_TAG_OPEN);
		page_content += form_input("host_influx", FPSTR(INTL_SERVER), host_influx, LEN_HOST_INFLUX);
		page_content += form_input("url_influx", FPSTR(INTL_PATH), url_influx, LEN_URL_INFLUX);
		page_content += form_input("port_influx", FPSTR(INTL_PORT), String(port_influx), MAX_PORT_DIGITS);
		page_content += form_input("user_influx", FPSTR(INTL_USER), user_influx, LEN_USER_INFLUX);
		page_content += form_password("pwd_influx", FPSTR(INTL_PASSWORD), pwd_influx, LEN_PWD_INFLUX);
		page_content += form_input("measurement_name_influx", F("Measurement"), measurement_name_influx, LEN_MEASUREMENT_NAME_INFLUX);
		page_content += form_submit(FPSTR(INTL_SAVE_AND_RESTART));
		page_content += FPSTR(TABLE_TAG_CLOSE_BR);
		page_content += FPSTR(BR_TAG);
		page_content += FPSTR(WEB_BR_FORM);
	}
	if (wificonfig_loop) {  // scan for wlan ssids
		page_content += FPSTR(TABLE_TAG_OPEN);
		page_content += form_submit(FPSTR(INTL_SAVE_AND_RESTART));
		page_content += FPSTR(TABLE_TAG_CLOSE_BR);
		page_content += FPSTR(WEB_BR_FORM);
		page_content += F("<script>window.setTimeout(load_wifi_list,1000);</script>");
	}
}


static void webserver_config_body_post(String& page_content) {
	String masked_pwd;

	using namespace cfg;

#define readCharParam(param) { \
		const String s_param(#param); \
		if (server.hasArg(s_param)){ \
			server.arg(s_param).toCharArray(param, sizeof(param)); \
		} \
	}

#define readBoolParam(param) \
		param = server.arg(#param) == "1";

#define readIntParam(param) \
		{ \
			int val = server.arg(#param).toInt(); \
			if (val != 0){ \
				param = val; \
			} \
		}

#define readTimeParam(param) { \
		const String s_param(#param); \
		if (server.hasArg(s_param)){ \
			param = server.arg(s_param).toInt() * 1000; \
		} \
	}

#define readPasswdParam(param) { \
		const String s_param(#param); \
		if (server.hasArg(s_param)) { \
			const String server_arg(server.arg(s_param)); \
			masked_pwd = ""; \
			for (uint8_t i=0;i<server_arg.length();i++) \
				masked_pwd += "*"; \
			if (masked_pwd != server_arg || !server_arg.length()) {\
				server_arg.toCharArray(param, sizeof(param)); \
			} \
		} \
	}

	if (server.hasArg("wlanssid") && server.arg("wlanssid") != "") {
		readCharParam(wlanssid);
		readPasswdParam(wlanpwd);
	}
	if (! wificonfig_loop) {
		readCharParam(current_lang);
		readCharParam(www_username);
		readPasswdParam(www_password);
		readBoolParam(www_basicauth_enabled);
		readCharParam(fs_ssid);
		String s_fs_pwd("fs_pwd");
		if (server.hasArg(s_fs_pwd) && ((server.arg(s_fs_pwd).length() > 7) || (server.arg(s_fs_pwd).length() == 0))) {
			readPasswdParam(fs_pwd);
		}
		readBoolParam(send2dusti);
		readBoolParam(ssl_dusti);
		readBoolParam(send2madavi);
		readBoolParam(ssl_madavi);
		readBoolParam(dht_read);
		readBoolParam(htu21d_read);
		readBoolParam(sds_read);
		readBoolParam(pms_read);
		readBoolParam(hpm_read);
		readBoolParam(sps30_read);
		readBoolParam(ppd_read);
		readBoolParam(bmp_read);
		readBoolParam(bmp280_read);
		readBoolParam(bme280_read);
		readBoolParam(ds18b20_read);
		readBoolParam(dnms_read);
		readCharParam(dnms_correction);
		readBoolParam(gps_read);

		readIntParam(debug);
		readTimeParam(sending_intervall_ms);
		readTimeParam(time_for_wifi_config);

		readBoolParam(send2csv);

		readBoolParam(send2fsapp);

		readBoolParam(send2aircms);

		readBoolParam(send2sensemap);
		readCharParam(senseboxid);

		readBoolParam(send2custom);
		readCharParam(host_custom);
		readCharParam(url_custom);
		readIntParam(port_custom);
		readCharParam(user_custom);
		readPasswdParam(pwd_custom);

		readBoolParam(send2influx);
		readCharParam(host_influx);
		readCharParam(url_influx);
		readIntParam(port_influx);
		readCharParam(user_influx);
		readPasswdParam(pwd_influx);
		readCharParam(measurement_name_influx);
		readBoolParam(ssl_influx);

	}

	readBoolParam(auto_update);
	readBoolParam(use_beta);
	readBoolParam(has_display);
	readBoolParam(has_sh1106);
	readBoolParam(has_flipped_display);
	readBoolParam(has_lcd1602);
	readBoolParam(has_lcd1602_27);
	readBoolParam(has_lcd2004_27);
	readBoolParam(display_wifi_info);
	readBoolParam(display_device_info);

#undef readCharParam
#undef readBoolParam
#undef readIntParam
#undef readTimeParam
#undef readPasswdParam

	page_content += line_from_value_bool(tmpl(FPSTR(INTL_SEND_TO), F("Luftdaten.info")), send2dusti);
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_SEND_TO), F("Madavi")), send2madavi);
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_READ_FROM), F("DHT")), dht_read);
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_READ_FROM), F("HTU21D")), htu21d_read);
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_READ_FROM), F("SDS")), sds_read);
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_READ_FROM), F("PMS(1,3,5,6,7)003")), pms_read);
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_READ_FROM), F("HPM")), hpm_read);
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_READ_FROM), F("SPS30")), sps30_read);
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_READ_FROM), F("PPD")), ppd_read);
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_READ_FROM), FPSTR(SENSORS_BMP180)), bmp_read);
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_READ_FROM), FPSTR(SENSORS_BMP280)), bmp280_read);
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_READ_FROM), FPSTR(SENSORS_BME280)), bme280_read);
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_READ_FROM), F("DS18B20")), ds18b20_read);
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_READ_FROM), F("DNMS")), dnms_read);
	page_content += line_from_value_bool(FPSTR(INTL_DNMS_CORRECTION), String(dnms_correction));
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_READ_FROM), F("GPS")), gps_read);
	page_content += line_from_value_bool(FPSTR(INTL_AUTO_UPDATE), auto_update);
	page_content += line_from_value_bool(FPSTR(INTL_USE_BETA), use_beta);
	page_content += line_from_value_bool(FPSTR(INTL_DISPLAY), has_display);
	page_content += line_from_value_bool(FPSTR(INTL_SH1106), has_sh1106);
	page_content += line_from_value_bool(FPSTR(INTL_FLIP_DISPLAY), has_flipped_display);
	page_content += line_from_value_bool(FPSTR(INTL_LCD1602_27), has_lcd1602_27);
	page_content += line_from_value_bool(FPSTR(INTL_LCD1602_3F), has_lcd1602);
	page_content += line_from_value_bool(FPSTR(INTL_LCD2004_27), has_lcd2004_27);
	page_content += line_from_value_bool(FPSTR(INTL_DISPLAY_WIFI_INFO), display_wifi_info);
	page_content += line_from_value_bool(FPSTR(INTL_DISPLAY_DEVICE_INFO), display_device_info);
	page_content += line_from_value(FPSTR(INTL_DEBUG_LEVEL), String(debug));
	page_content += line_from_value(FPSTR(INTL_MEASUREMENT_INTERVAL), String(sending_intervall_ms));
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_SEND_TO), F("Feinstaub-App")), send2fsapp);
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_SEND_TO), F("aircms.online")), send2aircms);
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_SEND_TO), FPSTR(WEB_CSV)), send2csv);
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_SEND_TO), FPSTR(WEB_FEINSTAUB_APP)), send2fsapp);
	page_content += line_from_value_bool(tmpl(FPSTR(INTL_SEND_TO), F("opensensemap")), send2sensemap);
	page_content += F("<br/>senseBox-ID ");
	page_content += senseboxid;
	page_content += FPSTR(WEB_BR_BR);
	page_content += line_from_value_bool(FPSTR(INTL_SEND_TO_OWN_API), send2custom);
	page_content += line_from_value(FPSTR(INTL_SERVER), host_custom);
	page_content += line_from_value(FPSTR(INTL_PATH), url_custom);
	page_content += line_from_value(FPSTR(INTL_PORT), String(port_custom));
	page_content += line_from_value(FPSTR(INTL_USER), user_custom);
	page_content += line_from_value(FPSTR(INTL_PASSWORD), pwd_custom);
	page_content += F("<br/><br/>InfluxDB: ");
	page_content += String(send2influx);
	page_content += line_from_value(FPSTR(INTL_SERVER), host_influx);
	page_content += line_from_value(FPSTR(INTL_PATH), url_influx);
	page_content += line_from_value(FPSTR(INTL_PORT), String(port_influx));
	page_content += line_from_value(FPSTR(INTL_USER), user_influx);
	page_content += line_from_value(FPSTR(INTL_PASSWORD), pwd_influx);
	page_content += line_from_value(F("Measurement"), measurement_name_influx);
	page_content += line_from_value(F("SSL"), String(ssl_influx));
	page_content += FPSTR(WEB_BR_BR);
	page_content += FPSTR(INTL_SENSOR_IS_REBOOTING);
}

void webserver_config() {
	if (!webserver_request_auth())
	{ return; }

	String page_content = make_header(FPSTR(INTL_CONFIGURATION));
	// This is a large page...
	page_content.reserve(12 * 1024);

	last_page_load = millis();

	debug_outln(F("output config page ..."), DEBUG_MIN_INFO);
	if (wificonfig_loop) {  // scan for wlan ssids
		page_content += FPSTR(WEB_CONFIG_SCRIPT);
	}

	if (server.method() == HTTP_GET) {
		webserver_config_body_get(page_content);
	} else {
		webserver_config_body_post(page_content);
	}
	page_content += make_footer();

	server.sendHeader(F("Cache-Control"), F("no-cache, no-store, must-revalidate"));
	server.sendHeader(F("Pragma"), F("no-cache"));
	server.sendHeader(F("Expires"), F("0"));
	server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);

	if (server.method() == HTTP_POST) {
		display_debug(F("Writing config"), F("and restarting"));
		writeConfig();
		delay(500);
		ESP.restart();
	}
}

/*****************************************************************
 * Webserver wifi: show available wifi networks                  *
 *****************************************************************/
void webserver_wifi() {
	String page_content;

	debug_out(F("wifi networks found: "), DEBUG_MIN_INFO);
	debug_outln(String(count_wifiInfo), DEBUG_MIN_INFO);
	if (count_wifiInfo == 0) {
		page_content += FPSTR(BR_TAG);
		page_content += FPSTR(INTL_NO_NETWORKS);
		page_content += FPSTR(BR_TAG);
	} else {
		std::unique_ptr<int[]> indices(new int[count_wifiInfo]);
		debug_outln(F("output config page 2"), DEBUG_MIN_INFO);
		for (unsigned i = 0; i < count_wifiInfo; ++i) {
			indices[i] = i;
		}
		for (unsigned i = 0; i < count_wifiInfo; i++) {
			for (unsigned j = i + 1; j < count_wifiInfo; j++) {
				if (wifiInfo[indices[j]].RSSI > wifiInfo[indices[i]].RSSI) {
					std::swap(indices[i], indices[j]);
				}
			}
		}
		debug_outln(F("output config page 3"), DEBUG_MIN_INFO);
		int duplicateSsids = 0;
		for (int i = 0; i < count_wifiInfo; i++) {
			if (indices[i] == -1) {
				continue;
			}
			for (int j = i + 1; j < count_wifiInfo; j++) {
				if (strncmp(wifiInfo[indices[i]].ssid, wifiInfo[indices[j]].ssid, sizeof(wifiInfo[0].ssid)) == 0) {
					indices[j] = -1; // set dup aps to index -1
					++duplicateSsids;
				}
			}
		}

		page_content += FPSTR(INTL_NETWORKS_FOUND);
		page_content += String(count_wifiInfo - duplicateSsids);
		page_content += FPSTR(BR_TAG);
		page_content += FPSTR(BR_TAG);
		page_content += FPSTR(TABLE_TAG_OPEN);
		//if(n > 30) n=30;
		for (int i = 0; i < count_wifiInfo; ++i) {
			if (indices[i] == -1
 #if defined (ESP8266)
				|| wifiInfo[indices[i]].isHidden
 #endif
			) {
				continue;
			}
			// Print SSID and RSSI for each network found
#if defined(ESP8266)
			page_content += wlan_ssid_to_table_row(wifiInfo[indices[i]].ssid, ((wifiInfo[indices[i]].encryptionType == ENC_TYPE_NONE) ? " " : u8"🔒"), wifiInfo[indices[i]].RSSI);
#endif
#if defined(ESP32)
			page_content += wlan_ssid_to_table_row(wifiInfo[indices[i]].ssid, ((wifiInfo[indices[i]].encryptionType == WIFI_AUTH_OPEN) ? " " : u8"🔒"), wifiInfo[indices[i]].RSSI);
#endif
		}
		page_content += FPSTR(TABLE_TAG_CLOSE_BR);
		page_content += FPSTR(BR_TAG);
	}
	server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
}

/*****************************************************************
 * Webserver root: show latest values                            *
 *****************************************************************/
void webserver_values() {
	if (WiFi.status() != WL_CONNECTED) {
		sendHttpRedirect();
	} else {
		String page_content = make_header(FPSTR(INTL_CURRENT_DATA));
		const String unit_PM = "µg/m³";
		const String unit_T = "°C";
		const String unit_H = "%";
		const String unit_P = "hPa";
		const String unit_NC = "#/cm³";
		const String unit_TS = "µm";
		const String unit_LA = "dB(A)";
		last_page_load = millis();

		const int signal_quality = calcWiFiSignalQuality(WiFi.RSSI());
		debug_outln(F("output values to web page..."), DEBUG_MIN_INFO);
		if (first_cycle) {
			page_content += F("<b style='color:red'>");
			page_content += warning_first_cycle();
			page_content += FPSTR(WEB_B_BR_BR);
		} else {
			page_content += age_last_values();
		}
		page_content += F("<table cellspacing='0' border='1' cellpadding='5'>");
		page_content += tmpl(F("<tr><th>{v1}</th><th>{v2}</th><th>{v3}</th>"), FPSTR(INTL_SENSOR), FPSTR(INTL_PARAMETER), FPSTR(INTL_VALUE));
		if (cfg::ppd_read) {
			page_content += FPSTR(EMPTY_ROW);
			page_content += table_row_from_value(FPSTR(SENSORS_PPD42NS), FPSTR(WEB_PM1), check_display_value(last_value_PPD_P1, -1, 1, 0), FPSTR(INTL_PARTICLES_PER_LITER));
			page_content += table_row_from_value(FPSTR(SENSORS_PPD42NS), FPSTR(WEB_PM25), check_display_value(last_value_PPD_P2, -1, 1, 0), FPSTR(INTL_PARTICLES_PER_LITER));
		}
		if (cfg::sds_read) {
			page_content += FPSTR(EMPTY_ROW);
			page_content += table_row_from_value(FPSTR(SENSORS_SDS011), FPSTR(WEB_PM25), check_display_value(last_value_SDS_P2, -1, 1, 0), unit_PM);
			page_content += table_row_from_value(FPSTR(SENSORS_SDS011), FPSTR(WEB_PM10), check_display_value(last_value_SDS_P1, -1, 1, 0), unit_PM);
		}
		if (cfg::pms_read) {
			page_content += FPSTR(EMPTY_ROW);
			page_content += table_row_from_value(FPSTR(SENSORS_PMSx003), FPSTR(WEB_PM1), check_display_value(last_value_PMS_P0, -1, 1, 0), unit_PM);
			page_content += table_row_from_value(FPSTR(SENSORS_PMSx003), FPSTR(WEB_PM25), check_display_value(last_value_PMS_P2, -1, 1, 0), unit_PM);
			page_content += table_row_from_value(FPSTR(SENSORS_PMSx003), FPSTR(WEB_PM10), check_display_value(last_value_PMS_P1, -1, 1, 0), unit_PM);
		}
		if (cfg::hpm_read) {
			page_content += FPSTR(EMPTY_ROW);
			page_content += table_row_from_value(FPSTR(SENSORS_HPM), FPSTR(WEB_PM25), check_display_value(last_value_HPM_P2, -1, 1, 0), unit_PM);
			page_content += table_row_from_value(FPSTR(SENSORS_HPM), FPSTR(WEB_PM10), check_display_value(last_value_HPM_P1, -1, 1, 0), unit_PM);
		}
		if (cfg::sps30_read) {
			page_content += FPSTR(EMPTY_ROW);
			page_content += table_row_from_value(FPSTR(SENSORS_SPS30), FPSTR(WEB_PM1), check_display_value(last_value_SPS30_P0, -1, 1, 0), unit_PM);
			page_content += table_row_from_value(FPSTR(SENSORS_SPS30), FPSTR(WEB_PM25), check_display_value(last_value_SPS30_P1, -1, 1, 0), unit_PM);
			page_content += table_row_from_value(FPSTR(SENSORS_SPS30), FPSTR(WEB_PM4), check_display_value(last_value_SPS30_P2, -1, 1, 0), unit_PM);
			page_content += table_row_from_value(FPSTR(SENSORS_SPS30), FPSTR(WEB_PM10), check_display_value(last_value_SPS30_P3, -1, 1, 0), unit_PM);
			page_content += table_row_from_value(FPSTR(SENSORS_SPS30), FPSTR(WEB_NC0k5), check_display_value(last_value_SPS30_N0, -1, 0, 0), unit_NC);
			page_content += table_row_from_value(FPSTR(SENSORS_SPS30), FPSTR(WEB_NC1k0), check_display_value(last_value_SPS30_N1, -1, 0, 0), unit_NC);
			page_content += table_row_from_value(FPSTR(SENSORS_SPS30), FPSTR(WEB_NC2k5), check_display_value(last_value_SPS30_N2, -1, 0, 0), unit_NC);
			page_content += table_row_from_value(FPSTR(SENSORS_SPS30), FPSTR(WEB_NC4k0), check_display_value(last_value_SPS30_N3, -1, 0, 0), unit_NC);
			page_content += table_row_from_value(FPSTR(SENSORS_SPS30), FPSTR(WEB_NC10), check_display_value(last_value_SPS30_N4, -1, 0, 0), unit_NC);
			page_content += table_row_from_value(FPSTR(SENSORS_SPS30), FPSTR(WEB_TPS), check_display_value(last_value_SPS30_TS, -1, 1, 0), unit_TS);
		}
		if (cfg::dht_read) {
			page_content += FPSTR(EMPTY_ROW);
			page_content += table_row_from_value(FPSTR(SENSORS_DHT22), FPSTR(INTL_TEMPERATURE), check_display_value(last_value_DHT_T, -128, 1, 0), unit_T);
			page_content += table_row_from_value(FPSTR(SENSORS_DHT22), FPSTR(INTL_HUMIDITY), check_display_value(last_value_DHT_H, -1, 1, 0), unit_H);
		}
		if (cfg::htu21d_read) {
			page_content += FPSTR(EMPTY_ROW);
			page_content += table_row_from_value(FPSTR(SENSORS_HTU21D), FPSTR(INTL_TEMPERATURE), check_display_value(last_value_HTU21D_T, -128, 1, 0), unit_T);
			page_content += table_row_from_value(FPSTR(SENSORS_HTU21D), FPSTR(INTL_HUMIDITY), check_display_value(last_value_HTU21D_H, -1, 1, 0), unit_H);
		}
		if (cfg::bmp_read) {
			page_content += FPSTR(EMPTY_ROW);
			page_content += table_row_from_value(FPSTR(SENSORS_BMP180), FPSTR(INTL_TEMPERATURE), check_display_value(last_value_BMP_T, -128, 1, 0), unit_T);
			page_content += table_row_from_value(FPSTR(SENSORS_BMP180), FPSTR(INTL_PRESSURE), check_display_value(last_value_BMP_P / 100.0, (-1 / 100.0), 2, 0), unit_P);
		}
		if (cfg::bmp280_read) {
			page_content += FPSTR(EMPTY_ROW);
			page_content += table_row_from_value(FPSTR(SENSORS_BMP280), FPSTR(INTL_TEMPERATURE), check_display_value(last_value_BMP280_T, -128, 1, 0), unit_T);
			page_content += table_row_from_value(FPSTR(SENSORS_BMP280), FPSTR(INTL_PRESSURE), check_display_value(last_value_BMP280_P / 100.0, (-1 / 100.0), 2, 0), unit_P);
		}
		if (cfg::bme280_read) {
			page_content += FPSTR(EMPTY_ROW);
			page_content += table_row_from_value(FPSTR(SENSORS_BME280), FPSTR(INTL_TEMPERATURE), check_display_value(last_value_BME280_T, -128, 1, 0), unit_T);
			page_content += table_row_from_value(FPSTR(SENSORS_BME280), FPSTR(INTL_HUMIDITY), check_display_value(last_value_BME280_H, -1, 1, 0), unit_H);
			page_content += table_row_from_value(FPSTR(SENSORS_BME280), FPSTR(INTL_PRESSURE), check_display_value(last_value_BME280_P / 100.0, (-1 / 100.0), 2, 0), unit_P);
		}
		if (cfg::ds18b20_read) {
			page_content += FPSTR(EMPTY_ROW);
			page_content += table_row_from_value(FPSTR(SENSORS_DS18B20), FPSTR(INTL_TEMPERATURE), check_display_value(last_value_DS18B20_T, -128, 1, 0), unit_T);
		}
		if (cfg::dnms_read) {
			page_content += FPSTR(EMPTY_ROW);
			page_content += table_row_from_value(FPSTR(SENSORS_DNMS), FPSTR(INTL_LEQ_A), check_display_value(last_value_dnms_laeq, -1, 1, 0), unit_LA);
			page_content += table_row_from_value(FPSTR(SENSORS_DNMS), FPSTR(INTL_LA_MIN), check_display_value(last_value_dnms_la_min, -1, 1, 0), unit_LA);
			page_content += table_row_from_value(FPSTR(SENSORS_DNMS), FPSTR(INTL_LA_MAX), check_display_value(last_value_dnms_la_max, -1, 1, 0), unit_LA);
		}
		if (cfg::gps_read) {
			page_content += FPSTR(EMPTY_ROW);
			page_content += table_row_from_value(FPSTR(WEB_GPS), FPSTR(INTL_LATITUDE), check_display_value(last_value_GPS_lat, -200.0, 6, 0), "°");
			page_content += table_row_from_value(FPSTR(WEB_GPS), FPSTR(INTL_LONGITUDE), check_display_value(last_value_GPS_lon, -200.0, 6, 0), "°");
			page_content += table_row_from_value(FPSTR(WEB_GPS), FPSTR(INTL_ALTITUDE), check_display_value(last_value_GPS_alt, -1000.0, 2, 0), "m");
			page_content += table_row_from_value(FPSTR(WEB_GPS), FPSTR(INTL_DATE), last_value_GPS_date, empty_String);
			page_content += table_row_from_value(FPSTR(WEB_GPS), FPSTR(INTL_TIME), last_value_GPS_time, empty_String);
		}

		page_content += FPSTR(EMPTY_ROW);
		page_content += table_row_from_value("WiFi", FPSTR(INTL_SIGNAL_STRENGTH), String(WiFi.RSSI()), "dBm");
		page_content += table_row_from_value("WiFi", FPSTR(INTL_SIGNAL_QUALITY), String(signal_quality), "%");

		page_content += FPSTR(EMPTY_ROW);
		page_content += F("<tr><td colspan='2'>");
		page_content += FPSTR(INTL_NUMBER_OF_MEASUREMENTS);
		page_content += F("</td><td class='r'>");
		page_content += String(count_sends);
		page_content += F("</td></tr>");
		page_content += FPSTR(TABLE_TAG_CLOSE_BR);
		page_content += make_footer();
		server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
	}
}

/*****************************************************************
 * Webserver set debug level                                     *
 *****************************************************************/
void webserver_debug_level() {
	if (!webserver_request_auth())
	{ return; }

	String page_content = make_header(FPSTR(INTL_DEBUG_LEVEL));
	last_page_load = millis();
	debug_outln(F("output change debug level page..."), DEBUG_MIN_INFO);

	if (server.hasArg("lvl")) {
		const int lvl = server.arg("lvl").toInt();
		if (lvl >= 0 && lvl <= 5) {
			cfg::debug = lvl;
			page_content += F("<h3>");
			page_content += FPSTR(INTL_DEBUG_SETTING_TO);
			page_content += F(" ");

			static constexpr std::array<const char *, 6> lvlText PROGMEM = {
				INTL_NONE, INTL_ERROR, INTL_WARNING, INTL_MIN_INFO, INTL_MED_INFO, INTL_MAX_INFO
			};

			page_content += FPSTR(lvlText[lvl]);
			page_content += F(".</h3>");
		}
	}
	page_content += make_footer();
	server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
}

/*****************************************************************
 * Webserver remove config                                       *
 *****************************************************************/
void webserver_removeConfig() {
	if (!webserver_request_auth())
	{ return; }

	String page_content = make_header(FPSTR(INTL_DELETE_CONFIG));
	String message_string = F("<h3>{v}.</h3>");
	last_page_load = millis();
	debug_outln(F("output remove config page..."), DEBUG_MIN_INFO);

	if (server.method() == HTTP_GET) {
		page_content += FPSTR(WEB_REMOVE_CONFIG_CONTENT);
		page_content.replace("{t}", FPSTR(INTL_CONFIGURATION_REALLY_DELETE));
		page_content.replace("{b}", FPSTR(INTL_DELETE));
		page_content.replace("{c}", FPSTR(INTL_CANCEL));

	} else {
		if (SPIFFS.exists("/config.json")) {	//file exists
			debug_outln(F("removing config.json..."), DEBUG_MIN_INFO);
			if (SPIFFS.remove("/config.json")) {
				page_content += tmpl(message_string, FPSTR(INTL_CONFIG_DELETED));
			} else {
				page_content += tmpl(message_string, FPSTR(INTL_CONFIG_CAN_NOT_BE_DELETED));
			}
		} else {
			page_content += tmpl(message_string, FPSTR(INTL_CONFIG_NOT_FOUND));
		}
	}
	page_content += make_footer();
	server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
}

/*****************************************************************
 * Webserver reset NodeMCU                                       *
 *****************************************************************/
void webserver_reset() {
	if (!webserver_request_auth())
	{ return; }

	String page_content = make_header(FPSTR(INTL_RESTART_SENSOR));
	last_page_load = millis();
	debug_outln(F("output reset NodeMCU page..."), DEBUG_MIN_INFO);

	if (server.method() == HTTP_GET) {
		page_content += FPSTR(WEB_RESET_CONTENT);
		page_content.replace("{t}", FPSTR(INTL_REALLY_RESTART_SENSOR));
		page_content.replace("{b}", FPSTR(INTL_RESTART));
		page_content.replace("{c}", FPSTR(INTL_CANCEL));
	} else {
		ESP.restart();
	}
	page_content += make_footer();
	server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), page_content);
}

/*****************************************************************
 * Webserver data.json                                           *
 *****************************************************************/
void webserver_data_json() {
	String s1;
	unsigned long age = 0;

	debug_outln(F("output data json..."), DEBUG_MIN_INFO);
	if (first_cycle) {
		s1 = tmpl(FPSTR(data_first_part), SOFTWARE_VERSION);
		s1 += "]}";
		age = cfg::sending_intervall_ms - msSince(starttime);
		if (age > cfg::sending_intervall_ms) {
			age = 0;
		}
		age = 0 - age;
	} else {
		s1 = last_data_string;
		debug_out(F("last data: "), DEBUG_MIN_INFO);
		debug_outln(s1, DEBUG_MIN_INFO);
		age = msSince(starttime);
		if (age > cfg::sending_intervall_ms) {
			age = 0;
		}
	}
	String s2 = F(", \"age\":\"");
	s2 += String((long)((age + 500) / 1000));
	s2 += F("\", \"sensordatavalues\"");
	debug_out(F("replace with: "), DEBUG_MIN_INFO);
	debug_outln(s2, DEBUG_MIN_INFO);
	s1.replace(F(", \"sensordatavalues\""), s2);
	debug_out(F("replaced: "), DEBUG_MIN_INFO);
	debug_outln(s1, DEBUG_MIN_INFO);
	server.send(200, FPSTR(TXT_CONTENT_TYPE_JSON), s1);
}

/*****************************************************************
 * Webserver prometheus metrics endpoint                         *
 *****************************************************************/
void webserver_prometheus_endpoint() {
	debug_outln(F("output prometheus endpoint..."), DEBUG_MIN_INFO);
	String data_4_prometheus = F("software_version{version=\"{ver}\",node=\"-{id}\"} 1\nuptime_ms{{id}} {up}\nsending_intervall_ms{{id}} {si}\nnumber_of_measurements{{id}} {cs}\n");
	debug_outln(F("Parse JSON for Prometheus"), DEBUG_MIN_INFO);
	debug_outln(last_data_string, DEBUG_MED_INFO);
	String id = F("node=\"esp8266-");
	id += esp_chipid + "\"";
	data_4_prometheus.replace("{id}", esp_chipid);
	data_4_prometheus.replace("{ver}", SOFTWARE_VERSION);
	data_4_prometheus.replace("{up}", String(msSince(time_point_device_start_ms)));
	data_4_prometheus.replace("{si}", String(cfg::sending_intervall_ms));
	data_4_prometheus.replace("{cs}", String(count_sends));
	DynamicJsonDocument json2data(JSON_BUFFER_SIZE);
	DeserializationError err = deserializeJson(json2data, last_data_string);
	if (!err) {
		for (uint8_t i = 0; i < json2data["sensordatavalues"].size() - 1; i++) {
			String tmp_str = json2data["sensordatavalues"][i]["value_type"].as<char*>();
			data_4_prometheus += tmp_str + "{" + id + "} ";
			tmp_str = json2data["sensordatavalues"][i]["value"].as<char*>();
			data_4_prometheus += tmp_str + "\n";
		}
		data_4_prometheus += F("last_sample_age_ms{");
		data_4_prometheus += id + "} " + String(msSince(starttime)) + "\n";
	} else {
		debug_outln(FPSTR(DBG_TXT_DATA_READ_FAILED), DEBUG_ERROR);
	}
	debug_outln(data_4_prometheus, DEBUG_MED_INFO);
	server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN), data_4_prometheus);
}

/*****************************************************************
 * Webserver Images                                              *
 *****************************************************************/
static void webserver_images() {
	if (server.arg("name") == F("luftdaten_logo")) {
		debug_outln(F("output luftdaten.info logo..."), DEBUG_MIN_INFO);
		server.send(200, FPSTR(TXT_CONTENT_TYPE_IMAGE_SVG), FPSTR(LUFTDATEN_INFO_LOGO_SVG));
	} else {
		webserver_not_found();
	}
}

/*****************************************************************
 * Webserver page not found                                      *
 *****************************************************************/
void webserver_not_found() {
	last_page_load = millis();
	debug_outln(F("output not found page..."), DEBUG_MIN_INFO);
	if (WiFi.status() != WL_CONNECTED) {
		if ((server.uri().indexOf(F("success.html")) != -1) || (server.uri().indexOf(F("detect.html")) != -1)) {
			server.send(200, FPSTR(TXT_CONTENT_TYPE_TEXT_HTML), FPSTR(WEB_IOS_REDIRECT));
		} else {
			sendHttpRedirect();
		}
	} else {
		server.send(404, FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN), F("Not found."));
	}
}

/*****************************************************************
 * Webserver setup                                               *
 *****************************************************************/
void setup_webserver() {
	server.on("/", webserver_root);
	server.on("/config", webserver_config);
	server.on("/wifi", webserver_wifi);
	server.on("/values", webserver_values);
	server.on("/generate_204", webserver_config);
	server.on("/fwlink", webserver_config);
	server.on("/debug", webserver_debug_level);
	server.on("/removeConfig", webserver_removeConfig);
	server.on("/reset", webserver_reset);
	server.on("/data.json", webserver_data_json);
	server.on("/metrics", webserver_prometheus_endpoint);
	server.on("/images", webserver_images);
	server.onNotFound(webserver_not_found);

	debug_out(F("Starting Webserver... "), DEBUG_MIN_INFO);
//	debug_outln(IPAddress2String(WiFi.localIP()), DEBUG_MIN_INFO);
	debug_outln(WiFi.localIP().toString(), DEBUG_MIN_INFO);
	server.begin();
}

static int selectChannelForAp() {
	std::array<int, 14> channels_rssi;
	std::fill(channels_rssi.begin(), channels_rssi.end(), -100);

	for (unsigned i = 0; i < count_wifiInfo; i++) {
		if (wifiInfo[i].RSSI > channels_rssi[wifiInfo[i].channel]) {
			channels_rssi[wifiInfo[i].channel] = wifiInfo[i].RSSI;
		}
	}

	if ((channels_rssi[1] < channels_rssi[6]) && (channels_rssi[1] < channels_rssi[11])) {
		return 1;
	} else if ((channels_rssi[6] < channels_rssi[1]) && (channels_rssi[6] < channels_rssi[11])) {
		return 6;
	} else {
		return 11;
	}
}

/*****************************************************************
 * WifiConfig                                                    *
 *****************************************************************/
void wifiConfig() {
	debug_outln(F("Starting WiFiManager"), DEBUG_MIN_INFO);
	debug_out(F("AP ID: "), DEBUG_MIN_INFO);
	debug_outln(cfg::fs_ssid, DEBUG_MIN_INFO);
	debug_out(F("Password: "), DEBUG_MIN_INFO);
	debug_outln(cfg::fs_pwd, DEBUG_MIN_INFO);

	wificonfig_loop = true;

	WiFi.disconnect(true);
	debug_outln(F("scan for wifi networks..."), DEBUG_MIN_INFO);
	count_wifiInfo = WiFi.scanNetworks(false /* scan async */, true /* show hidden networks */);
	{
		delete [] wifiInfo;
		wifiInfo = new struct_wifiInfo[count_wifiInfo];

		for (int i = 0; i < count_wifiInfo; i++) {
			String SSID;
			uint8_t* BSSID;

			memset(&wifiInfo[i], 0, sizeof(struct_wifiInfo));
#if defined(ESP8266)
			WiFi.getNetworkInfo(i, SSID, wifiInfo[i].encryptionType,
				wifiInfo[i].RSSI, BSSID, wifiInfo[i].channel,
				wifiInfo[i].isHidden);
#else
			WiFi.getNetworkInfo(i, SSID, wifiInfo[i].encryptionType,
				wifiInfo[i].RSSI, BSSID, wifiInfo[i].channel);
#endif
			SSID.toCharArray(wifiInfo[i].ssid, sizeof(wifiInfo[0].ssid));
		}

		WiFi.mode(WIFI_AP);
		const IPAddress apIP(192, 168, 4, 1);
		WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
		WiFi.softAP(cfg::fs_ssid, cfg::fs_pwd, selectChannelForAp());
		// In case we create a unique password at first start
		debug_outln(String(F("AP Password is: ")) + String(WLANPWD), DEBUG_MIN_INFO);

		DNSServer dnsServer;
		dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
		dnsServer.start(53, "*", apIP);							// 53 is port for DNS server

		setup_webserver();

		// 10 minutes timeout for wifi config
		last_page_load = millis();
		while (((millis() - last_page_load) < cfg::time_for_wifi_config)) {
			dnsServer.processNextRequest();
			server.handleClient();
#if defined(ESP8266)
			wdt_reset(); // nodemcu is alive
#endif
			yield();
		}

		// half second to answer last requests
		last_page_load = millis();
		while ((millis() - last_page_load) < 500) {
			dnsServer.processNextRequest();
			server.handleClient();
			yield();
		}

		WiFi.disconnect(true);
		WiFi.softAPdisconnect(true);
		WiFi.mode(WIFI_STA);

		dnsServer.stop();
	}

	delay(100);

	debug_out(FPSTR(DBG_TXT_CONNECTING_TO), DEBUG_MIN_INFO);
	debug_outln(cfg::wlanssid, DEBUG_MIN_INFO);

	WiFi.begin(cfg::wlanssid, cfg::wlanpwd);

	debug_outln(F("---- Result Webconfig ----"), DEBUG_MIN_INFO);
	debug_out(F("WLANSSID: "), DEBUG_MIN_INFO);
	debug_outln(cfg::wlanssid, DEBUG_MIN_INFO);
	debug_outln(F("----\nReading ..."), DEBUG_MIN_INFO);
	debug_out(F("PPD: "), DEBUG_MIN_INFO);
	debug_outln_bool(cfg::ppd_read, DEBUG_MIN_INFO);
	debug_out(F("SDS: "), DEBUG_MIN_INFO);
	debug_outln_bool(cfg::sds_read, DEBUG_MIN_INFO);
	debug_out(F("PMS: "), DEBUG_MIN_INFO);
	debug_outln_bool(cfg::pms_read, DEBUG_MIN_INFO);
	debug_out(F("HPM: "), DEBUG_MIN_INFO);
	debug_outln_bool(cfg::hpm_read, DEBUG_MIN_INFO);
	debug_out(F("SPS30: "), DEBUG_MIN_INFO);
	debug_outln_bool(cfg::sps30_read, DEBUG_MIN_INFO);
	debug_out(F("DHT: "), DEBUG_MIN_INFO);
	debug_outln_bool(cfg::dht_read, DEBUG_MIN_INFO);
	debug_out(F("DS18B20: "), DEBUG_MIN_INFO);
	debug_outln_bool(cfg::ds18b20_read, DEBUG_MIN_INFO);
	debug_out(F("HTU21D: "), DEBUG_MIN_INFO);
	debug_outln_bool(cfg::htu21d_read, DEBUG_MIN_INFO);
	debug_out(F("BMP: "), DEBUG_MIN_INFO);
	debug_outln_bool(cfg::bmp_read, DEBUG_MIN_INFO);
	debug_out(F("DNMS: "), DEBUG_MIN_INFO);
	debug_outln_bool(cfg::dnms_read, DEBUG_MIN_INFO);
	debug_outln(F("----\nSend to ..."), DEBUG_MIN_INFO);
	debug_out(F("Dusti: "), DEBUG_MIN_INFO);
	debug_outln_bool(cfg::send2dusti, DEBUG_MIN_INFO);
	debug_out(F("Madavi: "), DEBUG_MIN_INFO);
	debug_outln_bool(cfg::send2madavi, DEBUG_MIN_INFO);
	debug_out(F("CSV: "), DEBUG_MIN_INFO);
	debug_outln_bool(cfg::send2csv, DEBUG_MIN_INFO);
	debug_outln("----", DEBUG_MIN_INFO);
	debug_out(F("Autoupdate: "), DEBUG_MIN_INFO);
	debug_outln_bool(cfg::auto_update, DEBUG_MIN_INFO);
	debug_out(F("Display: "), DEBUG_MIN_INFO);
	debug_outln_bool(cfg::has_display, DEBUG_MIN_INFO);
	debug_out(F("LCD 1602: "), DEBUG_MIN_INFO);
	debug_outln_bool(cfg::has_lcd1602, DEBUG_MIN_INFO);
	debug_out(F("Debug: "), DEBUG_MIN_INFO);
	debug_outln(String(cfg::debug), DEBUG_MIN_INFO);
	wificonfig_loop = false;
}

static void waitForWifiToConnect(int maxRetries) {
	int retryCount = 0;
	while ((WiFi.status() != WL_CONNECTED) && (retryCount < maxRetries)) {
		delay(500);
		debug_out(".", DEBUG_MIN_INFO);
		++retryCount;
	}
}

/*****************************************************************
 * WiFi auto connecting script                                   *
 *****************************************************************/
void connectWifi() {
	debug_outln(String(WiFi.status()), DEBUG_MIN_INFO);
	WiFi.disconnect();
#if defined(ESP8266)
	WiFi.setOutputPower(20.5);
	WiFi.setPhyMode(WIFI_PHY_MODE_11N);
#endif
	WiFi.mode(WIFI_STA);
	WiFi.begin(cfg::wlanssid, cfg::wlanpwd); // Start WiFI

	mac_addr_sta = WiFi.macAddress();

	debug_out(FPSTR(DBG_TXT_CONNECTING_TO), DEBUG_MIN_INFO);
	debug_outln(cfg::wlanssid, DEBUG_MIN_INFO);

	waitForWifiToConnect(40);
	debug_outln("", DEBUG_MIN_INFO);
	if (WiFi.status() != WL_CONNECTED) {
		String fss = String(cfg::fs_ssid);
		display_debug(fss.substring(0, 16), fss.substring(16));
		wifiConfig();
		if (WiFi.status() != WL_CONNECTED) {
			waitForWifiToConnect(20);
			debug_outln("", DEBUG_MIN_INFO);
		}
	}
	debug_out(F("WiFi connected\nIP address: "), DEBUG_MIN_INFO);
	debug_outln(WiFi.localIP().toString(), DEBUG_MIN_INFO);
}

void setup_network() {
	if(networkInitialized)
		return;

	if (WiFi.status() != WL_CONNECTED) {
		connectWifi();
	}
	if(!got_ntp) {
		got_ntp = acquireNetworkTime();
		debug_out(F("\nNTP time "), DEBUG_MIN_INFO);
		debug_outln(String(got_ntp ? "" : "not ") + F("received"), DEBUG_MIN_INFO);
	}
	autoUpdate();

#if 0
	// TODO: MDNS.begin crashes 
	String server_name = F("airRohr-");
	server_name += esp_chipid;
	if (MDNS.begin(server_name.c_str())) {
		MDNS.addService("http", "tcp", 80);
	}
	debug_outln(FPSTR("after MDNS.addService"), DEBUG_MIN_INFO); // TODO: remove
#endif

	networkInitialized = true;
}

/*****************************************************************
 * send data to rest api                                         *
 *****************************************************************/
unsigned long sendData(const String& data, const int pin, const char* host, const int httpPort, const char* url, const bool use_ssl, const bool verify, const char* basic_auth_string, const String& contentType) {
//#include "ca-root.h"

	setup_network();
	unsigned long start_send = millis();

	debug_out(F("Start connecting to "), DEBUG_MIN_INFO);
	debug_out(host, DEBUG_MIN_INFO);
	debug_out(F(":"), DEBUG_MIN_INFO);
	debug_outln(String(httpPort), DEBUG_MIN_INFO);

	String request_head = F("POST ");
	request_head += String(url);
	request_head += F(" HTTP/1.1\r\nHost: ");
	request_head += String(host);
	request_head += F("\r\nContent-Type: ");
	request_head += contentType + "\r\n";
	if (strlen(basic_auth_string) != 0) {
		request_head += F("Authorization: Basic ");
		request_head += String(basic_auth_string) + "\r\n";
	}
	request_head += F("X-PIN: ");
	request_head += String(pin);
	request_head += F("\r\nX-Sensor: esp8266-");
	request_head += esp_chipid;
	request_head += F("\r\nContent-Length: ");
	request_head += String(data.length(), DEC);
	request_head += F("\r\nConnection: close\r\n\r\n");

	const auto doConnect = [ = ](WiFiClient * client) -> bool {
		client->setNoDelay(true);
		client->setTimeout(20000);

		if (!client->connect(host, httpPort)) {
			debug_outln(F("connection failed"), DEBUG_ERROR);
			return false;
		}
		return true;
	};

	const auto doRequest = [ = ](WiFiClient * client) {
		debug_out(F("Requesting URL: "), DEBUG_MIN_INFO);
		debug_outln(url, DEBUG_MIN_INFO);
		debug_outln(esp_chipid, DEBUG_MIN_INFO);
		debug_outln(data, DEBUG_MIN_INFO);

		// send request to the server
		client->print(request_head);

		client->println(data);

		// wait for response
		int retries = 20;
		while (client->connected() && !client->available()) {
			delay(100);
#if defined(ESP8266)
			wdt_reset(); // nodemcu is alive
#endif
			if (!--retries)
			{ break; }
		}

		// Read reply from server and print them
		while(client->available()) {
			char c = client->read();
			debug_out(String(c), DEBUG_MIN_INFO);
		}
		client->stop();
		debug_outln(F("\nclosing connection\n----\n\n"), DEBUG_MIN_INFO);
	};

	// Use WiFiClient class to create TCP connections
	if (use_ssl) {
		WiFiClientSecure client_s;
		if (doConnect(&client_s)) {
			doRequest(&client_s);
		}

		/*		WiFiClientSecure client_s;
				if (doConnect(&client_s)) {
					if (verify) {
						if (client_s.setCACert_P(dst_root_ca_x3_bin_crt, dst_root_ca_x3_bin_crt_len)) {
							if (client_s.verifyCertChain(host)) {
								debug_outln(F("Server cert verified"), DEBUG_MIN_INFO);
								doRequest(&client_s);
							} else {
								debug_outln(F("ERROR: cert verification failed!"), DEBUG_ERROR);
							}
						} else {
							debug_outln(F("Failed to load root CA cert!"), DEBUG_ERROR);
						}
					} else {
						doRequest(&client_s);
					}
				}
		*/
		/*		BearSSL::WiFiClientSecure client_s;
				if (verify) {
					BearSSLX509List cert(dst_root_ca_x3);
					client_s.setTrustAnchors(&cert);
				} else {
					client_s.setInsecure();
				}
				if (doConnect(&client_s)) {
					doRequest(&client_s);
				}
		*/
	} else {
		WiFiClient client;
		if (doConnect(&client)) {
			doRequest(&client);
		}
	}
	debug_out(F("End connecting to "), DEBUG_MIN_INFO);
	debug_outln(host, DEBUG_MIN_INFO);

#if defined(ESP8266)
	wdt_reset(); // nodemcu is alive
#endif
	yield();

	return millis() - start_send;
}

/*****************************************************************
 * send single sensor data to luftdaten.info api                 *
 *****************************************************************/
unsigned long sendLuftdaten(const String& data, const int pin, const char* host, const int httpPort, const char* url, const bool use_ssl, const bool verify, const char* replace_str) {
	String data_4_dusti = tmpl(FPSTR(data_first_part), SOFTWARE_VERSION);
	unsigned long sum_send_time = 0;

	data_4_dusti += data;
	data_4_dusti.remove(data_4_dusti.length() - 1);
	data_4_dusti.replace(replace_str, empty_String);
	data_4_dusti += "]}";
	if (data != "") {
		sum_send_time = sendData(data_4_dusti, pin, host, httpPort, url, use_ssl, verify, "", FPSTR(TXT_CONTENT_TYPE_JSON));
	} else {
		debug_outln(F("No data sent..."), DEBUG_MIN_INFO);
	}

	return sum_send_time;
}

/*****************************************************************
 * send data to LoRa gateway                                     *
 *****************************************************************/
// void send_lora(const String& data) {
// }

/*****************************************************************
 * send data to mqtt api                                         *
 *****************************************************************/
// rejected (see issue #33)

/*****************************************************************
 * send data to influxdb                                         *
 *****************************************************************/
String create_influxdb_string(const String& data) {
	String data_4_influxdb;

	debug_outln(F("Parse JSON for influx DB"), DEBUG_MIN_INFO);
	debug_outln(data, DEBUG_MIN_INFO);
	DynamicJsonDocument json2data(JSON_BUFFER_SIZE);
	DeserializationError err = deserializeJson(json2data, data);
	if (!err) {
		data_4_influxdb += cfg::measurement_name_influx;
		data_4_influxdb += F(",node=esp8266-");
		data_4_influxdb += esp_chipid + " ";
		for (uint8_t i = 0; i < json2data["sensordatavalues"].size(); i++) {
			String tmp_str = json2data["sensordatavalues"][i]["value_type"].as<char*>();
			data_4_influxdb += tmp_str + "=";
			tmp_str = json2data["sensordatavalues"][i]["value"].as<char*>();
			data_4_influxdb += tmp_str + ",";
		}
		if ((unsigned)(data_4_influxdb.lastIndexOf(',') + 1) == data_4_influxdb.length()) {
			data_4_influxdb.remove(data_4_influxdb.length() - 1);
		}

		data_4_influxdb += "\n";
	} else {
		debug_outln(FPSTR(DBG_TXT_DATA_READ_FAILED), DEBUG_ERROR);
	}
	return data_4_influxdb;
}

/*****************************************************************
 * send data as csv to serial out                                *
 *****************************************************************/
void send_csv(const String& data) {
	DynamicJsonDocument json2data(JSON_BUFFER_SIZE);
	DeserializationError err = deserializeJson(json2data, data);
	debug_outln(F("CSV Output"), DEBUG_MIN_INFO);
	debug_outln(data, DEBUG_MIN_INFO);
	if (!err) {
		String headline = F("Timestamp_ms;");
		String valueline = String(act_milli) + ";";
		for (uint8_t i = 0; i < json2data["sensordatavalues"].size(); i++) {
			String tmp_str = json2data["sensordatavalues"][i]["value_type"].as<char*>();
			headline += tmp_str + ";";
			tmp_str = json2data["sensordatavalues"][i]["value"].as<char*>();
			valueline += tmp_str + ";";
		}
		static bool first_csv_line = true;
		if (first_csv_line) {
			if (headline.length() > 0) {
				headline.remove(headline.length() - 1);
			}
			Serial.println(headline);
			first_csv_line = false;
		}
		if (valueline.length() > 0) {
			valueline.remove(valueline.length() - 1);
		}
		Serial.println(valueline);
	} else {
		debug_outln(FPSTR(DBG_TXT_DATA_READ_FAILED), DEBUG_ERROR);
	}
}

/*****************************************************************
 * read DHT22 sensor values                                      *
 *****************************************************************/
static String sensorDHT() {
	String s;

	debug_outln(String(FPSTR(DBG_TXT_START_READING)) + "DHT11/22", DEBUG_MED_INFO);

	// Check if valid number if non NaN (not a number) will be send.
	last_value_DHT_T = -128;
	last_value_DHT_H = -1;

	int count = 0;
	const int MAX_ATTEMPTS = 5;
	while ((count++ < MAX_ATTEMPTS) && (s == "")) {
		auto h = dht.readHumidity();
		auto t = dht.readTemperature();
		if (isnan(t) || isnan(h)) {
			delay(100);
			h = dht.readHumidity();
			t = dht.readTemperature(false);
		}
		if (isnan(t) || isnan(h)) {
			debug_outln(String(FPSTR(SENSORS_DHT22)) + FPSTR(DBG_TXT_COULDNT_BE_READ), DEBUG_ERROR);
		} else {
			debug_outln(tmpl(FPSTR(DBG_TXT_TEMPERATURE), String(t)), DEBUG_MIN_INFO);
			debug_outln(tmpl(FPSTR(DBG_TXT_HUMIDITY), String(h)), DEBUG_MIN_INFO);
			last_value_DHT_T = t;
			last_value_DHT_H = h;
			s += Value2Json(F("temperature"), last_value_DHT_T);
			s += Value2Json(F("humidity"), last_value_DHT_H);
		}
	}
	debug_outln("----", DEBUG_MIN_INFO);

	debug_outln(String(FPSTR(DBG_TXT_END_READING)) + "DHT11/22", DEBUG_MED_INFO);

	return s;
}

/*****************************************************************
 * read HTU21D sensor values                                     *
 *****************************************************************/
static String sensorHTU21D() {
	String s;

	debug_outln(String(FPSTR(DBG_TXT_START_READING)) + FPSTR(SENSORS_HTU21D), DEBUG_MED_INFO);

	const auto t = htu21d.readTemperature();
	const auto h = htu21d.readHumidity();
	if (isnan(t) || isnan(h)) {
		last_value_HTU21D_T = -128.0;
		last_value_HTU21D_H = -1.0;
		debug_outln(String(FPSTR(SENSORS_HTU21D)) + FPSTR(DBG_TXT_COULDNT_BE_READ), DEBUG_ERROR);
	} else {
		debug_outln(tmpl(FPSTR(DBG_TXT_TEMPERATURE), Float2String(t)), DEBUG_MIN_INFO);
		debug_outln(tmpl(FPSTR(DBG_TXT_HUMIDITY), Float2String(h)), DEBUG_MIN_INFO);
		last_value_HTU21D_T = t;
		last_value_HTU21D_H = h;
		s += Value2Json(F("HTU21D_temperature"), last_value_HTU21D_T);
		s += Value2Json(F("HTU21D_humidity"), last_value_HTU21D_H);
	}
	debug_outln("----", DEBUG_MIN_INFO);

	debug_outln(String(FPSTR(DBG_TXT_END_READING)) + FPSTR(SENSORS_HTU21D), DEBUG_MED_INFO);

	return s;
}

/*****************************************************************
 * read BMP180 sensor values                                     *
 *****************************************************************/
static String sensorBMP() {
	String s;

	debug_outln(String(FPSTR(DBG_TXT_START_READING)) + FPSTR(SENSORS_BMP180), DEBUG_MED_INFO);

	const auto p = bmp.readPressure();
	const auto t = bmp.readTemperature();
	if (isnan(p) || isnan(t)) {
		last_value_BMP_T = -128.0;
		last_value_BMP_P = -1.0;
		debug_outln(String(FPSTR(SENSORS_BMP180)) + FPSTR(DBG_TXT_COULDNT_BE_READ), DEBUG_ERROR);
	} else {
		debug_outln(tmpl(FPSTR(DBG_TXT_TEMPERATURE), String(t)), DEBUG_MIN_INFO);
		debug_outln(tmpl(FPSTR(DBG_TXT_PRESSURE), Float2String(p / 100.0)), DEBUG_MIN_INFO);
		last_value_BMP_T = t;
		last_value_BMP_P = p;
		s += Value2Json(F("BMP_pressure"), last_value_BMP_P);
		s += Value2Json(F("BMP_temperature"), last_value_BMP_T);
	}
	debug_outln("----", DEBUG_MIN_INFO);

	debug_outln(String(FPSTR(DBG_TXT_END_READING)) + FPSTR(SENSORS_BMP180), DEBUG_MED_INFO);

	return s;
}

/*****************************************************************
 * read BMP280 sensor values                                     *
 *****************************************************************/
static String sensorBMP280() {
	String s;

	debug_outln(String(FPSTR(DBG_TXT_START_READING)) + FPSTR(SENSORS_BMP280), DEBUG_MED_INFO);

	const auto p = bmp280.readPressure();
	const auto t = bmp280.readTemperature();
	if (isnan(p) || isnan(t)) {
		last_value_BMP280_T = -128.0;
		last_value_BMP280_P = -1.0;
		debug_outln(String(FPSTR(SENSORS_BMP280)) + FPSTR(DBG_TXT_COULDNT_BE_READ), DEBUG_ERROR);
	} else {
		debug_outln(tmpl(FPSTR(DBG_TXT_TEMPERATURE), String(t)), DEBUG_MIN_INFO);
		debug_outln(tmpl(FPSTR(DBG_TXT_PRESSURE), Float2String(p / 100.0)), DEBUG_MIN_INFO);
		last_value_BMP280_T = t;
		last_value_BMP280_P = p;
		s += Value2Json(F("BMP280_pressure"), last_value_BMP280_P);
		s += Value2Json(F("BMP280_temperature"), last_value_BMP280_T);
	}
	debug_outln("----", DEBUG_MIN_INFO);

	debug_outln(String(FPSTR(DBG_TXT_END_READING)) + FPSTR(SENSORS_BMP280), DEBUG_MED_INFO);

	return s;
}

/*****************************************************************
 * read BME280 sensor values                                     *
 *****************************************************************/
static String sensorBME280() {
	String s;

	debug_outln(String(FPSTR(DBG_TXT_START_READING)) + FPSTR(SENSORS_BME280), DEBUG_MED_INFO);

	bme280.takeForcedMeasurement();
	const auto t = bme280.readTemperature();
	const auto h = bme280.readHumidity();
	const auto p = bme280.readPressure();
	if (isnan(t) || isnan(h) || isnan(p)) {
		last_value_BME280_T = -128.0;
		last_value_BME280_H = -1.0;
		last_value_BME280_P = -1.0;
		debug_outln(String(FPSTR(SENSORS_BME280)) + FPSTR(DBG_TXT_COULDNT_BE_READ), DEBUG_ERROR);
	} else {
		debug_outln(tmpl(FPSTR(DBG_TXT_TEMPERATURE), Float2String(t)), DEBUG_MIN_INFO);
		debug_outln(tmpl(FPSTR(DBG_TXT_HUMIDITY), Float2String(h)), DEBUG_MIN_INFO);
		debug_outln(tmpl(FPSTR(DBG_TXT_PRESSURE), Float2String(p / 100.0)), DEBUG_MIN_INFO);
		last_value_BME280_T = t;
		last_value_BME280_H = h;
		last_value_BME280_P = p;
		s += Value2Json(F("BME280_temperature"), last_value_BME280_T);
		s += Value2Json(F("BME280_humidity"), last_value_BME280_H);
		s += Value2Json(F("BME280_pressure"), last_value_BME280_P);
	}
	debug_outln("----", DEBUG_MIN_INFO);

	debug_outln(String(FPSTR(DBG_TXT_END_READING)) + FPSTR(SENSORS_BME280), DEBUG_MED_INFO);

	return s;
}

/*****************************************************************
 * read DS18B20 sensor values                                    *
 *****************************************************************/
static String sensorDS18B20() {
	double t;
	debug_outln(String(FPSTR(DBG_TXT_START_READING)) + FPSTR(SENSORS_DS18B20), DEBUG_MED_INFO);

	//it's very unlikely (-127: impossible) to get these temperatures in reality. Most times this means that the sensor is currently faulty
	//try 5 times to read the sensor, otherwise fail
	const int MAX_ATTEMPTS = 5;
	int count = 0;
	do {
		ds18b20.requestTemperatures();
		//for now, we want to read only the first sensor
		t = ds18b20.getTempCByIndex(0);
		++count;
		debug_out(F("DS18B20 trying...."), DEBUG_MIN_INFO);
		debug_outln(String(count), DEBUG_MIN_INFO);
	} while (count < MAX_ATTEMPTS && (isnan(t) || t >= 85.0 || t <= (-127.0)));

	String s;
	if (count == MAX_ATTEMPTS) {
		last_value_DS18B20_T = -128.0;
		debug_outln(String(FPSTR(SENSORS_DS18B20)) + FPSTR(DBG_TXT_COULDNT_BE_READ), DEBUG_ERROR);
	} else {
		debug_outln(tmpl(FPSTR(DBG_TXT_TEMPERATURE), Float2String(t)), DEBUG_MIN_INFO);
		last_value_DS18B20_T = t;
		s += Value2Json(F("DS18B20_temperature"), last_value_DS18B20_T);
	}
	debug_outln("----", DEBUG_MIN_INFO);
	debug_outln(String(FPSTR(DBG_TXT_END_READING)) + FPSTR(SENSORS_DS18B20), DEBUG_MED_INFO);

	return s;
}

/*****************************************************************
 * read SDS011 sensor values                                     *
 *****************************************************************/
static String sensorSDS() {
	String s;
	char buffer;
	int value;
	int len = 0;
	int pm10_serial = 0;
	int pm25_serial = 0;
	int checksum_is = 0;
	int checksum_ok = 0;

	debug_outln(String(FPSTR(DBG_TXT_START_READING)) + FPSTR(SENSORS_SDS011), DEBUG_MED_INFO);
	if (msSince(starttime) < (cfg::sending_intervall_ms - (WARMUPTIME_SDS_MS + READINGTIME_SDS_MS))) {
		if (is_SDS_running) {
			SDS_cmd(PmSensorCmd::Stop);
		}
	} else {
		if (! is_SDS_running) {
			SDS_cmd(PmSensorCmd::Start);
		}
		SDS_cmd(PmSensorCmd::QueryData);
		delay((10 + 2) * (1+8+1) * 1000 / 9600); // 10 characters at 9600 bps (8N1) + 20% extra

		while (serialSDS.available() > 0) {
			buffer = serialSDS.read();
			debug_outln(String(len) + " - " + String(buffer, DEC) + " - " + String(buffer, HEX) + " - " + int(buffer) + " .", DEBUG_MAX_INFO);
//			"aa" = 170, "ab" = 171, "c0" = 192
			value = int(buffer);
			switch (len) {
			case (0):
				if (value != 170) {
					len = -1;
				};
				break;
			case (1):
				if (value != 192) {
					len = -1;
				};
				break;
			case (2):
				pm25_serial = value;
				checksum_is = value;
				break;
			case (3):
				pm25_serial += (value << 8);
				break;
			case (4):
				pm10_serial = value;
				break;
			case (5):
				pm10_serial += (value << 8);
				break;
			case (8):
				debug_out(FPSTR(DBG_TXT_CHECKSUM_IS), DEBUG_MED_INFO);
				debug_out(String(checksum_is % 256), DEBUG_MED_INFO);
				debug_out(FPSTR(DBG_TXT_CHECKSUM_SHOULD), DEBUG_MED_INFO);
				debug_outln(String(value), DEBUG_MED_INFO);
				if (value == (checksum_is % 256)) {
					checksum_ok = 1;
				} else {
					len = -1;
				};
				break;
			case (9):
				if (value != 171) {
					len = -1;
				};
				break;
			}
			if (len > 2) { checksum_is += value; }
			len++;
			if (len == 10 && checksum_ok == 1 && (msSince(starttime) > (cfg::sending_intervall_ms - READINGTIME_SDS_MS))) {
				if ((! isnan(pm10_serial)) && (! isnan(pm25_serial))) {
					sds_pm10_sum += pm10_serial;
					sds_pm25_sum += pm25_serial;
					if (sds_pm10_min > pm10_serial) {
						sds_pm10_min = pm10_serial;
					}
					if (sds_pm10_max < pm10_serial) {
						sds_pm10_max = pm10_serial;
					}
					if (sds_pm25_min > pm25_serial) {
						sds_pm25_min = pm25_serial;
					}
					if (sds_pm25_max < pm25_serial) {
						sds_pm25_max = pm25_serial;
					}
					debug_out(F("PM10 (sec.) : "), DEBUG_MED_INFO);
					debug_outln(Float2String(double(pm10_serial) / 10), DEBUG_MED_INFO);
					debug_out(F("PM2.5 (sec.): "), DEBUG_MED_INFO);
					debug_outln(Float2String(double(pm25_serial) / 10), DEBUG_MED_INFO);
					sds_val_count++;
				}
				len = 0;
				checksum_ok = 0;
				pm10_serial = 0.0;
				pm25_serial = 0.0;
				checksum_is = 0;
			}
			yield();
		}

	}
	if (send_now) {
		last_value_SDS_P1 = -1;
		last_value_SDS_P2 = -1;
		if (sds_val_count > 2) {
			sds_pm10_sum = sds_pm10_sum - sds_pm10_min - sds_pm10_max;
			sds_pm25_sum = sds_pm25_sum - sds_pm25_min - sds_pm25_max;
			sds_val_count = sds_val_count - 2;
		}
		if (sds_val_count > 0) {
			last_value_SDS_P1 = double(sds_pm10_sum) / (sds_val_count * 10.0);
			last_value_SDS_P2 = double(sds_pm25_sum) / (sds_val_count * 10.0);
			debug_outln("PM10:  " + Float2String(last_value_SDS_P1), DEBUG_MIN_INFO);
			debug_outln("PM2.5: " + Float2String(last_value_SDS_P2), DEBUG_MIN_INFO);
			debug_outln("----", DEBUG_MIN_INFO);
			s += Value2Json(F("SDS_P1"), last_value_SDS_P1);
			s += Value2Json(F("SDS_P2"), last_value_SDS_P2);
		}
		sds_pm10_sum = 0;
		sds_pm25_sum = 0;
		sds_val_count = 0;
		sds_pm10_max = 0;
		sds_pm10_min = 20000;
		sds_pm25_max = 0;
		sds_pm25_min = 20000;
		if ((cfg::sending_intervall_ms > (WARMUPTIME_SDS_MS + READINGTIME_SDS_MS))) {
			SDS_cmd(PmSensorCmd::Stop);
		}
	}

	debug_outln(String(FPSTR(DBG_TXT_END_READING)) + FPSTR(SENSORS_SDS011), DEBUG_MED_INFO);

	return s;
}

/*****************************************************************
 * read Plantronic PM sensor sensor values                       *
 *****************************************************************/
static String sensorPMS() {
	String s;
	char buffer;
	int value;
	int len = 0;
	int pm1_serial = 0;
	int pm10_serial = 0;
	int pm25_serial = 0;
	int checksum_is = 0;
	int checksum_should = 0;
	int checksum_ok = 0;
	int frame_len = 24;				// min. frame length

	debug_outln(String(FPSTR(DBG_TXT_START_READING)) + FPSTR(SENSORS_PMSx003), DEBUG_MED_INFO);
	if (msSince(starttime) < (cfg::sending_intervall_ms - (WARMUPTIME_SDS_MS + READINGTIME_SDS_MS))) {
		if (is_PMS_running) {
			PMS_cmd(PmSensorCmd::Stop);
		}
	} else {
		if (! is_PMS_running) {
			PMS_cmd(PmSensorCmd::Start);
		}

		while (serialSDS.available() > 0) {
			buffer = serialSDS.read();
			debug_outln(String(len) + " - " + String(buffer, DEC) + " - " + String(buffer, HEX) + " - " + int(buffer) + " .", DEBUG_MAX_INFO);
//			"aa" = 170, "ab" = 171, "c0" = 192
			value = int(buffer);
			switch (len) {
			case (0):
				if (value != 66) {
					len = -1;
				};
				break;
			case (1):
				if (value != 77) {
					len = -1;
				};
				break;
			case (2):
				checksum_is = value;
				break;
			case (3):
				frame_len = value + 4;
				break;
			case (10):
				pm1_serial += ( value << 8);
				break;
			case (11):
				pm1_serial += value;
				break;
			case (12):
				pm25_serial = ( value << 8);
				break;
			case (13):
				pm25_serial += value;
				break;
			case (14):
				pm10_serial = ( value << 8);
				break;
			case (15):
				pm10_serial += value;
				break;
			case (22):
				if (frame_len == 24) {
					checksum_should = ( value << 8 );
				};
				break;
			case (23):
				if (frame_len == 24) {
					checksum_should += value;
				};
				break;
			case (30):
				checksum_should = ( value << 8 );
				break;
			case (31):
				checksum_should += value;
				break;
			}
			if ((len > 2) && (len < (frame_len - 2))) { checksum_is += value; }
			len++;
			if (len == frame_len) {
				debug_out(FPSTR(DBG_TXT_CHECKSUM_IS), DEBUG_MED_INFO);
				debug_out(String(checksum_is + 143), DEBUG_MED_INFO);
				debug_out(FPSTR(DBG_TXT_CHECKSUM_SHOULD), DEBUG_MED_INFO);
				debug_outln(String(checksum_should), DEBUG_MED_INFO);
				if (checksum_should == (checksum_is + 143)) {
					checksum_ok = 1;
				} else {
					len = 0;
				};
				if (checksum_ok == 1 && (msSince(starttime) > (cfg::sending_intervall_ms - READINGTIME_SDS_MS))) {
					if ((! isnan(pm1_serial)) && (! isnan(pm10_serial)) && (! isnan(pm25_serial))) {
						pms_pm1_sum += pm1_serial;
						pms_pm10_sum += pm10_serial;
						pms_pm25_sum += pm25_serial;
						if (pms_pm1_min > pm1_serial) {
							pms_pm1_min = pm1_serial;
						}
						if (pms_pm1_max < pm1_serial) {
							pms_pm1_max = pm1_serial;
						}
						if (pms_pm25_min > pm25_serial) {
							pms_pm25_min = pm25_serial;
						}
						if (pms_pm25_max < pm25_serial) {
							pms_pm25_max = pm25_serial;
						}
						if (pms_pm10_min > pm10_serial) {
							pms_pm10_min = pm10_serial;
						}
						if (pms_pm10_max < pm10_serial) {
							pms_pm10_max = pm10_serial;
						}
						debug_out(F("PM1 (sec.): "), DEBUG_MED_INFO);
						debug_outln(Float2String(double(pm1_serial)), DEBUG_MED_INFO);
						debug_out(F("PM2.5 (sec.): "), DEBUG_MED_INFO);
						debug_outln(Float2String(double(pm25_serial)), DEBUG_MED_INFO);
						debug_out(F("PM10 (sec.) : "), DEBUG_MED_INFO);
						debug_outln(Float2String(double(pm10_serial)), DEBUG_MED_INFO);
						pms_val_count++;
					}
					len = 0;
					checksum_ok = 0;
					pm1_serial = 0.0;
					pm10_serial = 0.0;
					pm25_serial = 0.0;
					checksum_is = 0;
				}
			}
			yield();
		}

	}
	if (send_now) {
		last_value_PMS_P0 = -1;
		last_value_PMS_P1 = -1;
		last_value_PMS_P2 = -1;
		if (pms_val_count > 2) {
			pms_pm1_sum = pms_pm1_sum - pms_pm1_min - pms_pm1_max;
			pms_pm10_sum = pms_pm10_sum - pms_pm10_min - pms_pm10_max;
			pms_pm25_sum = pms_pm25_sum - pms_pm25_min - pms_pm25_max;
			pms_val_count = pms_val_count - 2;
		}
		if (pms_val_count > 0) {
			last_value_PMS_P0 = double(pms_pm1_sum) / (pms_val_count * 1.0);
			last_value_PMS_P1 = double(pms_pm10_sum) / (pms_val_count * 1.0);
			last_value_PMS_P2 = double(pms_pm25_sum) / (pms_val_count * 1.0);
			debug_outln("PM1:   " + Float2String(last_value_PMS_P0), DEBUG_MIN_INFO);
			debug_outln("PM2.5: " + Float2String(last_value_PMS_P2), DEBUG_MIN_INFO);
			debug_outln("PM10:  " + Float2String(last_value_PMS_P1), DEBUG_MIN_INFO);
			debug_outln("-------", DEBUG_MIN_INFO);
			s += Value2Json(F("PMS_P0"), last_value_PMS_P0);
			s += Value2Json(F("PMS_P1"), last_value_PMS_P1);
			s += Value2Json(F("PMS_P2"), last_value_PMS_P2);
		}
		pms_pm1_sum = 0;
		pms_pm10_sum = 0;
		pms_pm25_sum = 0;
		pms_val_count = 0;
		pms_pm1_max = 0;
		pms_pm1_min = 20000;
		pms_pm10_max = 0;
		pms_pm10_min = 20000;
		pms_pm25_max = 0;
		pms_pm25_min = 20000;
		if (cfg::sending_intervall_ms > (WARMUPTIME_SDS_MS + READINGTIME_SDS_MS)) {
			PMS_cmd(PmSensorCmd::Stop);
		}
	}

	debug_outln(String(FPSTR(DBG_TXT_END_READING)) + FPSTR(SENSORS_PMSx003), DEBUG_MED_INFO);

	return s;
}

/*****************************************************************
 * read Honeywell PM sensor sensor values                        *
 *****************************************************************/
static String sensorHPM() {
	String s;
	char buffer;
	int value;
	int len = 0;
	int pm10_serial = 0;
	int pm25_serial = 0;
	int checksum_is = 0;
	int checksum_should = 0;
	int checksum_ok = 0;

	debug_outln(String(FPSTR(DBG_TXT_START_READING)) + FPSTR(SENSORS_HPM), DEBUG_MED_INFO);
	if (msSince(starttime) < (cfg::sending_intervall_ms - (WARMUPTIME_SDS_MS + READINGTIME_SDS_MS))) {
		if (is_HPM_running) {
			HPM_cmd(PmSensorCmd::Stop);
		}
	} else {
		if (! is_HPM_running) {
			HPM_cmd(PmSensorCmd::Start);
		}

		while (serialSDS.available() > 0) {
			buffer = serialSDS.read();
			debug_outln(String(len) + " - " + String(buffer, DEC) + " - " + String(buffer, HEX) + " - " + int(buffer) + " .", DEBUG_MAX_INFO);
//			"aa" = 170, "ab" = 171, "c0" = 192
			value = int(buffer);
			switch (len) {
			case (0):
				if (value != 66) {
					len = -1;
				};
				break;
			case (1):
				if (value != 77) {
					len = -1;
				};
				break;
			case (2):
				checksum_is = value;
				break;
			case (6):
				pm25_serial += ( value << 8);
				break;
			case (7):
				pm25_serial += value;
				break;
			case (8):
				pm10_serial = ( value << 8);
				break;
			case (9):
				pm10_serial += value;
				break;
			case (30):
				checksum_should = ( value << 8 );
				break;
			case (31):
				checksum_should += value;
				break;
			}
			if (len > 2 && len < 30) { checksum_is += value; }
			len++;
			if (len == 32) {
				debug_out(FPSTR(DBG_TXT_CHECKSUM_IS), DEBUG_MED_INFO);
				debug_out(String(checksum_is + 143), DEBUG_MED_INFO);
				debug_out(FPSTR(DBG_TXT_CHECKSUM_SHOULD), DEBUG_MED_INFO);
				debug_outln(String(checksum_should), DEBUG_MED_INFO);
				if (checksum_should == (checksum_is + 143)) {
					checksum_ok = 1;
				} else {
					len = 0;
				};
				if (checksum_ok == 1 && (long(msSince(starttime)) > (long(cfg::sending_intervall_ms) - long(READINGTIME_SDS_MS)))) {
					if ((! isnan(pm10_serial)) && (! isnan(pm25_serial))) {
						hpm_pm10_sum += pm10_serial;
						hpm_pm25_sum += pm25_serial;
						if (hpm_pm10_min > pm10_serial) {
							hpm_pm10_min = pm10_serial;
						}
						if (hpm_pm10_max < pm10_serial) {
							hpm_pm10_max = pm10_serial;
						}
						if (hpm_pm25_min > pm25_serial) {
							hpm_pm25_min = pm25_serial;
						}
						if (hpm_pm25_max < pm25_serial) {
							hpm_pm25_max = pm25_serial;
						}
						debug_out(F("PM2.5 (sec.): "), DEBUG_MED_INFO);
						debug_outln(Float2String(double(pm25_serial)), DEBUG_MED_INFO);
						debug_out(F("PM10 (sec.) : "), DEBUG_MED_INFO);
						debug_outln(Float2String(double(pm10_serial)), DEBUG_MED_INFO);
						hpm_val_count++;
					}
					len = 0;
					checksum_ok = 0;
					pm10_serial = 0.0;
					pm25_serial = 0.0;
					checksum_is = 0;
				}
			}
			yield();
		}

	}
	if (send_now) {
		last_value_HPM_P1 = -1;
		last_value_HPM_P2 = -1;
		if (hpm_val_count > 2) {
			hpm_pm10_sum = hpm_pm10_sum - hpm_pm10_min - hpm_pm10_max;
			hpm_pm25_sum = hpm_pm25_sum - hpm_pm25_min - hpm_pm25_max;
			hpm_val_count = hpm_val_count - 2;
		}
		if (hpm_val_count > 0) {
			last_value_HPM_P1 = double(hpm_pm10_sum) / (hpm_val_count * 1.0);
			last_value_HPM_P2 = double(hpm_pm25_sum) / (hpm_val_count * 1.0);
			debug_outln("PM2.5: " + Float2String(last_value_HPM_P1), DEBUG_MIN_INFO);
			debug_outln("PM10:  " + Float2String(last_value_HPM_P2), DEBUG_MIN_INFO);
			debug_outln("-------", DEBUG_MIN_INFO);
			s += Value2Json(F("HPM_P1"), last_value_HPM_P1);
			s += Value2Json(F("HPM_P2"), last_value_HPM_P2);
		}
		hpm_pm10_sum = 0;
		hpm_pm25_sum = 0;
		hpm_val_count = 0;
		hpm_pm10_max = 0;
		hpm_pm10_min = 20000;
		hpm_pm25_max = 0;
		hpm_pm25_min = 20000;
		if (cfg::sending_intervall_ms > (WARMUPTIME_SDS_MS + READINGTIME_SDS_MS)) {
			HPM_cmd(PmSensorCmd::Stop);
		}
	}

	debug_outln(String(FPSTR(DBG_TXT_END_READING)) + FPSTR(SENSORS_HPM), DEBUG_MED_INFO);

	return s;
}

/*****************************************************************
 * read PPD42NS sensor values                                    *
 *****************************************************************/
static String sensorPPD() {
	String s;

	debug_outln(String(FPSTR(DBG_TXT_START_READING)) + FPSTR(SENSORS_PPD42NS), DEBUG_MED_INFO);

	if (msSince(starttime) <= SAMPLETIME_MS) {

		// Read pins connected to ppd42ns
		boolean valP1 = digitalRead(PPD_PIN_PM1);
		boolean valP2 = digitalRead(PPD_PIN_PM2);

		if(valP1 == LOW && trigP1 == false) {
			trigP1 = true;
			trigOnP1 = act_micro;
		}

		if (valP1 == HIGH && trigP1 == true) {
			durationP1 = act_micro - trigOnP1;
			lowpulseoccupancyP1 = lowpulseoccupancyP1 + durationP1;
			trigP1 = false;
		}

		if(valP2 == LOW && trigP2 == false) {
			trigP2 = true;
			trigOnP2 = act_micro;
		}

		if (valP2 == HIGH && trigP2 == true) {
			durationP2 = act_micro - trigOnP2;
			lowpulseoccupancyP2 = lowpulseoccupancyP2 + durationP2;
			trigP2 = false;
		}

	}
	// Checking if it is time to sample
	if (send_now) {
		const auto calcConcentration = [](double ratio) {
			/* spec sheet curve*/
			return (1.1 * pow(ratio, 3) - 3.8 * pow(ratio, 2) + 520 * ratio + 0.62);
		};

		last_value_PPD_P1 = -1;
		last_value_PPD_P2 = -1;
		double ratio = lowpulseoccupancyP1 / (SAMPLETIME_MS * 10.0);					// int percentage 0 to 100
		double concentration = calcConcentration(ratio);
		// Begin printing
		debug_out(F("LPO P10    : "), DEBUG_MIN_INFO);
		debug_outln(String(lowpulseoccupancyP1), DEBUG_MIN_INFO);
		debug_out(F("Ratio PM10 : "), DEBUG_MIN_INFO);
		debug_outln(Float2String(ratio) + " %", DEBUG_MIN_INFO);
		debug_out(F("PM10 Count : "), DEBUG_MIN_INFO);
		debug_outln(Float2String(concentration), DEBUG_MIN_INFO);

		// json for push to api / P1
		last_value_PPD_P1 = concentration;
		s += Value2Json(F("durP1"), String(lowpulseoccupancyP1));
		s += Value2Json(F("ratioP1"), ratio);
		s += Value2Json(F("P1"), last_value_PPD_P1);

		ratio = lowpulseoccupancyP2 / (SAMPLETIME_MS * 10.0);
		concentration = calcConcentration(ratio);
		// Begin printing
		debug_out(F("LPO PM25   : "), DEBUG_MIN_INFO);
		debug_outln(String(lowpulseoccupancyP2), DEBUG_MIN_INFO);
		debug_out(F("Ratio PM25 : "), DEBUG_MIN_INFO);
		debug_outln(Float2String(ratio) + " %", DEBUG_MIN_INFO);
		debug_out(F("PM25 Count : "), DEBUG_MIN_INFO);
		debug_outln(Float2String(concentration), DEBUG_MIN_INFO);

		// json for push to api / P2
		last_value_PPD_P2 = concentration;
		s += Value2Json(F("durP2"), String(lowpulseoccupancyP2));
		s += Value2Json(F("ratioP2"), ratio);
		s += Value2Json(F("P2"), last_value_PPD_P2);

		debug_outln("----", DEBUG_MIN_INFO);
	}

	debug_outln(String(FPSTR(DBG_TXT_END_READING)) + FPSTR(SENSORS_PPD42NS), DEBUG_MED_INFO);

	return s;
}

/*****************************************************************
   read SPS30 PM sensor values
 *****************************************************************/
static String sensorSPS30() {
	String s;

	debug_outln(String(FPSTR(DBG_TXT_START_READING)) + "SPS30", DEBUG_MED_INFO);

	last_value_SPS30_P0 = value_SPS30_P0 / SPS30_measurement_count;
	last_value_SPS30_P1 = value_SPS30_P1 / SPS30_measurement_count;
	last_value_SPS30_P2 = value_SPS30_P2 / SPS30_measurement_count;
	last_value_SPS30_P3 = value_SPS30_P3 / SPS30_measurement_count;
	last_value_SPS30_N0 = value_SPS30_N0 / SPS30_measurement_count;
	last_value_SPS30_N1 = value_SPS30_N1 / SPS30_measurement_count;
	last_value_SPS30_N2 = value_SPS30_N2 / SPS30_measurement_count;
	last_value_SPS30_N3 = value_SPS30_N3 / SPS30_measurement_count;
	last_value_SPS30_N4 = value_SPS30_N4 / SPS30_measurement_count;
	last_value_SPS30_TS = value_SPS30_TS / SPS30_measurement_count;

	debug_outln("PM1.0: " + Float2String(last_value_SPS30_P0), DEBUG_MIN_INFO);
	debug_outln("PM2.5: " + Float2String(last_value_SPS30_P1), DEBUG_MIN_INFO);
	debug_outln("PM4.0: " + Float2String(last_value_SPS30_P2), DEBUG_MIN_INFO);
	debug_outln("PM10:  " + Float2String(last_value_SPS30_P3), DEBUG_MIN_INFO);
	debug_outln("NC0.5: " + Float2String(last_value_SPS30_N0), DEBUG_MIN_INFO);
	debug_outln("NC1.0: " + Float2String(last_value_SPS30_N1), DEBUG_MIN_INFO);
	debug_outln("NC2.5: " + Float2String(last_value_SPS30_N2), DEBUG_MIN_INFO);
	debug_outln("NC4.0: " + Float2String(last_value_SPS30_N3), DEBUG_MIN_INFO);
	debug_outln("NC10:  " + Float2String(last_value_SPS30_N4), DEBUG_MIN_INFO);
	debug_outln("TPS:   " + Float2String(last_value_SPS30_TS), DEBUG_MIN_INFO);

	s += Value2Json(F("SPS30_P0"), last_value_SPS30_P0);
	s += Value2Json(F("SPS30_P1"), last_value_SPS30_P1);
	s += Value2Json(F("SPS30_P2"), last_value_SPS30_P2);
	s += Value2Json(F("SPS30_P3"), last_value_SPS30_P3);
	s += Value2Json(F("SPS30_N0"), last_value_SPS30_N0);
	s += Value2Json(F("SPS30_N1"), last_value_SPS30_N1);
	s += Value2Json(F("SPS30_N2"), last_value_SPS30_N2);
	s += Value2Json(F("SPS30_N3"), last_value_SPS30_N3);
	s += Value2Json(F("SPS30_N4"), last_value_SPS30_N4);
	s += Value2Json(F("SPS30_TS"), last_value_SPS30_TS);

	debug_outln("SPS30 read counter: " + String(SPS30_read_counter), DEBUG_MIN_INFO);
	debug_outln("SPS30 read error counter: " + String(SPS30_read_error_counter), DEBUG_MIN_INFO);

	SPS30_measurement_count = 0;
	SPS30_read_counter = 0;
	SPS30_read_error_counter = 0;
	value_SPS30_P0 = 0.0;
	value_SPS30_P1 = 0.0;
	value_SPS30_P2 = 0.0;
	value_SPS30_P3 = 0.0;
	value_SPS30_N0 = 0.0;
	value_SPS30_N1 = 0.0;
	value_SPS30_N2 = 0.0;
	value_SPS30_N3 = 0.0;
	value_SPS30_N4 = 0.0;
	value_SPS30_TS = 0.0;

	debug_outln(F("----"), DEBUG_MIN_INFO);
	debug_outln(String(FPSTR(DBG_TXT_END_READING)) + "SPS30", DEBUG_MED_INFO);
	return s;
}

/*****************************************************************
   read DNMS values
 *****************************************************************/
String sensorDNMS() {
	String s;
	uint16_t i;
	dnms_error = false;
	debug_outln(String(FPSTR(DBG_TXT_START_READING)) + "DNMS", DEBUG_MED_INFO);
	last_value_dnms_laeq = -1.0;
	last_value_dnms_la_min = -1.0;
	last_value_dnms_la_max = -1.0;

	if (dnms_calculate_leq() !=  0) {
		// error
		dnms_error = true;
	}
	data_ready = 0;
	dnms_error = true;
	for (i = 0; i < 20; i++) {
		delay(2);
		ret_dnms = dnms_read_data_ready(&data_ready);
		if ((ret_dnms == 0) && (data_ready != 0)) {
			dnms_error = false;
			break;
		}
	}
	if (!dnms_error) {
		if (dnms_read_leq(&dnms_values) == 0) {
			last_value_dnms_laeq = dnms_values.leq_a + dnms_corr_value;
			last_value_dnms_la_min = dnms_values.leq_a_min + dnms_corr_value;
			last_value_dnms_la_max = dnms_values.leq_a_max + dnms_corr_value;
		} else {
			// error
			dnms_error = true;
		}
	}
	if (dnms_error) {
		// es gab einen Fehler
		dnms_reset(); // try to reset dnms
		debug_outln(F("DNMS couldn't be read"), DEBUG_ERROR);
	} else {
		debug_out(FPSTR(DBG_TXT_DNMS_LAEQ), DEBUG_MIN_INFO);
		debug_outln(Float2String(last_value_dnms_laeq) + " dB", DEBUG_MIN_INFO);
		debug_out(FPSTR(DBG_TXT_DNMS_LA_MIN), DEBUG_MIN_INFO);
		debug_outln(Float2String(last_value_dnms_la_min) + " dB", DEBUG_MIN_INFO);
		debug_out(FPSTR(DBG_TXT_DNMS_LA_MAX), DEBUG_MIN_INFO);
		debug_outln(Float2String(last_value_dnms_la_max) + " dB", DEBUG_MIN_INFO);

		s += Value2Json(F("noise_LAeq"), last_value_dnms_laeq);
		s += Value2Json(F("noise_LA_min"), last_value_dnms_la_min);
		s += Value2Json(F("noise_LA_max"), last_value_dnms_la_max);
	}
	debug_outln(F("----"), DEBUG_MIN_INFO);

	debug_outln(String(FPSTR(DBG_TXT_END_READING)) + "DNMS", DEBUG_MED_INFO);
	return s;
}

/*****************************************************************
 * read GPS sensor values                                        *
 *****************************************************************/
String sensorGPS() {
	String s, gps_lat, gps_lon;

	debug_outln(String(FPSTR(DBG_TXT_START_READING)) + "GPS", DEBUG_MED_INFO);

	while (serialGPS.available() > 0) {
		if (gps.encode(serialGPS.read())) {
			if (gps.location.isValid()) {
				last_value_GPS_lat = gps.location.lat();
				last_value_GPS_lon = gps.location.lng();
				gps_lat = Float2String(last_value_GPS_lat, 6);
				gps_lon = Float2String(last_value_GPS_lon, 6);
			} else {
				last_value_GPS_lat = -200;
				last_value_GPS_lon = -200;
				debug_outln(F("Lat/Lng INVALID"), DEBUG_MAX_INFO);
			}
			if (gps.altitude.isValid()) {
				last_value_GPS_alt = gps.altitude.meters();
				String gps_alt = Float2String(last_value_GPS_lat, 2);
			} else {
				last_value_GPS_alt = -1000;
				debug_outln(F("Altitude INVALID"), DEBUG_MAX_INFO);
			}
			if (gps.date.isValid()) {
				String gps_date;
				if (gps.date.month() < 10) {
					gps_date += "0";
				}
				gps_date += String(gps.date.month());
				gps_date += "/";
				if (gps.date.day() < 10) {
					gps_date += "0";
				}
				gps_date += String(gps.date.day());
				gps_date += "/";
				gps_date += String(gps.date.year());
				last_value_GPS_date = gps_date;
			} else {
				debug_outln(F("Date INVALID"), DEBUG_MAX_INFO);
			}
			if (gps.time.isValid()) {
				String gps_time;
				if (gps.time.hour() < 10) {
					gps_time += "0";
				}
				gps_time += String(gps.time.hour());
				gps_time += ":";
				if (gps.time.minute() < 10) {
					gps_time += "0";
				}
				gps_time += String(gps.time.minute());
				gps_time += ":";
				if (gps.time.second() < 10) {
					gps_time += "0";
				}
				gps_time += String(gps.time.second());
				gps_time += ".";
				if (gps.time.centisecond() < 10) {
					gps_time += "0";
				}
				gps_time += String(gps.time.centisecond());
				last_value_GPS_time = gps_time;
			} else {
				debug_outln(F("Time: INVALID"), DEBUG_MAX_INFO);
			}
		}
	}

	if (send_now) {
		debug_outln("Lat/Lng: " + Float2String(last_value_GPS_lat, 6) + "," + Float2String(last_value_GPS_lon, 6), DEBUG_MIN_INFO);
		debug_outln("Altitude: " + Float2String(last_value_GPS_alt, 2), DEBUG_MIN_INFO);
		debug_outln("Date: " + last_value_GPS_date, DEBUG_MIN_INFO);
		debug_outln("Time " + last_value_GPS_time, DEBUG_MIN_INFO);
		debug_outln("----", DEBUG_MIN_INFO);
		s += Value2Json(F("GPS_lat"), Float2String(last_value_GPS_lat, 6));
		s += Value2Json(F("GPS_lon"), Float2String(last_value_GPS_lon, 6));
		s += Value2Json(F("GPS_height"), Float2String(last_value_GPS_alt, 2));
		s += Value2Json(F("GPS_date"), last_value_GPS_date);
		s += Value2Json(F("GPS_time"), last_value_GPS_time);
	}

	if ( gps.charsProcessed() < 10) {
		debug_outln(F("No GPS data received: check wiring"), DEBUG_ERROR);
	}

	debug_outln(String(FPSTR(DBG_TXT_END_READING)) + "GPS", DEBUG_MED_INFO);

	return s;
}

/*****************************************************************
 * AutoUpdate                                                    *
 *****************************************************************/
static void autoUpdate() {
	if (!cfg::auto_update) return;

	debug_outln(F("Starting OTA update ..."), DEBUG_MIN_INFO);
	debug_out(F("NodeMCU firmware : "), DEBUG_MIN_INFO);
	debug_outln(SOFTWARE_VERSION, DEBUG_MIN_INFO);
	debug_outln(UPDATE_HOST, DEBUG_MED_INFO);
	debug_outln(UPDATE_URL, DEBUG_MED_INFO);

	const String SDS_version = cfg::sds_read ? SDS_version_date() : "";
	display_debug(F("Looking for"), F("OTA update"));
	last_update_attempt = millis();
	String version = SOFTWARE_VERSION + String(" ") + esp_chipid + String(" ") + SDS_version + String(" ") +
					 String(cfg::current_lang) + String(" ") + String(INTL_LANG) + String(" ") +
					 String(cfg::use_beta ? "BETA" : "");
#if defined(ESP8266)
	const HTTPUpdateResult ret = ESPhttpUpdate.update(UPDATE_HOST, UPDATE_PORT, UPDATE_URL, version);
	String LastErrorString = ESPhttpUpdate.getLastErrorString().c_str();
#endif
#if defined(ESP32)
	WiFiClient client;
	t_httpUpdate_return ret = httpUpdate.update(client, UPDATE_HOST, UPDATE_PORT, UPDATE_URL, version);
	String LastErrorString = httpUpdate.getLastErrorString().c_str();
#endif
	switch(ret) {
	case HTTP_UPDATE_FAILED:
		debug_out(String(FPSTR(DBG_TXT_UPDATE)) + FPSTR(DBG_TXT_UPDATE_FAILED), DEBUG_ERROR);
		debug_outln(LastErrorString, DEBUG_ERROR);
		display_debug(FPSTR(DBG_TXT_UPDATE), FPSTR(DBG_TXT_UPDATE_FAILED));
		break;
	case HTTP_UPDATE_NO_UPDATES:
		debug_outln(String(FPSTR(DBG_TXT_UPDATE)) + FPSTR(DBG_TXT_UPDATE_NO_UPDATE), DEBUG_MIN_INFO);
		display_debug(FPSTR(DBG_TXT_UPDATE), FPSTR(DBG_TXT_UPDATE_NO_UPDATE));
		break;
	case HTTP_UPDATE_OK:
		debug_outln(String(FPSTR(DBG_TXT_UPDATE)) + FPSTR(DBG_TXT_UPDATE_OK), DEBUG_MIN_INFO); // may not called we reboot the ESP
		break;
	}
}

static String displayGenerateFooter(unsigned int screen_count) {
	String display_footer;
	for (unsigned int i = 0; i < screen_count; ++i) {
		display_footer += (i != (next_display_count % screen_count)) ? " . " : " o ";
	}
	return display_footer;
}

/*****************************************************************
 * display values                                                *
 *****************************************************************/
void display_values() {
	double t_value = -128.0;
	double h_value = -1.0;
	double p_value = -1.0;
	String t_sensor, h_sensor, p_sensor;
	float pm010_value = -1.0;
	float pm040_value = -1.0;
	double pm10_value = -1.0;
	double pm25_value = -1.0;
	String pm10_sensor;
	String pm25_sensor;
	float nc005_value = -1.0;
	float nc010_value = -1.0;
	float nc025_value = -1.0;
	float nc040_value = -1.0;
	float nc100_value = -1.0;
	float la_eq_value = -1.0;
	float la_max_value = -1.0;
	float la_min_value = -1.0;
	String la_sensor;
	float tps_value = -1.0;
	double lat_value = -200.0;
	double lon_value = -200.0;
	double alt_value = -1000.0;
	String gps_sensor, display_header;
	String display_lines[3] = { "", "", ""};
	int screen_count = 0;
	int screens[5];
	int line_count = 0;
	debug_outln(F("output values to display..."), DEBUG_MIN_INFO);
	if (cfg::ppd_read) {
		pm10_value = last_value_PPD_P1;
		pm10_sensor = FPSTR(SENSORS_PPD42NS);
		pm25_value = last_value_PPD_P2;
		pm25_sensor = FPSTR(SENSORS_PPD42NS);
	}
	if (cfg::pms_read) {
		pm10_value = last_value_PMS_P1;
		pm10_sensor = FPSTR(SENSORS_PMSx003);
		pm25_value = last_value_PMS_P2;
		pm25_sensor = FPSTR(SENSORS_PMSx003);
	}
	if (cfg::hpm_read) {
		pm10_value = last_value_HPM_P1;
		pm10_sensor = FPSTR(SENSORS_HPM);
		pm25_value = last_value_HPM_P2;
		pm25_sensor = FPSTR(SENSORS_HPM);
	}
	if (cfg::sps30_read) {
		pm10_sensor = FPSTR(SENSORS_SPS30);
		pm25_sensor = FPSTR(SENSORS_SPS30);
		pm010_value = last_value_SPS30_P0;
		pm25_value = last_value_SPS30_P1;
		pm040_value = last_value_SPS30_P2;
		pm10_value = last_value_SPS30_P3;
		nc005_value = last_value_SPS30_N0;
		nc010_value = last_value_SPS30_N1;
		nc025_value = last_value_SPS30_N2;
		nc040_value = last_value_SPS30_N3;
		nc100_value = last_value_SPS30_N4;
		tps_value = last_value_SPS30_TS;
	}
	if (cfg::sds_read) {
		pm10_value = last_value_SDS_P1;
		pm10_sensor = FPSTR(SENSORS_SDS011);
		pm25_value = last_value_SDS_P2;
		pm25_sensor = FPSTR(SENSORS_SDS011);
	}
	if (cfg::dht_read) {
		t_value = last_value_DHT_T;
		t_sensor = FPSTR(SENSORS_DHT22);
		h_value = last_value_DHT_H;
		h_sensor = FPSTR(SENSORS_DHT22);
	}
	if (cfg::ds18b20_read) {
		t_value = last_value_DS18B20_T;
		t_sensor = FPSTR(SENSORS_DS18B20);
	}
	if (cfg::htu21d_read) {
		t_value = last_value_HTU21D_T;
		t_sensor = FPSTR(SENSORS_HTU21D);
		h_value = last_value_HTU21D_H;
		h_sensor = FPSTR(SENSORS_HTU21D);
	}
	if (cfg::bmp_read) {
		t_value = last_value_BMP_T;
		t_sensor = FPSTR(SENSORS_BMP180);
		p_value = last_value_BMP_P;
		p_sensor = FPSTR(SENSORS_BMP180);
	}
	if (cfg::bmp280_read) {
		t_value = last_value_BMP280_T;
		t_sensor = FPSTR(SENSORS_BMP280);
		p_value = last_value_BMP280_P;
		p_sensor = FPSTR(SENSORS_BMP280);
	}
	if (cfg::bme280_read) {
		t_value = last_value_BME280_T;
		t_sensor = FPSTR(SENSORS_BME280);
		h_value = last_value_BME280_H;
		h_sensor = FPSTR(SENSORS_BME280);
		p_value = last_value_BME280_P;
		p_sensor = FPSTR(SENSORS_BME280);
	}
	if (cfg::dnms_read) {
		la_sensor = FPSTR(SENSORS_DNMS);
		la_eq_value = last_value_dnms_laeq;
		la_max_value = last_value_dnms_la_max;
		la_min_value = last_value_dnms_la_min;
	}
	if (cfg::gps_read) {
		lat_value = last_value_GPS_lat;
		lon_value = last_value_GPS_lon;
		alt_value = last_value_GPS_alt;
		gps_sensor = "NEO6M";
	}
	if (cfg::ppd_read || cfg::pms_read || cfg::hpm_read || cfg::sds_read) {
		screens[screen_count++] = 1;
	}
	if (cfg::dht_read || cfg::ds18b20_read || cfg::htu21d_read || cfg::bmp_read || cfg::bmp280_read || cfg::bme280_read) {
		screens[screen_count++] = 2;
	}
	if (cfg::gps_read) {
		screens[screen_count++] = 3;
	}
	if (cfg::sps30_read) {
		screens[screen_count++] = 4;
	}
	if (cfg::dnms_read) {
		screens[screen_count++] = 5;
	}
	if (cfg::display_wifi_info) {
		screens[screen_count++] = 6;	// Wifi info
	}
	if (cfg::display_device_info) {
		screens[screen_count++] = 7;	// chipID, firmware and count of measurements
	}
	if (cfg::has_display || cfg::has_sh1106 || cfg::has_lcd2004_27) {
		switch (screens[next_display_count % screen_count]) {
		case (1):
			display_header = pm25_sensor;
			if (pm25_sensor != pm10_sensor) {
				display_header += " / " + pm10_sensor;
			}
			display_lines[0] = "PM2.5: " + check_display_value(pm25_value, -1, 1, 6) + " µg/m³";
			display_lines[1] = "PM10:  " + check_display_value(pm10_value, -1, 1, 6) + " µg/m³";
			display_lines[2] = "";
			break;
		case (2):
			display_header = t_sensor;
			if (h_sensor != "" && t_sensor != h_sensor) {
				display_header += " / " + h_sensor;
			}
			if ((h_sensor != "" && p_sensor != "" && (h_sensor != p_sensor)) || (h_sensor == "" && p_sensor != "" && (t_sensor != p_sensor))) {
				display_header += " / " + p_sensor;
			}
			if (t_sensor != "") { display_lines[line_count++] = "Temp.: " + check_display_value(t_value, -128, 1, 6) + " °C"; }
			if (h_sensor != "") { display_lines[line_count++] = "Hum.:  " + check_display_value(h_value, -1, 1, 6) + " %"; }
			if (p_sensor != "") { display_lines[line_count++] = "Pres.: " + check_display_value(p_value / 100, (-1 / 100.0), 1, 6) + " hPa"; }
			while (line_count < 3) { display_lines[line_count++] = ""; }
			break;
		case (3):
			display_header = gps_sensor;
			display_lines[0] = "Lat: " + check_display_value(lat_value, -200.0, 6, 10);
			display_lines[1] = "Lon: " + check_display_value(lon_value, -200.0, 6, 10);
			display_lines[2] = "Alt: " + check_display_value(alt_value, -1000.0, 2, 10);
			break;
		case (4):
			display_header = FPSTR(SENSORS_SPS30);
			display_lines[0] = "PM: " + check_display_value(pm010_value, -1, 1, 4) + " " + check_display_value(pm25_value, -1, 1, 4) + " " + check_display_value(pm040_value, -1, 1, 4) + " " + check_display_value(pm10_value, -1, 1, 4);
			display_lines[1] = "NC: " + check_display_value(nc005_value, -1, 0, 3) + " " + check_display_value(nc010_value, -1, 0, 3) + " " + check_display_value(nc025_value, -1, 0, 3) + " " + check_display_value(nc040_value, -1, 0, 3) + " " + check_display_value(nc100_value, -1, 0, 3);
			display_lines[2] = "TPS: " + check_display_value(tps_value, -1, 2, 5) + " µm";
			break;
		case (5):
			display_header = FPSTR(SENSORS_DNMS);
			display_lines[0] = "LAeq: " + check_display_value(la_eq_value, -1, 1, 6) + " db(A)";
			display_lines[1] = "LA_max: " + check_display_value(la_max_value, -1, 1, 6) + " db(A)";
			display_lines[2] = "LA_min: " + check_display_value(la_min_value, -1, 1, 6) + " db(A)";;
			break;
		case (6):
			display_header = F("Wifi info");
			display_lines[0] = "IP: " + WiFi.localIP().toString();
			display_lines[1] = "SSID:" + WiFi.SSID();
			display_lines[2] = "Signal: " + String(calcWiFiSignalQuality(WiFi.RSSI())) + "%";
			break;
		case (7):
			display_header = F("Device Info");
			display_lines[0] = "ID: " + esp_chipid;
			display_lines[1] = "FW: " + String(SOFTWARE_VERSION);
			display_lines[2] = "Measurements: " + String(count_sends);
			break;
		}

		if (cfg::has_display) {
			display.clear();
			display.displayOn();
			display.setTextAlignment(TEXT_ALIGN_CENTER);
			display.drawString(64, 1, display_header);
			display.setTextAlignment(TEXT_ALIGN_LEFT);
			display.drawString(0, 16, display_lines[0]);
			display.drawString(0, 28, display_lines[1]);
			display.drawString(0, 40, display_lines[2]);
			display.setTextAlignment(TEXT_ALIGN_CENTER);
			display.drawString(64, 52, displayGenerateFooter(screen_count));
			display.display();
		}
		if (cfg::has_sh1106) {
			display_sh1106.clear();
			display_sh1106.displayOn();
			display_sh1106.setTextAlignment(TEXT_ALIGN_CENTER);
			display_sh1106.drawString(64, 1, display_header);
			display_sh1106.setTextAlignment(TEXT_ALIGN_LEFT);
			display_sh1106.drawString(0, 16, display_lines[0]);
			display_sh1106.drawString(0, 28, display_lines[1]);
			display_sh1106.drawString(0, 40, display_lines[2]);
			display_sh1106.setTextAlignment(TEXT_ALIGN_CENTER);
			display_sh1106.drawString(64, 52, displayGenerateFooter(screen_count));
			display_sh1106.display();
		}
		if (cfg::has_lcd2004_27) {
			display_header = String((next_display_count % screen_count) + 1) + "/" + String(screen_count) + " " + display_header;
			display_lines[0].replace(" µg/m³", empty_String);
			display_lines[0].replace("°", String(char(223)));
			display_lines[1].replace(" µg/m³", empty_String);
			lcd_2004_27.clear();
			lcd_2004_27.setCursor(0, 0);
			lcd_2004_27.print(display_header);
			lcd_2004_27.setCursor(0, 1);
			lcd_2004_27.print(display_lines[0]);
			lcd_2004_27.setCursor(0, 2);
			lcd_2004_27.print(display_lines[1]);
			lcd_2004_27.setCursor(0, 3);
			lcd_2004_27.print(display_lines[2]);
		}
	}

// ----5----0----5----0
// PM10/2.5: 1999/999
// T/H: -10.0°C/100.0%
// T/P: -10.0°C/1000hPa

	switch (screens[next_display_count % screen_count]) {
	case (1):
		display_lines[0] = "PM2.5: " + check_display_value(pm25_value, -1, 1, 6);
		display_lines[1] = "PM10:  " + check_display_value(pm10_value, -1, 1, 6);
		break;
	case (2):
		display_lines[0] = "T: " + check_display_value(t_value, -128, 1, 6) + char(223) + "C";
		display_lines[1] = "H: " + check_display_value(h_value, -1, 1, 6) + "%";
		break;
	case (3):
		display_lines[0] = "Lat: " + check_display_value(lat_value, -200.0, 6, 11);
		display_lines[1] = "Lon: " + check_display_value(lon_value, -200.0, 6, 11);
		break;
	case (4):
		display_lines[0] = WiFi.localIP().toString();
		display_lines[1] = WiFi.SSID();
		break;
	case (5):
		display_lines[0] = "ID: " + esp_chipid;
		display_lines[1] = "FW: " + String(SOFTWARE_VERSION);
		break;
	}

	if (cfg::has_lcd1602_27) {
		lcd_1602_27.clear();
		lcd_1602_27.setCursor(0, 0);
		lcd_1602_27.print(display_lines[0]);
		lcd_1602_27.setCursor(0, 1);
		lcd_1602_27.print(display_lines[1]);
	}
	if (cfg::has_lcd1602) {
		lcd_1602_3f.clear();
		lcd_1602_3f.setCursor(0, 0);
		lcd_1602_3f.print(display_lines[0]);
		lcd_1602_3f.setCursor(0, 1);
		lcd_1602_3f.print(display_lines[1]);
	}
	yield();
	next_display_count += 1;
	next_display_millis = millis() + DISPLAY_UPDATE_INTERVAL_MS;
}

/*****************************************************************
 * Init OLED display                                             *
 *****************************************************************/
void init_display() {
	display.init();
	display_sh1106.init();
	if (cfg::has_flipped_display) {
		display.flipScreenVertically();
		display_sh1106.flipScreenVertically();
	}
}

/*****************************************************************
 * Init LCD display                                              *
 *****************************************************************/
void init_lcd() {
	if (cfg::has_lcd1602_27) {
		lcd_1602_27.init();
		lcd_1602_27.backlight();
	}
	if (cfg::has_lcd1602) {
		lcd_1602_3f.init();
		lcd_1602_3f.backlight();
	}
	if (cfg::has_lcd2004_27) {
		lcd_2004_27.init();
		lcd_2004_27.backlight();
	}
}

/*****************************************************************
 * Init BMP280                                                   *
 *****************************************************************/
bool initBMP280(char addr) {
	debug_out(F("Trying BMP280 sensor on "), DEBUG_MIN_INFO);
	debug_out(String(addr, HEX), DEBUG_MIN_INFO);

	if (bmp280.begin(addr)) {
		debug_outln(FPSTR(DBG_TXT_FOUND), DEBUG_MIN_INFO);
		return true;
	} else {
		debug_outln(FPSTR(DBG_TXT_NOT_FOUND), DEBUG_MIN_INFO);
		return false;
	}
}

/*****************************************************************
 * Init BME280                                                   *
 *****************************************************************/
bool initBME280(char addr) {
	debug_out(F("Trying BME280 sensor on "), DEBUG_MIN_INFO);
	debug_out(String(addr, HEX), DEBUG_MIN_INFO);

	if (bme280.begin(addr)) {
		debug_outln(FPSTR(DBG_TXT_FOUND), DEBUG_MIN_INFO);
		bme280.setSampling(
			Adafruit_BME280::MODE_FORCED,
			Adafruit_BME280::SAMPLING_X1,
			Adafruit_BME280::SAMPLING_X1,
			Adafruit_BME280::SAMPLING_X1,
			Adafruit_BME280::FILTER_OFF);
		return true;
	} else {
		debug_outln(FPSTR(DBG_TXT_NOT_FOUND), DEBUG_MIN_INFO);
		return false;
	}
}

/*****************************************************************
   Init SPS30 PM Sensor
 *****************************************************************/
bool initSPS30() {
	char serial[SPS_MAX_SERIAL_LEN];
	debug_out(F("Trying SPS30 sensor on 0x69H "), DEBUG_MIN_INFO);
	sps30_reset();
	delay(200);
	if ( sps30_get_serial(serial) != 0 ) {
		debug_outln(FPSTR(DBG_TXT_NOT_FOUND), DEBUG_MIN_INFO);
		return false;
	} else {
		debug_out(F(" ... found, Serial-No.: "), DEBUG_MIN_INFO);
		debug_outln(String(serial), DEBUG_MIN_INFO);
		if (sps30_set_fan_auto_cleaning_interval(SPS30_AUTO_CLEANING_INTERVAL) != 0) {
			debug_outln(F("setting of Auto Cleaning Intervall SPS30 failed!"), DEBUG_MIN_INFO);
			return false;
		} else {
			delay(100);
			if (sps30_start_measurement() != 0) {
				debug_outln(F("SPS30 error starting measurement"), DEBUG_MIN_INFO);
				return false;
			}
		}
	}
	return true;
}

/*****************************************************************
   Init DNMS - Digital Noise Measurement Sensor
 *****************************************************************/
bool initDNMS() {
	debug_out(F("Trying DNMS sensor on 0x55H "), DEBUG_MIN_INFO);
	dnms_reset();
	delay(1000);
	dnms_corr_value = atof(cfg::dnms_correction);
	if (dnms_read_version(dnms_version) != 0) {
		debug_outln(FPSTR(DBG_TXT_NOT_FOUND), DEBUG_MIN_INFO);
		return false;
	} else {
		debug_out(FPSTR(DBG_TXT_FOUND), DEBUG_MIN_INFO);
		debug_outln(String(dnms_version), DEBUG_MIN_INFO);
		return true;
	}
}

static void powerOnTestSensors(bool initPmSensors) {
	if (cfg::ppd_read) {
		pinMode(PPD_PIN_PM1, INPUT_PULLUP);					// Listen at the designated PIN
		pinMode(PPD_PIN_PM2, INPUT_PULLUP);					// Listen at the designated PIN
		debug_outln(F("Read PPD..."), DEBUG_MIN_INFO);
	}

	if (cfg::sds_read) {
		debug_outln(F("Read SDS..."), DEBUG_MIN_INFO);
		if (initPmSensors) {
			debug_outln(F("Read SDS..."), DEBUG_MIN_INFO);
			SDS_cmd(PmSensorCmd::Start);
			delay(100);
		}
		SDS_cmd(PmSensorCmd::ContinuousMode);
		delay(100);
		if (initPmSensors) {
			debug_outln(F("Stopping SDS011..."), DEBUG_MIN_INFO);
			SDS_cmd(PmSensorCmd::Stop);
		}
	}

	if (cfg::pms_read) {
		debug_outln(F("Read PMS(1,3,5,6,7)003..."), DEBUG_MIN_INFO);
		if (initPmSensors) {
			PMS_cmd(PmSensorCmd::Start);
			delay(100);
		}
		PMS_cmd(PmSensorCmd::ContinuousMode);
		delay(100);
		if (initPmSensors) {
			debug_outln(F("Stopping PMS..."), DEBUG_MIN_INFO);
			PMS_cmd(PmSensorCmd::Stop);
		}
	}

	if (cfg::hpm_read) {
		debug_outln(F("Read HPM..."), DEBUG_MIN_INFO);
		if (initPmSensors) {
			HPM_cmd(PmSensorCmd::Start);
			delay(100);
		}
		HPM_cmd(PmSensorCmd::ContinuousMode);
		delay(100);
		if (initPmSensors) {
			debug_outln(F("Stopping HPM..."), DEBUG_MIN_INFO);
			HPM_cmd(PmSensorCmd::Stop);
		}
	}

	if (cfg::sps30_read) {
		debug_outln(F("Read SPS30..."), DEBUG_MIN_INFO);
		if (!initSPS30()) {
			debug_outln(F("Check SPS30 wiring"), DEBUG_MIN_INFO);
			sps30_init_failed = 1;
		}
	}

	if (cfg::dht_read) {
		dht.begin();										// Start DHT
		debug_outln(F("Read DHT..."), DEBUG_MIN_INFO);
	}

	if (cfg::htu21d_read) {
		htu21d.begin();										// Start HTU21D
		debug_outln(F("Read HTU21D..."), DEBUG_MIN_INFO);
	}

	if (cfg::bmp_read) {
		debug_outln(F("Read BMP..."), DEBUG_MIN_INFO);
		if (!bmp.begin()) {
			debug_outln(F("No valid BMP085 sensor, check wiring!"), DEBUG_MIN_INFO);
			bmp_init_failed = 1;
		}
	}

	if (cfg::bmp280_read) {
		debug_outln(F("Read BMP280..."), DEBUG_MIN_INFO);
		if (!initBMP280(0x76) && !initBMP280(0x77)) {
			debug_outln(F("Check BMP280 wiring"), DEBUG_MIN_INFO);
			bmp280_init_failed = 1;
		}
	}

	if (cfg::bme280_read) {
		debug_outln(F("Read BME280..."), DEBUG_MIN_INFO);
		if (!initBME280(0x76) && !initBME280(0x77)) {
			debug_outln(F("Check BME280 wiring"), DEBUG_MIN_INFO);
			bme280_init_failed = 1;
		}
	}

	if (cfg::ds18b20_read) {
		ds18b20.begin();									// Start DS18B20
		debug_outln(F("Read DS18B20..."), DEBUG_MIN_INFO);
	}

	if (cfg::dnms_read) {
		debug_outln(F("Read DNMS..."), DEBUG_MIN_INFO);
		if (!initDNMS()) {
			debug_outln(F("Check DNMS wiring"), DEBUG_MIN_INFO);
			dnms_init_failed = 1;
		}
	}

}

static void logEnabledAPIs() {
	debug_outln(F("Send to :"), DEBUG_MIN_INFO);
	if (cfg::send2dusti) {
		debug_outln(F("luftdaten.info"), DEBUG_MIN_INFO);
	}

	if (cfg::send2madavi) {
		debug_outln(F("Madavi.de"), DEBUG_MIN_INFO);
	}

	if (cfg::send2lora) {
		debug_outln(F("LoRa gateway"), DEBUG_MIN_INFO);
	}

	if (cfg::send2csv) {
		debug_outln(F("Serial as CSV"), DEBUG_MIN_INFO);
	}

	if (cfg::send2custom) {
		debug_outln(F("custom API"), DEBUG_MIN_INFO);
	}

	if (cfg::send2aircms) {
		debug_outln(F("aircms API"), DEBUG_MIN_INFO);
	}

	if (cfg::send2influx) {
		debug_outln(F("custom influx DB"), DEBUG_MIN_INFO);
	}
	debug_outln("", DEBUG_MIN_INFO);
	if (cfg::auto_update) {
		debug_outln(F("Auto-Update active..."), DEBUG_MIN_INFO);
		debug_outln("", DEBUG_MIN_INFO);
	}
}

static void logEnabledDisplays() {
	if (cfg::has_display || cfg::has_sh1106) {
		debug_outln(F("Show on OLED..."), DEBUG_MIN_INFO);
	}
	if (cfg::has_lcd1602 || cfg::has_lcd1602_27) {
		debug_outln(F("Show on LCD 1602 ..."), DEBUG_MIN_INFO);
	}
	if (cfg::has_lcd2004_27) {
		debug_outln(F("Show on LCD 2004 ..."), DEBUG_MIN_INFO);
	}
}

void time_is_set (void) {
	sntp_time_is_set = true;
}

static bool acquireNetworkTime() {
	int retryCount = 0;
	debug_outln(F("Setting time using SNTP"), DEBUG_MIN_INFO);
	time_t now = time(nullptr);
	debug_outln(ctime(&now), DEBUG_MIN_INFO);
	debug_outln(F("NTP.org:"), DEBUG_MIN_INFO);
#if defined(ESP8266)
	settimeofday_cb(time_is_set);
#endif
	configTime(8 * 3600, 0, "pool.ntp.org");
	while (retryCount++ < 20) {
		// later than 2000/01/01:00:00:00
		if (sntp_time_is_set) {
			now = time(nullptr);
			debug_outln(ctime(&now), DEBUG_MIN_INFO);
			return true;
		}
		delay(500);
		debug_out(".", DEBUG_MIN_INFO);
	}
	debug_outln(F("\nrouter/gateway:"), DEBUG_MIN_INFO);
	retryCount = 0;
	configTime(0, 0, WiFi.gatewayIP().toString().c_str());
	while (retryCount++ < 20) {
		// later than 2000/01/01:00:00:00
		if (sntp_time_is_set) {
			now = time(nullptr);
			debug_outln(ctime(&now), DEBUG_MIN_INFO);
			return true;
		}
		delay(500);
		debug_out(".", DEBUG_MIN_INFO);
	}
	return false;
}

static void checkForceRestart() {
	if (msSince(time_point_device_start_ms) > DURATION_BEFORE_FORCED_RESTART_MS) {
		ESP.restart();
	}
}

static unsigned long sendDataToOptionalApis(const String &data) {
	unsigned long sum_send_time = 0;

	if (cfg::send2madavi) {
		debug_outln(String(FPSTR(DBG_TXT_SENDING_TO)) + F("madavi.de: "), DEBUG_MIN_INFO);
		sum_send_time += sendData(data, 0, HOST_MADAVI, (cfg::ssl_madavi ? 443 : 80), URL_MADAVI, cfg::ssl_madavi, true, "", FPSTR(TXT_CONTENT_TYPE_JSON));
	}

	if (cfg::send2sensemap && (cfg::senseboxid[0] != '\0')) {
		debug_outln(String(FPSTR(DBG_TXT_SENDING_TO)) + F("opensensemap: "), DEBUG_MIN_INFO);
		String sensemap_path(tmpl(F(URL_SENSEMAP), cfg::senseboxid));
		sum_send_time += sendData(data, 0, HOST_SENSEMAP, PORT_SENSEMAP, sensemap_path.c_str(), true, false, "", FPSTR(TXT_CONTENT_TYPE_JSON));
	}

	if (cfg::send2fsapp) {
		debug_outln(String(FPSTR(DBG_TXT_SENDING_TO)) + F("Server FS App: "), DEBUG_MIN_INFO);
		sum_send_time += sendData(data, 0, HOST_FSAPP, PORT_FSAPP, URL_FSAPP, false, false, "", FPSTR(TXT_CONTENT_TYPE_JSON));
	}

	if (cfg::send2aircms) {
		debug_outln(String(FPSTR(DBG_TXT_SENDING_TO)) + F(" aircms.online: "), DEBUG_MIN_INFO);
		unsigned long ts = millis() / 1000;
		String login = esp_chipid;
		String token = WiFi.macAddress();

		String aircms_data = "L=" + login + "&t=" + String(ts, DEC) + "&airrohr=" + data;
		String token_hash = sha1Hex(token);
		String hash = hmac1(String(token_hash), aircms_data + token);
		char char_full_url[100];
		sprintf(char_full_url, "%s%s", URL_AIRCMS, hash.c_str());

		sum_send_time += sendData(aircms_data, 0, HOST_AIRCMS, PORT_AIRCMS, char_full_url, true, false, "", FPSTR(TXT_CONTENT_TYPE_TEXT_PLAIN));
	}

	if (cfg::send2influx) {
		debug_outln(String(FPSTR(DBG_TXT_SENDING_TO)) + F("custom influx db: "), DEBUG_MIN_INFO);
		const String data_4_influxdb = create_influxdb_string(data);
		sum_send_time += sendData(data_4_influxdb, 0, cfg::host_influx, cfg::port_influx, cfg::url_influx, cfg::ssl_influx, false, basic_auth_influx.c_str(), FPSTR(TXT_CONTENT_TYPE_INFLUXDB));
	}

	/*		if (send2lora) {
				debug_outln(F("## Sending to LoRa gateway: "), DEBUG_MIN_INFO);
				send_lora(data);
			}
	*/
	if (cfg::send2csv) {
		debug_outln(F("## Sending as csv: "), DEBUG_MIN_INFO);
		send_csv(data);
	}

	if (cfg::send2custom) {
		String data_4_custom = data;
		data_4_custom.remove(0, 1);
		data_4_custom = "{\"esp8266id\": \"" + String(esp_chipid) + "\", " + data_4_custom;
		debug_outln(String(FPSTR(DBG_TXT_SENDING_TO)) + F("custom api: "), DEBUG_MIN_INFO);
		sum_send_time += sendData(data_4_custom, 0, cfg::host_custom, cfg::port_custom, cfg::url_custom, cfg::ssl_custom || (cfg::port_custom == 443), false, basic_auth_custom.c_str(), FPSTR(TXT_CONTENT_TYPE_JSON));
	}
	return sum_send_time;
}

/*****************************************************************
 * The Setup                                                     *
 *****************************************************************/
extern "C" void setup() {
	Serial.begin(9600);					// Output to Serial at 9600 baud
	WiFi.persistent(false);
	WiFi.setAutoConnect(false);
	WiFi.mode(WIFI_OFF);
	starttime = millis();									// store the start time
	time_point_device_start_ms = starttime;
	starttime_SDS = starttime;
	next_display_millis = starttime + DISPLAY_UPDATE_INTERVAL_MS;
	rtcData.load();

#if defined(ESP32)
	serialSDS.begin(9600, SERIAL_8N1, D1, D2);
	serialGPS.begin(9600, SERIAL_8N1, D5, D6);
	pinMode(16, OUTPUT);
	digitalWrite(16, LOW);
	delay(50);
	digitalWrite(16, HIGH);
#endif
	Wire.begin(I2C_PIN_SDA, I2C_PIN_SCL);

#if defined(ESP8266)
	esp_chipid = String(ESP.getChipId());
#endif
#if defined(ESP32)
	uint64_t chipid_num;
	chipid_num = ESP.getEfuseMac();
	esp_chipid = String((uint16_t)(chipid_num >> 32), HEX);
	esp_chipid += String((uint32_t)chipid_num, HEX);
#endif
	serialSDS.begin(9600);
	debug_out(F("\nChipId: "), DEBUG_MIN_INFO);
	debug_outln(esp_chipid, DEBUG_MIN_INFO);
	cfg::initNonTrivials(esp_chipid.c_str());
	readConfig();

	if(rtcData.stateMachine == 0 || rtcData.stateMachine >= 100)
	{
		// for the powersave states, these initializations are not good
		init_display();
		init_lcd();
		setup_network();
		setup_webserver();
		create_basic_auth_strings();
		powerOnTestSensors(true);
	}

	if (cfg::gps_read) {
		serialGPS.begin(9600);
		debug_outln(F("Read GPS..."), DEBUG_MIN_INFO);
		disable_unneeded_nmea();
	}

	logEnabledAPIs();
	logEnabledDisplays();

	delay(50);

	// sometimes parallel sending data and web page will stop nodemcu, watchdogtimer set to 30 seconds
#if defined(ESP8266)
	wdt_disable();
	wdt_enable(30000);
#endif

	switch(rtcData.stateMachine) {
		case 1:
			loop_StateSensorWarmup(); // will never return
			break;

		case 2:
			loop_StateMeasureAndSend(); // will never return
			break;

		case 101:
			loop_StartupTemporaryWebServer();
			break;

		default:
			// normal mode
			break;
	}
}

/*****************************************************************
 * And action                                                    *
 *****************************************************************/
extern "C" void loop() {
	String result_PPD, result_SDS, result_PMS, result_HPM, result_SPS30;
	String result_DHT, result_HTU21D, result_BMP, result_BMP280;
	String result_BME280, result_DS18B20, result_GPS, result_DNMS;
	int16_t ret_SPS30;

	unsigned long sum_send_time = 0;

	act_micro = micros();
	act_milli = millis();
	send_now = msSince(starttime) > cfg::sending_intervall_ms;

	sample_count++;

#if defined(ESP8266)
	wdt_reset(); // nodemcu is alive
#endif

	if(rtcData.stateMachine == 101
		&& (msSince(time_point_device_start_ms) > (3*60*1000))) // TODO: add constant
	{
		debug_outln(F("Switching to powersave mode"), DEBUG_MIN_INFO);
		rtcData.stateMachine = 1;
		deepSleep(1000, true);
	}

	if (last_micro != 0) {
		unsigned long diff_micro = act_micro - last_micro;
		if (max_micro < diff_micro) {
			max_micro = diff_micro;
		}
		if (min_micro > diff_micro) {
			min_micro = diff_micro;
		}
	}
	last_micro = act_micro;

	if (cfg::sps30_read && ( !sps30_init_failed)) {
		if ((msSince(starttime) - SPS30_read_timer) > SPS30_WAITING_AFTER_LAST_READ) {
			SPS30_read_timer = msSince(starttime);
			ret_SPS30 = sps30_read_measurement(&sps30_values);
			++SPS30_read_counter;
			if (ret_SPS30 < 0) {
				debug_outln(F("SPS30 error reading measurement"), DEBUG_MIN_INFO);
				SPS30_read_error_counter++;
			} else {
				if (SPS_IS_ERR_STATE(ret_SPS30)) {
					debug_outln(F("SPS30 measurements may not be accurate"), DEBUG_MIN_INFO);
					SPS30_read_error_counter++;
				}
				value_SPS30_P0 += sps30_values.mc_1p0;
				value_SPS30_P1 += sps30_values.mc_2p5;
				value_SPS30_P2 += sps30_values.mc_4p0;
				value_SPS30_P3 += sps30_values.mc_10p0;
				value_SPS30_N0 += sps30_values.nc_0p5;
				value_SPS30_N1 += sps30_values.nc_1p0;
				value_SPS30_N2 += sps30_values.nc_2p5;
				value_SPS30_N3 += sps30_values.nc_4p0;
				value_SPS30_N4 += sps30_values.nc_10p0;
				value_SPS30_TS += sps30_values.tps;
				++SPS30_measurement_count;
			}
		}
	}

	if (cfg::ppd_read) {
		debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + "PPD", DEBUG_MAX_INFO);
		result_PPD = sensorPPD();
	}

	if ((msSince(starttime_SDS) > SAMPLETIME_SDS_MS) || send_now) {
		if (cfg::sds_read) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + "SDS", DEBUG_MAX_INFO);
			result_SDS = sensorSDS();
			starttime_SDS = act_milli;
		}

		if (cfg::pms_read) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + "PMS", DEBUG_MAX_INFO);
			result_PMS = sensorPMS();
			starttime_SDS = act_milli;
		}

		if (cfg::hpm_read) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + "HPM", DEBUG_MAX_INFO);
			result_HPM = sensorHPM();
			starttime_SDS = act_milli;
		}
	}

	server.handleClient();

	if (send_now) {
		if (cfg::dht_read) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + FPSTR(SENSORS_DHT22), DEBUG_MAX_INFO);
			result_DHT = sensorDHT();						// getting temperature and humidity (optional)
		}

		if (cfg::htu21d_read) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + FPSTR(SENSORS_HTU21D), DEBUG_MAX_INFO);
			result_HTU21D = sensorHTU21D();					// getting temperature and humidity (optional)
		}

		if (cfg::bmp_read && (! bmp_init_failed)) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + FPSTR(SENSORS_BMP180), DEBUG_MAX_INFO);
			result_BMP = sensorBMP();						// getting temperature and pressure (optional)
		}

		if (cfg::bmp280_read && (! bmp280_init_failed)) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + FPSTR(SENSORS_BMP280), DEBUG_MAX_INFO);
			result_BMP280 = sensorBMP280();					// getting temperature, humidity and pressure (optional)
		}

		if (cfg::bme280_read && (! bme280_init_failed)) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + FPSTR(SENSORS_BME280), DEBUG_MAX_INFO);
			result_BME280 = sensorBME280();					// getting temperature, humidity and pressure (optional)
		}

		if (cfg::ds18b20_read) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + FPSTR(SENSORS_DS18B20), DEBUG_MAX_INFO);
			result_DS18B20 = sensorDS18B20();				// getting temperature (optional)
		}

		if (cfg::sps30_read && (! sps30_init_failed)) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + FPSTR(SENSORS_SPS30), DEBUG_MAX_INFO);
			result_SPS30 = sensorSPS30();               // getting PM values
		}

		if (cfg::dnms_read && (! dnms_init_failed)) {
			debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + FPSTR(SENSORS_DNMS), DEBUG_MAX_INFO);
			result_DNMS = sensorDNMS();                 // getting noise measurement values from dnms (optional)
		}
	}

	if (cfg::gps_read && ((msSince(starttime_GPS) > SAMPLETIME_GPS_MS) || send_now)) {
		debug_outln(String(FPSTR(DBG_TXT_CALL_SENSOR)) + "GPS", DEBUG_MAX_INFO);
		result_GPS = sensorGPS();							// getting GPS coordinates
		starttime_GPS = act_milli;
	}

	if ((cfg::has_display || cfg::has_sh1106 || cfg::has_lcd2004_27 || cfg::has_lcd1602 ||
			cfg::has_lcd1602_27) && (act_milli > next_display_millis)) {
		display_values();
	}

	if (send_now) {
		debug_outln(F("Creating data string:"), DEBUG_MIN_INFO);
		String data = tmpl(FPSTR(data_first_part), SOFTWARE_VERSION);
		String data_sample_times = Value2Json(F("samples"), String(sample_count));
		data_sample_times += Value2Json(F("min_micro"), String(min_micro));
		data_sample_times += Value2Json(F("max_micro"), String(max_micro));

		String signal_strength = String(WiFi.RSSI());
		debug_out(F("WLAN signal strength: "), DEBUG_MIN_INFO);
		debug_out(signal_strength, DEBUG_MIN_INFO);
		debug_outln(" dBm", DEBUG_MIN_INFO);
		debug_outln("----", DEBUG_MIN_INFO);

		server.handleClient();
		yield();
		server.stop();
		const int HTTP_PORT_DUSTI = (cfg::ssl_dusti ? 443 : 80);
		if (cfg::ppd_read) {
			data += result_PPD;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(PPD42NS): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_PPD, PPD_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "PPD_");
			}
		}
		if (cfg::sds_read) {
			data += result_SDS;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(SDS): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_SDS, SDS_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "SDS_");
			}
		}
		if (cfg::pms_read) {
			data += result_PMS;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(PMS): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_PMS, PMS_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "PMS_");
			}
		}
		if (cfg::hpm_read) {
			data += result_HPM;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(HPM): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_HPM, HPM_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "HPM_");
			}
		}
		if (cfg::sps30_read && (! sps30_init_failed)) {
			data += result_SPS30;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(SPS30): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_SPS30, SPS30_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "SPS30_");
			}
		}
		if (cfg::dht_read) {
			data += result_DHT;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(DHT): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_DHT, DHT_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "DHT_");
			}
		}
		if (cfg::htu21d_read) {
			data += result_HTU21D;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(HTU21D): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_HTU21D, HTU21D_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "HTU21D_");
			}
		}
		if (cfg::bmp_read && (! bmp_init_failed)) {
			data += result_BMP;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(BMP): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_BMP, BMP_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "BMP_");
			}
		}
		if (cfg::bmp280_read && (! bmp280_init_failed)) {
			data += result_BMP280;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(BMP280): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_BMP280, BMP280_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "BMP280_");
			}
		}
		if (cfg::bme280_read && (! bme280_init_failed)) {
			data += result_BME280;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(BME280): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_BME280, BME280_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "BME280_");
			}
		}

		if (cfg::ds18b20_read) {
			data += result_DS18B20;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(DS18B20): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_DS18B20, DS18B20_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "DS18B20_");
			}
		}

		if (cfg::dnms_read && (! dnms_init_failed)) {
			data += result_DNMS;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(DNMS): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_DNMS, DNMS_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "DNMS_");
			}
		}

		if (cfg::gps_read) {
			data += result_GPS;
			if (cfg::send2dusti) {
				debug_outln(String(FPSTR(DBG_TXT_SENDING_TO_LUFTDATEN)) + F("(GPS): "), DEBUG_MIN_INFO);
				sum_send_time += sendLuftdaten(result_GPS, GPS_API_PIN, HOST_DUSTI, HTTP_PORT_DUSTI, URL_DUSTI, cfg::ssl_dusti, true, "GPS_");
			}
		}

		data_sample_times += Value2Json(F("signal"), signal_strength);
		data += data_sample_times;

		if ((unsigned)(data.lastIndexOf(',') + 1) == data.length()) {
			data.remove(data.length() - 1);
		}
		data += "]}";

		setup_network();
		sum_send_time += sendDataToOptionalApis(data);

		server.begin();

		checkForceRestart();

		if (msSince(last_update_attempt) > PAUSE_BETWEEN_UPDATE_ATTEMPTS_MS) {
			autoUpdate();
		}

		sending_time = (4 * sending_time + sum_send_time) / 5;
		debug_out(F("Time for sending data (ms): "), DEBUG_MIN_INFO);
		debug_outln(String(sending_time), DEBUG_MIN_INFO);

		// reconnect to WiFi if disconnected
		if (WiFi.status() != WL_CONNECTED) {
			debug_out(F("Connection lost, reconnecting "), DEBUG_MIN_INFO);
			WiFi.reconnect();
			waitForWifiToConnect(20);
			debug_outln("", DEBUG_MIN_INFO);
		}

		// Resetting for next sampling
		last_data_string = data;
		lowpulseoccupancyP1 = 0;
		lowpulseoccupancyP2 = 0;
		sample_count = 0;
		last_micro = 0;
		min_micro = 1000000000;
		max_micro = 0;
		sum_send_time = 0;
		starttime = millis();								// store the start time
		first_cycle = false;
		count_sends += 1;
	}
	yield();
	if (sample_count % 500 == 0) {
//		Serial.println(ESP.getFreeHeap(),DEC);
#if defined(ESP8266)
		MDNS.update();
#endif
	}
}

