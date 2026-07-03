#include <SoftwareSerial.h>

SoftwareSerial loraSerial(10, 11); 

#define M0 7
#define M1 6

void setup() {
  Serial.begin(9600);
  loraSerial.begin(9600);
  
  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);

  // NORMAL MOD (MODE 0)
  digitalWrite(M0, LOW); 
  digitalWrite(M1, LOW); 

  Serial.println("VERICI: Sinyal gonderiliyor...");
}

void loop() {
  String mesaj = "Telemetri_Verisi:100";

  // Adres: 1, Kanal: 23
  loraSerial.write((uint8_t)0x00); 
  loraSerial.write((uint8_t)0x01); 
  loraSerial.write((uint8_t)0x17); 
  
  loraSerial.println(mesaj);
  Serial.println("Paket Gonderildi: " + mesaj);

  delay(2000); 
}
