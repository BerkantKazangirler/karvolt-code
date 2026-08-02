// --- PIN TANIMLAMALARI ---
const int buzzerPin = 9;   
const int potPin = A0;     
const int lm35Pin = A1;    
const int voltajPin = A2;  

unsigned long eskiZaman = 0;
float sanalBatarya = 100.0; 
String sonMesaj = "ILK_ACILIS"; 
int eskiHiz = -1; // İbre kontrolü için eski hız değişkeni

void setup() {
  Serial.begin(9600); 
  pinMode(buzzerPin, OUTPUT);

  tone(buzzerPin, 1000);  
  delay(300);
  noTone(buzzerPin);

  // Başlangıç değerlerini gönder
  int ilkHiz = map(analogRead(potPin), 0, 1023, 0, 120);
  sendToNextion("h0.val", ilkHiz);
  hataMesajiGonder("SISTEM HAZIR"); 
  delay(500); 
}

void loop() {
  // --- 1. VERİ OKUMA ---
  int hiz = map(analogRead(potPin), 0, 1023, 0, 120);
  int okunanTemp = analogRead(lm35Pin);
  float mv = okunanTemp * (5000.0 / 1024.0);
  int sicaklik = mv / 10.0;
  int gerilimGonder = map(analogRead(voltajPin), 0, 1023, 0, 500); 

  if (hiz > 0) sanalBatarya -= (hiz / 1500.0); 
  if (sanalBatarya < 0) sanalBatarya = 0;
  int bataryaYuzde = (int)sanalBatarya;

  // --- 2. İBRE ÇİZİMİ VE HIZ GÜNCELLEME (Değişim Kontrolü) ---
  // Hız 2 birimden fazla değiştiyse çizim yap
  if (abs(hiz - eskiHiz) >= 2) {
    
    // Önce ekranı temizle (Eski çizgileri siler)
    Serial.print("ref page0"); 
    terminateNextion();

    // Hız değerini gönder (h0 veya n0 için)
    sendToNextion("h0.val", hiz);

    // --- İBRE ÇİZİM KOMUTU (Örnek) ---
    // Eğer line komutu kullanıyorsan buraya ekleyebilirsin.
    // Örn: Serial.print("line 240,150,x2,y2,RED"); 
    // terminateNextion();

    eskiHiz = hiz; // Yeni hızı kaydet
  }

  // --- 3. DİĞER VERİLERİ GÜNCELLEME ---
  sendToNextion("n1.val", sicaklik);
  sendToNextion("n2.val", gerilimGonder);
  sendToNextion("j0.val", bataryaYuzde);

  int tBar = map(sicaklik, 0, 150, 0, 100);
  sendToNextion("j1.val", constrain(tBar, 0, 100));

  Serial.print("t1.txt=\"%"); Serial.print(bataryaYuzde); Serial.print("\"");
  terminateNextion();

  // --- 4. HATA VE KRİTİK DURUM MANTIĞI ---
  String suankiDurum = ""; 
  if (hiz > 70) suankiDurum += "HIZ "; 
  if (sicaklik > 24) suankiDurum += "ISI "; 
  if (gerilimGonder > 450) suankiDurum += "VOLT "; 
  if (bataryaYuzde < 50) suankiDurum += "BATARYA ";

  if (suankiDurum != sonMesaj) {
    if (suankiDurum == "") {
      noTone(buzzerPin);
      hataMesajiGonder("SISTEM NORMAL");
    } else {
      tone(buzzerPin, 1500); 
      hataMesajiGonder("KRITIK: " + suankiDurum);
    }
    sonMesaj = suankiDurum; 
  }

  delay(100); // 150ms yerine 100ms yaparak biraz daha akıcılık kazandırabilirsin
}

// Yardımcı Fonksiyonlar
void sendToNextion(String obj, int val) {
  Serial.print(obj + "=");
  Serial.print(val);
  terminateNextion();
}

void terminateNextion() {
  Serial.write(0xff); Serial.write(0xff); Serial.write(0xff);
}

void hataMesajiGonder(String mesaj) {
  Serial.print("t0.txt=\"");
  Serial.print(mesaj);
  Serial.print("\"");
  terminateNextion();
}