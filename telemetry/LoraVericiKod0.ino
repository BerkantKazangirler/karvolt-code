#include <SoftwareSerial.h>

SoftwareSerial loraSerial(10, 11); 

#define M0 7
#define M1 6

void setup() {
  Serial.begin(9600);
  loraSerial.begin(9600);

  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);

  // NORMAL MOD (MODE 0): M0=0, M1=0
  digitalWrite(M0, LOW); 
  digitalWrite(M1, LOW); 

  Serial.println("ALICI DINLEMEDE: Telemetri bekleniyor...");
}

void loop() {
  if (loraSerial.available()) {
    // Gelen veriyi satır sonuna kadar oku
    String gelen = loraSerial.readStringUntil('\n'); 
    
    Serial.print("YAKALANAN VERI: ");
    Serial.println(gelen);
    Serial.println("-------------------------");
  }
}