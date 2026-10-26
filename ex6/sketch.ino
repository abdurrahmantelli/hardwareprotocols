#include <Wire.h>
#include <SPI.h>
#include "FS.h"
#include "SD.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>
#include <RTClib.h>

// --- Pin ve Donanım Tanımları ---
const int PIN_POT       = 34;
const int PIN_CAN_RX    = 16;
const int PIN_CAN_TX    = 17;
const int PIN_SD_CS     = 5;
const int PIN_LED       = 2;

const uint8_t  CAN_SYNC      = 0xAA;
const uint16_t ID_OBD_TESTER = 0x07E0;
const uint16_t ID_OBD_ECU    = 0x07E8;
const char*    BLACKBOX      = "/TELEMETRY.CSV";

uint32_t canTxCount = 0, canRxCount = 0, canErrCount = 0;

// Nesneler
Adafruit_SSD1306 display(128, 64, &Wire, -1);
RTC_DS1307 rtc;
Adafruit_BMP085 bmp;

// Telemetri ve Simülasyon Değişkenleri
float gaz = 0, rpm = 800;
uint8_t hiz = 0, engT = 21, fuel = 78;
float kabinC = 24.0;
const char* dtc = "-";
bool overheatLatch = false;

// Zamanlayıcılar
uint32_t tQ = 0, tEkran = 0, tLog = 0;

// --- CAN Çerçeve Gönderme ---
void canSend(uint16_t id, const uint8_t* data, uint8_t len) {
  uint8_t sum = 0;
  Serial2.write(CAN_SYNC); sum += CAN_SYNC;
  uint8_t idH = id >> 8, idL = id & 0xFF;
  Serial2.write(idH); sum += idH;
  Serial2.write(idL); sum += idL;
  Serial2.write(len);  sum += len;
  for (uint8_t i = 0; i < len; i++) {
    Serial2.write(data[i]); sum += data[i];
  }
  Serial2.write((uint8_t)(0 - sum));
  canTxCount++;
}

// --- ECU Yanıt Üreteci ---
void ecuOnQuery() {
  uint8_t resp[8];
  uint16_t raw = (uint16_t)(rpm * 4);
  resp[0] = 0x04;  resp[1] = 0x41;  resp[2] = 0x0C;
  resp[3] = raw >> 8;  resp[4] = raw & 0xFF;
  resp[5] = engT;  resp[6] = fuel;  resp[7] = 0;
  canSend(ID_OBD_ECU, resp, 8);
}

// --- CAN Alım Durum Makinesi ---
enum CanRxState { CAN_SYNC_WAIT, CAN_IDH, CAN_IDL, CAN_DLC, CAN_DATA, CAN_CHK };
CanRxState canRx = CAN_SYNC_WAIT;
uint16_t   canIdBuf;
uint8_t    canDlcBuf, canDataBuf[8], canDataIdx, canSumBuf;

void canTick() {
  while (Serial2.available()) {
    uint8_t b = Serial2.read();
    switch (canRx) {
      case CAN_SYNC_WAIT:
        if (b == CAN_SYNC) { canSumBuf = b; canRx = CAN_IDH; }
        break;
      case CAN_IDH: canIdBuf  = (uint16_t)b << 8; canSumBuf += b; canRx = CAN_IDL; break;
      case CAN_IDL: canIdBuf |= b;               canSumBuf += b; canRx = CAN_DLC;  break;
      case CAN_DLC:
        canDlcBuf = b; canSumBuf += b;
        if (b <= 8) { canDataIdx = 0; canRx = CAN_DATA; }
        else        { canRx = CAN_SYNC_WAIT; }
        break;
      case CAN_DATA:
        canDataBuf[canDataIdx++] = b; canSumBuf += b;
        if (canDataIdx >= canDlcBuf) canRx = CAN_CHK;
        break;
      case CAN_CHK:
        canSumBuf += b;
        if (canSumBuf == 0) {
          canRxCount++;
          if (canIdBuf == ID_OBD_TESTER) ecuOnQuery();
          if (canIdBuf == ID_OBD_ECU) {
            engT = canDataBuf[5];
          }
        } else {
          canErrCount++;
        }
        canRx = CAN_SYNC_WAIT;
        break;
    }
  }
}

// --- Kara Kutu (SD) Fonksiyonları ---
void blackboxLog() {
  DateTime now = rtc.now();
  File f = SD.open(BLACKBOX, FILE_APPEND);
  if (!f) {
    Serial.println("[SD] Yazma hatası!");
    return;
  }
  f.printf("%04u-%02u-%02uT%02u:%02u:%02u,%u,%u,%u,%.1f,%u,%s\n",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second(),
           (unsigned)rpm, (unsigned)hiz, (unsigned)engT, kabinC, (unsigned)fuel, dtc);
  f.close();
}

void blackboxDump() {
  File f = SD.open(BLACKBOX, FILE_READ);
  if (!f) {
    Serial.println("[SD] Okuma hatası! Dosya bulunamadı.");
    return;
  }
  Serial.println("---- KARA KUTU ICERIGI (TELEMETRY.CSV) ----");
  while (f.available()) {
    Serial.write(f.read());
  }
  Serial.println("---- KUTU SONU ----");
  f.close();
}

// --- OLED Ekran Güncelleme ---
void drawDashboard() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(0, 0);
  display.printf("RPM:%4.0f HIZ:%3u", rpm, hiz);
  
  display.setCursor(0, 12);
  display.printf("MOT:%3uC KAB:%.1fC", engT, kabinC);

  display.setCursor(0, 24);
  display.printf("YAKIT:%3u%% DTC:%s", fuel, dtc);

  if (overheatLatch) {
    display.setCursor(0, 40);
    display.print("! UYARI: ASIRI ISINMA !");
  }
  display.display();
}

void setup() {
  Serial.begin(115200);
  while(!Serial);
  
  Serial2.begin(115200, SERIAL_8N1, PIN_CAN_RX, PIN_CAN_TX);
  pinMode(PIN_LED, OUTPUT);
  
  // I2C Başlatma (OLED, BMP180, DS1307)
  Wire.begin(21, 22);
  
  // OLED Başlatma
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[HATA] OLED bulunamadi!");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 20);
    display.print("OBD-II GATEWAY INIT");
    display.display();
  }

  // RTC Başlatma
  if (!rtc.begin()) {
    Serial.println("[HATA] DS1307 bulunamadi!");
  } else if (!rtc.isrunning() || rtc.now().year() < 2020) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // BMP180 Başlatma
  if (!bmp.begin()) {
    Serial.println("[HATA] BMP180 bulunamadi!");
  }

  // SPI ve SD Kart (Kara Kutu) Başlatma
  SPI.begin(18, 19, 23, PIN_SD_CS);
  Serial.println("[SD] SD Kart başlatılıyor...");
  
  if (!SD.begin(PIN_SD_CS)) {
    Serial.println("[OBD!] SD kart takılı değil veya bağlantı hatası! (CS=IO5?)");
  } else {
    Serial.println("[SD] Kart OK — kara kutu hazır.");
    if (!SD.exists(BLACKBOX)) {
      File f = SD.open(BLACKBOX, FILE_WRITE);
      if (f) {
        f.println("timestamp,rpm,hiz,engT,cabT,fuel,dtc");
        f.close();
        Serial.println("[SD] Başlık satırı yazıldı.");
      }
    }
  }
}

void loop() {
  uint32_t now = millis();
  
  // Seri Monitörden Komut Kontrolü (Örn: "dump" yazarak SD kartı oku)
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "dump") {
      blackboxDump();
    }
  }
  
  // Fizik Simülasyonu (Potansiyometre -> Gaz)
  gaz = analogRead(PIN_POT) * 100.0f / 4095.0f;
  rpm = 800 + gaz * 60;
  
  float hedefHiz = gaz * 1.8f;
  hiz += (hedefHiz - hiz) * 0.10f;
  
  float hedefT = 80 + (gaz / 100.0f) * 35.0f;
  engT += (hedefT - engT) * 0.02f;

  fuel = constrain(fuel - (gaz * 0.0005f), 0, 100);

  if (bmp.begin()) {
    kabinC = bmp.readTemperature();
  }

  // Arıza Durumu (DTC) Kontrolü
  if (engT > 105 && !overheatLatch) {
    overheatLatch = true;
    dtc = "P0217";
    Serial.println("[DTC] P0217 ASIRI MOTOR SICAKLIGI TESPIT EDILDI!");
  }
  digitalWrite(PIN_LED, (overheatLatch && engT > 105) ? HIGH : LOW);

  // CAN Hattı İşleme
  canTick();

  // OBD-II Sorgu Periyodu (200ms)
  if (now - tQ >= 200) {
    tQ = now;
    uint8_t q[8] = { 0x02, 0x01, 0x0C, 0, 0, 0, 0, 0 };
    canSend(ID_OBD_TESTER, q, 8);
  }

  // Ekran Güncelleme Periyodu (250ms)
  if (now - tEkran >= 250) {
    tEkran = now;
    drawDashboard();
  }

  // Log ve Telemetri Periyodu (1000ms)
  if (now - tLog >= 1000) {
    tLog = now;
    blackboxLog();
    Serial.printf("{\"rpm\":%u,\"hiz\":%u,\"engT\":%u,\"cabT\":%.1f,\"fuel\":%u,\"dtc\":\"%s\"}\n",
                  (unsigned)rpm, (unsigned)hiz, (unsigned)engT, kabinC, (unsigned)fuel, dtc);
  }

  delay(2);
}