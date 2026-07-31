#define M0 7
#define M1 6

int sayac = 0;

void setup() {
  Serial.begin(9600);   // Bilgisayar iletişimi
  Serial1.begin(9600);  // LoRa iletişimi (Pin 18 ve 19)

  pinMode(M0, OUTPUT);
  pinMode(M1, OUTPUT);

  digitalWrite(M0, LOW); 
  digitalWrite(M1, LOW); 

  delay(500);
  Serial.println("VERICI (Mega): Sinyal gonderiliyor...");
}

void loop() {
  String mesaj = "Telemetri_Verisi:" + String(sayac);

  Serial1.println(mesaj); // Serial1'e yazıyoruz
  
  Serial.println("Paket Gonderildi: " + mesaj);

  sayac++;
  delay(2000); 
}