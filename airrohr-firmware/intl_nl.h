/*
 *	airRohr firmware
 *	Copyright (C) 2016-2018  Code for Stuttgart a.o.
 * 
 *  Dutch translations
 * 
 *	Texts should be as short as possible
 *	We only have 512 kB for the entire firmware
 */

const char INTL_LANG[] = "NL";
const char INTL_PM_SENSOR[] PROGMEM = "Fijnstofsensor";
const char INTL_CONFIGURATION[] PROGMEM = "Configuratie";
const char INTL_WIFI_SETTINGS[] PROGMEM = "WiFi-instellingen";
const char INTL_WIFI_NETWORKS[] PROGMEM = "Zoeken naar WiFi-netwerken ...";
const char INTL_LANGUAGE[] PROGMEM = "Taal";
const char INTL_NO_NETWORKS[] PROGMEM =  "Geen WiFi-netwerk gevonden";
const char INTL_NETWORKS_FOUND[] PROGMEM = "Netwerk gevonden: ";
const char INTL_AB_HIER_NUR_ANDERN[] PROGMEM = "Geavanceerde instellingen (enkel als je weet wat je doet)";
const char INTL_SAVE[] PROGMEM = "Opslaan";
const char INTL_SENSORS[] PROGMEM = "Sensors";
const char INTL_SDS011[] PROGMEM = "SDS011 ({pm})";
const char INTL_PMS[] PROGMEM = "Plantower PMS(1,3,5,6,7)003 ({pm})";
const char INTL_HPM[] PROGMEM = "Honeywell fijnstofsensor";
const char INTL_PPD42NS[] PROGMEM = "PPD42NS";
const char INTL_DHT22[] PROGMEM = "DHT22 ({t}, {h})";
const char INTL_HTU21D[] PROGMEM = "HTU21D ({t}, {h})";
const char INTL_BMP180[] PROGMEM = "BMP180 ({t}, {p})";
const char INTL_BMP280[] PROGMEM = "BMP280 ({t}, {p})";
const char INTL_BME280[] PROGMEM = "BME280 ({t}, {h}, {p})";
const char INTL_DS18B20[] PROGMEM = "DS18B20 ({t})";
const char INTL_NEO6M[] PROGMEM = "GPS (NEO 6M)";
const char INTL_BASICAUTH[] PROGMEM = "Beperk toegang";
const char INTL_FS_WIFI[] PROGMEM = "Instellingen voor rechtstreekse WiFi-verbinding";
const char INTL_FS_WIFI_DESCRIPTION[] PROGMEM = "Netwerknaam en -wachtwoord van de fijnstofsensor";
const char INTL_FS_WIFI_NAME[] PROGMEM = "Naam (SSID)";
const char INTL_MORE_SETTINGS[] PROGMEM ="Meer instellingen";
const char INTL_AUTO_UPDATE[] PROGMEM = "Firmware automatisch bijwerken";
const char INTL_USE_BETA[] PROGMEM = "Installeer beta-firmwares";
const char INTL_DISPLAY[] PROGMEM = "OLED SSD1306";
const char INTL_SH1106[] PROGMEM = "OLED SH1106";
const char INTL_LCD1602_27[] PROGMEM = "LCD 1602 (I2C: 0x27)";
const char INTL_LCD1602_3F[] PROGMEM = "LCD 1602 (I2C: 0x3F)";
const char INTL_LCD2004_27[] PROGMEM = "LCD 2004 (I2C: 0x27)";
const char INTL_DEBUG_LEVEL[] PROGMEM = "Debugniveau";
const char INTL_MEASUREMENT_INTERVAL[] PROGMEM = "Meetinterval";
const char INTL_DURATION_ROUTER_MODE[] PROGMEM = "Tijdsduur routermodus";
const char INTL_MORE_APIS[] PROGMEM = "Meer API's";
const char INTL_SEND_TO_OWN_API[] PROGMEM = "Verzend data naar eigen API";
const char INTL_SERVER[] PROGMEM = "Serveradres";
const char INTL_PATH[] PROGMEM = "Pad";
const char INTL_PORT[] PROGMEM = "Poort";
const char INTL_USER[] PROGMEM = "Gebruiker";
const char INTL_PASSWORD[] PROGMEM = "Wachtwoord";
const char INTL_SEND_TO[] PROGMEM = "Verzend naar {v}";
const char INTL_READ_FROM[] PROGMEM = "Lees uit {v}";
const char INTL_SENSOR_IS_REBOOTING[] PROGMEM = "Toestel wordt opnieuw opgestart";
const char INTL_RESTART_DEVICE[] PROGMEM = "Herstart toestel";
const char INTL_DELETE_CONFIG[] PROGMEM = "Verwijder opgeslagen configuratie";
const char INTL_RESTART_SENSOR[] PROGMEM = "Herstart sensor";
const char INTL_HOME[] PROGMEM = "Home";
const char INTL_BACK_TO_HOME[] PROGMEM = "Terug naar homepagina";
const char INTL_CURRENT_DATA[] PROGMEM = "Huidige data";
const char INTL_ACTIVE_SENSORS_MAP[] PROGMEM = "Actieve sensors kaart (externe link)";
const char INTL_CONFIGURATION_DELETE[] PROGMEM = "Configuratie verwijderen";
const char INTL_CONFIGURATION_REALLY_DELETE[] PROGMEM = "Bent u zeker dat u de configuratie wilt verwijderen?";
const char INTL_DELETE[] PROGMEM = "Verwijderen";
const char INTL_CANCEL[] PROGMEM = "Annuleren";
const char INTL_REALLY_RESTART_SENSOR[] PROGMEM = "Bent u zeker dat u de sensor wilt herstarten?";
const char INTL_RESTART[] PROGMEM = "Herstart";
const char INTL_SAVE_AND_RESTART[] PROGMEM = "Opslaan en herstarten";
const char INTL_FIRMWARE[] PROGMEM = "Firmware-versie";
const char INTL_DEBUG_SETTING_TO[] PROGMEM = "Debugniveau naar ";
const char INTL_NONE[] PROGMEM = "Uit";
const char INTL_ERROR[] PROGMEM = "enkel foutmeldingen";
const char INTL_WARNING[] PROGMEM = "waarschuwingen";
const char INTL_MIN_INFO[] PROGMEM = "min. info";
const char INTL_MED_INFO[] PROGMEM = "mid. info";
const char INTL_MAX_INFO[] PROGMEM = "max. info";
const char INTL_CONFIG_DELETED[] PROGMEM = "Configuratie is verwijderd";
const char INTL_CONFIG_CAN_NOT_BE_DELETED[] PROGMEM = "Configuratie kan niet worden verwijderd";
const char INTL_CONFIG_NOT_FOUND[] PROGMEM = "Configuratie is niet gevonden";
const char INTL_TIME_TO_FIRST_MEASUREMENT[] PROGMEM = "Nog {v} seconden voor de eerste meting.";
const char INTL_TIME_SINCE_LAST_MEASUREMENT[] PROGMEM = " seconden sinds laatste meting.";
const char INTL_PARTICLES_PER_LITER[] PROGMEM = "deeltjes/liter";
const char INTL_PARTICULATE_MATTER[] PROGMEM = "fijnstof";
const char INTL_TEMPERATURE[] PROGMEM = "Temperatuur";
const char INTL_HUMIDITY[] PROGMEM = "Rel. luchtvochtigheid";
const char INTL_PRESSURE[] PROGMEM = "Luchtdruk";
const char INTL_LATITUDE[] PROGMEM = "Breedtegraad";
const char INTL_LONGITUDE[] PROGMEM = "Lengtegraad";
const char INTL_ALTITUDE[] PROGMEM = "Hoogte";
const char INTL_DATE[] PROGMEM = "Datum";
const char INTL_TIME[] PROGMEM = "Tijd";
const char INTL_SIGNAL_STRENGTH[] PROGMEM = "Signaalsterkte";
const char INTL_SIGNAL_QUALITY[] PROGMEM = "Signaalkwaliteit";
const char INTL_NUMBER_OF_MEASUREMENTS[] PROGMEM = "Aantal metingen:";
const char INTL_SENSOR[] PROGMEM = "Sensor";
const char INTL_PARAMETER[] PROGMEM = "Parameter";
const char INTL_VALUE[] PROGMEM = "Waarde";

const char LUFTDATEN_INFO_LOGO_SVG[] PROGMEM = "<svg id=\"Layer_1\" xmlns=\"http://www.w3.org/2000/svg\" x=\"0px\" y=\"0px\" width=\"100px\" height=\"88.891px\" viewBox=\"0 0 100 88.891\" enable-background=\"new 0 0 100 88.891\"><path fill=\"#fff\" d=\"M90.28 66.57v.1H83.33c-2.04 0-3.7-1.66-3.7-3.7 0-2.05 1.66-3.7 3.7-3.7h5.56c2.03 0 3.7-1.67 3.7-3.7 0-2.06-1.67-3.72-3.7-3.72s-3.72-1.66-3.72-3.7c0-2.05 1.66-3.7 3.7-3.7.26 0 .5.02.75.07 5.8.4 10.37 5.15 10.37 11.04 0 5.66-4.24 10.32-9.72 11z\"/><path fill=\"#fff\" d=\"M70.37 44.44c-2.04 0-3.7-1.65-3.7-3.7 0-2.04 1.66-3.7 3.7-3.7.26 0 .5.02.75.07 5.78.4 10.36 5.17 10.36 11.05 0 5.66-4.24 10.33-9.72 11v.1H29.63c-2.04 0-3.7 1.67-3.7 3.7 0 2.06 1.65 3.72 3.7 3.72h42.14v.1c5.47.68 9.7 5.35 9.7 11 0 5.9-4.57 10.65-10.35 11.05-.25.04-.5.07-.75.07-2.04 0-3.7-1.67-3.7-3.7 0-2.06 1.66-3.72 3.7-3.72s3.7-1.66 3.7-3.7c0-2.05-1.66-3.7-3.7-3.7H28.23v-.1c-5.47-.7-9.7-5.35-9.7-11.02 0-1.95.53-3.76 1.42-5.35C8.35 53.6 0 42.6 0 29.64 0 13.27 13.27 0 29.63 0c11.12 0 20.8 6.13 25.86 15.2 1.22-.22 2.47-.38 3.76-.38 10.92 0 19.98 7.88 21.85 18.26-2.57-1.84-5.64-3.05-8.97-3.36-2.55-4.48-7.35-7.5-12.87-7.5-2.96 0-5.7.9-8.03 2.4-2.28-9.85-11.06-17.2-21.6-17.2C17.36 7.4 7.4 17.35 7.4 29.6c0 12.28 9.96 22.23 22.23 22.23h40.74c2.04 0 3.7-1.66 3.7-3.7 0-2.05-1.65-3.7-3.7-3.7z\"/></svg>";
