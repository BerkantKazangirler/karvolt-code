#ifndef CONFIG_H
#define CONFIG_H

#define SERIAL_BAUD    9600
#define LORA_SERIAL    9600

#define ALARM_MAX      55 // Derece F
#define SICAKLIK_MAX   70 // Derece F
#define LOST_MAX       60 // Saniye

#define BMS_CAN_CS     xx
#define BMS_CAN_INT    xx

#define HMI_TX         xx
#define HMI_RX         xx

#define SICAKLIK_DS1   3 // F
#define SICAKLIK_DS2   4 // F
#define SICAKLIK_DS3   5 // F

#define TELEMETRY_SEND 4 // Saniye

#define LORA_RX        6
#define LORA_TX        7
#define LORA_M0        8
#define LORA_M1        9

#define SD_MODULE      xx

#define ALARM_PIN      xx // F
#define ROLE_PIN       xx // F

enum ActiveStatus {
  STATE_IDLE,
  STATE_RUNNING,
  STATE_OBSTACLE,
  STATE_STOPPED,
  STATE_ERROR
};

#endif