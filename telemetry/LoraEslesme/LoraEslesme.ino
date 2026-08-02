#include <SoftwareSerial.h>

SoftwareSerial mySerial(10, 11); // RX, TX

#define M0 7
#define M1 6

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);

  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);

  digitalWrite(M0, LOW);
  digitalWrite(M1, HIGH);
}

void loop() {
  if (Serial.available()) {
    mySerial.write(Serial.read());
  }

  if (mySerial.available()) {
    Serial.write(mySerial.read());
  }
}