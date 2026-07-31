#ifndef CONFIG_H
#define CONFIG_H

#define SERIAL_BAUD    9600
#define LORA_SERIAL    9600

#define ALARM_MAX      55 // Derece F
#define SICAKLIK_MAX   70 // Derece F
#define LOST_MAX       60 // Saniye

#define CAN_CS_PIN     53
#define CAN_INT_PIN    21
#define BMS_CAN_SO     50
#define BMS_CAN_SI     51
#define BMS_CAN_SCK    52

#define HMI_TX         xx
#define HMI_RX         xx

#define SICAKLIK_DS1   A0 // F
#define SICAKLIK_DS2   A1 // F
#define SICAKLIK_DS3   A2 // F

#define TELEMETRY_SEND 4 // Saniye

#define LORA_RX        0
#define LORA_TX        1
#define LORA_M0        2
#define LORA_M1        3

#define SD_MODULE      xx

#define ALARM_PIN      6 // F
#define ROLE_PIN       7 // F

enum ActiveStatus {
  STATE_IDLE,
  STATE_RUNNING,
  STATE_OBSTACLE,
  STATE_STOPPED,
  STATE_ERROR
};

#endif