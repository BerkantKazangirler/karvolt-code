#ifndef CONFIG_H
#define CONFIG_H

#define SERIAL_BAUD    9600
#define LORA_SERIAL    9600
#define HMI_BAUD       9600

#define ALARM_MAX      55    // Derece
#define SICAKLIK_MAX   70    // Derece
#define LOST_MAX       60    // Saniye
#define HIZ_KRITIK_KMH 60.0

#define CAN_CS_PIN     53
#define CAN_INT_PIN    21

#define HALL_PIN       2

#define SICAKLIK_DS1   A0
#define SICAKLIK_DS2   A1
#define SICAKLIK_DS3   A2

#define IZOLASYON_PIN  A9 
#define BUZZER_PIN     28 

#define IMD_ADC_FAULT_POS  225
#define IMD_ADC_WARN_POS   450
#define IMD_ADC_WARN_NEG   585 
#define IMD_ADC_FAULT_NEG  635

#define SEND_DELAY     4000
#define LORA_SERIAL_P  Serial1 
#define LORA_M0        4
#define LORA_M1        5

#define SARJ_PIN       3  
#define SD_CS          10
#define ALARM_PIN      6
#define ROLE_PIN       7
#define HMI_SERIAL     Serial2

#define BATARYA_PAKET_KAPASITE_WH  5000.0

enum ActiveStatus {
  STATE_IDLE     = 0,
  STATE_RUNNING  = 1,
  STATE_CHARGING = 2,
  STATE_CRITICAL = 3,
  STATE_ERROR    = 4,
  STATE_IMD      = 5
};

#endif