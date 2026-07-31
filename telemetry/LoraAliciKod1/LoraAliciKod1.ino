#include <SoftwareSerial.h>

SoftwareSerial loraSerial(10, 11); // RX, TX

#define M0 7
#define M1 6
#define LED_PIN 13 

unsigned long ledTimer = 0;
bool ledDurum = false;

void setup() {
  Serial.begin(9600);
  loraSerial.begin(9600);

  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // NORMAL MOD (MODE 0)
  digitalWrite(M0, LOW); 
  digitalWrite(M1, LOW); 

  delay(500);
  Serial.println("ALICI: Veri bekleniyor...");
}

void loop() {
  // Veri Geldi mi?
  if (loraSerial.available()) {
    String gelen = loraSerial.readStringUntil('\n'); 
    
    if (gelen.length() > 0) {
      Serial.println("GELEN: " + gelen);
      
      // LED'i yak ve zamanlayıcıyı başlat (delay kullanmadan!)
      digitalWrite(LED_PIN, HIGH);
      ledDurum = true;
      ledTimer = millis(); 
    }
  }

  // LED'i kilitlenme yapmadan 200ms sonra söndür
  if (ledDurum && (millis() - ledTimer > 200)) {
    digitalWrite(LED_PIN, LOW);
    ledDurum = false;
  }
}