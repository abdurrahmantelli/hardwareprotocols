# 🚗 Smart OBD-II Gateway & Telematics — Adım Adım Öğretici Plan (Wokwi)

> **Hedef:** Tek projede **UART, I2C, SPI ve CAN** protokollerini yaşayarak öğrenmek.
> Her adım aynı ritimde: **Ders → Devre → Kod → Test → Aklında Kalsın**
> Toplam süre: rahat 5-6 saat (adım başına 30-60 dk). Gereken: wokwi.com hesabı (ücretsiz).

---

## 📦 1. Klasördeki dosyalar

| Dosya | İşe yaradığı şey | Wokwi'de nereye? |
|---|---|---|
| `PLAN.md` | **Bu rehber** — adımları buradan takip et | okumalık |
| `sketch.ino` | Bitmiş projenin tam kodu (referans çözüm) | `sketch.ino` sekmesi |
| `diagram.json` | Bitmiş devre şeması | `diagram.json` sekmesi |
| `libraries.txt` | Gerekli kütüphane listesi | Library Manager (+) |

**İki çalışma yolu:**
- **YOL A (önerilen):** Adımları sırayla **sen** yaz/kur, her adım sonunda test et.
  Takıldığında `sketch.ino`'daki ilgili `[KATMAN]` yorum bölümüne bak — orada referans çözüm var.
- **YOL B (hızlı):** Her şeyi yapıştır, ▶ Play ile çalıştır, sonra kodu bölüm bölüm inceleyerek dersleri uygula.

---

## ⚡ 2. 5 dakikada çalıştırma (Yol B)

1. **wokwi.com** → giriş → **New Project** → **ESP32** şablonu
2. Sol alttaki **Library Manager** (+) → `libraries.txt` içeriğini satır satır ekle
3. `diagram.json` sekmesi → eski içeriği sil, dosyadakini yapıştır
4. `sketch.ino` sekmesi → kodu yapıştır
5. Yeşil **▶ Play** bas → seri monitörde saniyede bir JSON akar:
   `{"rpm":800,"vss":0,"engT":21,"cabT":24.0,"fuel":78,"dtc":"-","canRx":5,"canErr":0}`
6. **OLED** canlanır; **pot'u çevir** → RPM / hız / ısı / yakıt değişir
7. Seri monitöre `help` yaz → konsol komutları: `read`, `err`, `dtc`, `clear`, `i2cscan`
8. Pot'u **%70 ve üzeri** ~15 sn tut → LED yanar, **P0217** aşırı ısınma DTC'si kara kutuya yazılır!

---

## 🏗️ 3. Mimari — tek kartta 3 katman

```
        [ Gaz Kolu (pot) ]
               │ ADC (IO34)
               ▼
     ┌─────────────────────┐
     │ 1) ECU SİMÜLATÖRÜ    │ motor fiziği: RPM, hız, ısı, yakıt
     └─────────┬───────────┘
               │ OBD-II sorgusuna yanıt çerçevesi (ID 0x07E8, 8 bayt)
               ▼
    ═══ SANAL CAN HATTI ═══  ← UART2'nin FİZİKSEL teli: IO17(TX) ──▶ IO16(RX)
               │                (gerçek tel! logic analyzer'da sinyali görünür)
               ▼
     ┌─────────────────────┐
     │ 2) CAN DENETLEYİCİ   │ çerçeveyi bayt bayt ayrıştır + checksum doğrula
     │    (MCP2515 rolü)    │ (senkron → ID → uzunluk → veri → sağlama)
     └─────────┬───────────┘
               ▼
     ┌─────────────────────┐   I2C (2 tel!): OLED 0x3C + DS1307 0x68 + BMP180 0x77
     │ 3) GATEWAY          │   SPI (4 tel): microSD kara kutu /TELEMETRY.CSV
     │                      │   UART0: PC'ye JSON telemetri + komut konsolu
     └─────────┬───────────┘
               ▼ [LED IO2] motor >105°C → DTC P0217!
```

**Neden böyle?** Wokwi'de tek projede birden fazla kart desteklenmez ve gerçek CAN
donanım simülasyonu yoktur. Bu yüzden ECU + CAN denetleyici + gateway, aynı kartın
**yazılımdaki üç ayrı modülü**; CAN hattı ise UART2'nin TX ucundan RX ucuna çekilmiş
**gerçek fiziksel tel**dir. Kod da bu disiplini korur: ECU ile gateway birbirini
ASLA doğrudan görmez — ikisi yalnızca CAN çerçeveleriyle konuşur.

---

## 🧰 4. Parça listesi ve pin haritası

**Parça listesi** (Wokwi "+"/çıkış menüsünden ya da diagram.json'dan eklenir):

| Parça | Wokwi parça tipi | id | Protokol / adres |
|---|---|---|---|
| ESP32 DevKit v1 | `wokwi-esp32-devkit-v1` | `esp` | — |
| OLED 128x64 (SSD1306) | `board-ssd1306` | `oled` | I2C **0x3C** |
| BMP180 sıcaklık+basınç | `board-bmp180` | `bmp1` | I2C **0x77** |
| DS1307 gerçek zaman saati | `wokwi-ds1307` | `rtc1` | I2C **0x68** |
| microSD kart | `wokwi-microsd-card` | `sd1` | SPI |
| Potansiyometre (gaz kolu) | `wokwi-potentiometer` | `pot1` | analog |
| Kırmızı LED + 220Ω direnç | `wokwi-led` + `wokwi-resistor` | `led1`, `r1` | GPIO |
| *(bonus)* Logic analyzer | `wokwi-logic-analyzer` | `logic1` | pasif dinleyici |

**Pin haritası** (tüm adımlar bu tek tabloyu kullanır — ezber gerekmez, buradan bak):

| Görev | Protokol | ESP32 pini | Karşı uç |
|---|---|---|---|
| PC telemetri + konsol | UART0 | dahili USB (TX0/RX0) | Seri monitör |
| **Sanal CAN kablosu** | UART2 | **IO17 (TX2)** ─tel─▶ **IO16 (RX2)** | kendi RX ucuna |
| I2C veri hattı | I2C | **IO21 (SDA)** | oled+bmp1+rtc1 → üçü de paralel |
| I2C saat hattı | I2C | **IO22 (SCL)** | oled+bmp1+rtc1 → üçü de paralel |
| SD kara kutu | SPI | **IO23 (MOSI)**→DI, **IO19 (MISO)**←DO, **IO18 (SCK)**→SCK, **IO5 (CS)**→CS | sd1 |
| Gaz kolu | ADC | **IO34** ← SIG | pot1 |
| Uyarı LED'i | GPIO | **IO2** → 220Ω → LED(A)…LED(C) → GND | led1, r1 |
| Güç | — | **3V3**: oled.VCC, bmp1.VCC, sd1.VCC, pot1.VCC · **VIN(5V)**: rtc1.5V | — |
| Toprak | — | GND.1 ve GND.2'ye dağıt | — |

⚠️ **Wokwi notları:**
- Pin adları harfi harfine bunlardır: `esp:IO21`, `sd1:SCK` (CLK değil!), `rtc1:5V` (VCC değil!), `bmp1:VCC` (VIN değil!). Emin olamazsan parçaya tıkla, pin etiketlerini oku.
- Kabloyu çizmek: bir pinin üstüne tıkla → hedef pinin üstüne tıkla. Bitirdiğin kablo görünmesin istersen sağ tık → sil.
- DS1307 5V'luk bir modüldür → ESP32'de **VIN** ucuna (5V) bağlanır. Gerçek hayatta 5V'lu modülle 3.3V'lu ESP32 I2C hattı arasında seviye uyumu gerekir — simülasyonda sorun değil, ders defterine not düş!

## 📚 5. Kütüphaneler (libraries.txt)

Wokwi'de sol alt **Library Manager** (+) içine satır satır gir:

```
Adafruit GFX Library
Adafruit BusIO
Adafruit SSD1306
Adafruit Unified Sensor
Adafruit BMP085 Library
RTClib
```

*(Wire, SPI, SD kütüphaneleri ESP32 çekirdeğinde gömülüdür — listeye gerek yok.
"Adafruit BMP085" adı sizi şaşırtmasın: BMP180, BMP085'in aynı aileden kardeşidir.)*

---

# 🪜 ADIM ADIM YOL — her adımı test etmeden sonrakine geçme!

## Adım 0 — Proje ısınması (~20 dk)

- wokwi.com → **New Project → ESP32**. Açılan şablon zaten blink'dir.
- `sketch.ino` altına **Library Manager**'dan yukarıdaki 6 kütüphaneyi ekle.
- **Test:** ▶ Play → kart resmindeki kırmızı LED yanıp söner (yaklaşık 1 sn aralıkla).
- **Aklında kalsın:** Blink'in kullandığı `IO2`, kartın **dahili LED** pidir — projede tam da bu pini harici uyarı LED'i olarak kullanacağız.

## Adım 1 — UART: telemetri hattı (~40 dk)

**Ders:** UART = **2 tel (TX/RX), asenkron**: arada saat hattı YOK! İki tarafın
hızda (baud) anlaşması şart; veri start/stop bitleriyle sarılır. Adres kavramı
yok → bu hat **yalnız iki cihaz** arasında çalışır.

**Devre:** Henüz gerek yok — UART0 kartın USB'sinden zaten dışarı çıkar.

**Kod** (tüm sketch'i bununla değiştir):

```cpp
void setup() {
  Serial.begin(115200);              // baud: sn başına bit — iki taraf aynı olmalı!
}
void loop() {
  Serial.println("{\"rpm\":800,\"engT\":21,\"fuel\":78}");   // ham telemetri
  delay(1000);
}
```

**Test:**
1. Monitörde saniyede bir JSON satırı akmıyor mu?
2. Şimdi `Serial.begin(9600)` yapıp ▶ yeniden başlat (monitör 115200'de kalsın)
   → **çöp karakterler** gör. İşte "iki taraf baud'da anlaşmazsa" dersinin canlı kanıtı!
3. Geri düzelt: `115200`.

**Aklında kalsın:** *UART iki kişilik telefon hattıdır — üçüncü biri giremez.
Arabada 40+ modül varsa ne olacak? CAN tam bu sorundan doğdu!*

## Adım 2 — Sanal CAN'ın omurgası: UART2 + gaz kolu (~45 dk)

**Ders:** ESP32'de **3 donanım UART** var. UART0 PC'de meşgul → araç hattına
**UART2**'yi (IO16/IO17) ayırıyoruz. "Loopback" (kendi TX'in→RX'ine) gerçek
hayatta kullanılan bir **donanım test tekniğidir**. İkinci ders: ADC — pot
değeri 0–4095 arasıdır (12-bit).

**Devre** (4 kablo çiz):
1. `pot1:SIG` → `esp:IO34`
2. `pot1:VCC` → `esp:3V3`
3. `pot1:GND` → `esp:GND.1`
4. **`esp:IO17` → `esp:IO16`** ← işte "CAN kablomuz"! (IO17'ye tıkla, IO16'ya tıkla)

**Kod** (tüm sketch'i bununla değiştir):

```cpp
const int PIN_POT    = 34;   // gaz kolu (giriş-özellikli pin)
const int PIN_CAN_RX = 16;   // UART2 RX
const int PIN_CAN_TX = 17;   // UART2 TX

void setup() {
  Serial.begin(115200);                                          // UART0: PC tarafı
  Serial2.begin(115200, SERIAL_8N1, PIN_CAN_RX, PIN_CAN_TX);      // UART2: araç hattı
}

void loop() {
  // --- "ECU" tohumu: gaz kolunu oku, motor değeri üret ---
  int   ham = analogRead(PIN_POT);           // 0..4095 (12-bit ADC)
  float gaz = ham * 100.0f / 4095.0f;        // % gaz
  float rpm = 800 + gaz * 60;                // rölanti 800, tam gaz ~6800

  // --- "hatta" ham satır yaz ---
  Serial2.printf("RPM:%.0f,GAZ:%.0f\n", rpm, gaz);

  // --- telden geri geleni oku ---
  while (Serial2.available()) {
    Serial.print("HATTAN GELEN: ");
    Serial.write(Serial2.read());
  }
  delay(200);
}
```

**Test:**
1. Pot'a bir kez tıkla → **sağ/sol ok tuşlarıyla çevirebilirsin** (Wokwi özelliği).
2. Monitörde `HATTAN GELEN: RPM:3240,GAZ:40` gibi satırlar akmıyor mu?
3. Veri IO17'den çıktı, **fiziksel telden geçti**, IO16'dan geri girdi — bunu
   gözlerinle gördün: artık gerçek bir "veriyolu"nun var!

**Aklında kalsın:** *Adım 1'de bu veri çıplak UART metniydi. Adım 3'te tam da
bu veriyi CAN gibi **çerçeveyle saracağız**. Protokol farkı = veriyi paketleme biçimi farkı.*

## Adım 3 — CAN çerçeve katmanı: projenin kalbi (~60 dk) ⭐

**Ders:** Gerçek CAN 2.0 çerçevesi şöyle görünür:

```
SOF │ 11-bit ID │ RTR │ Kontrol │ VERİ (0-8 bayt) │ CRC-15 │ ACK │ EOF
```

- **ID = adres değil, öncelik/etiket!** Hattaki herkes her çerçeveyi duyar
  (**multicast**); çakışmada **en düşük ID önce konuşur** (arbitration).
- **0x07E0 ve 0x07E8 gerçek OBD-II ID'leridir:** test cihazı sorguyu 0x07E0'a
  yollar, ECU cevabı 0x07E8'den geri verir. Aynı kuralı biz de kullanacağız!
- Wokwi'de CAN donanımı yok → bizim yazdığımız katman = "MCP2515'in işi",
  UART2 teli = "CAN kablosu". Gerçek hayatta bu katmanın altına yalnızca
  donanım sürücüsü eklenir, mantık aynı kalır.

**Bizim (basitleştirilmiş) çerçeve biçimimiz:**

```
[0xAA senkron] [ID üst] [ID alt] [DLC = 0-8] [D0..D7] [CHECKSUM]
```

- **DLC** = veri uzunluğu (Data Length Code). CAN çerçevesi en fazla 8 bayt taşır.
- **CHECKSUM** = kendisinden önceki tüm baytların toplamının **2'li tümleyeni**.
  Sağlam çerçevede tüm baytların toplamı (checksum dahil) **0** olur.
  Mantık UDP sağlamasının aynısıdır: en küçük bozulma bile kendini ele verir.
  *(Gerçek CAN'da 15 bitlik CRC-15 kullanılır — Adım 7'de ona da değineceğiz.)*

**Kod — Adım 2'deki sketch'in başına (pin sabitlerinin yanına) ekle:**

```cpp
const uint8_t  CAN_SYNC      = 0xAA;    // senkron baytı
const uint16_t ID_OBD_TESTER = 0x07E0;  // sorgu ID'si (test cihazı)
const uint16_t ID_OBD_ECU    = 0x07E8;  // yanıt ID'si (ECU)
uint32_t canTxCount = 0, canRxCount = 0, canErrCount = 0;   // hat sağlığı
```

**Kod — çerçeve gönderme fonksiyonunu ekle:**

```cpp
// Çerçeveyi kurar ve UART2'ye (yani "CAN kablomuza") yazar.
void canSend(uint16_t id, const uint8_t* data, uint8_t len) {
  uint8_t sum = 0;
  Serial2.write(CAN_SYNC); sum += CAN_SYNC;         // 1) senkron
  uint8_t idH = id >> 8, idL = id & 0xFF;
  Serial2.write(idH); sum += idH;                    // 2) ID üst baytı
  Serial2.write(idL); sum += idL;                    // 3) ID alt baytı
  Serial2.write(len);  sum += len;                   // 4) uzunluk (DLC)
  for (uint8_t i = 0; i < len; i++) {                // 5) veri baytları
    Serial2.write(data[i]); sum += data[i];
  }
  Serial2.write((uint8_t)(0 - sum));                  // 6) 2'li tümleyen
  canTxCount++;
}
```

*Kontrol: senkron+ID+DLC+veri+checksum toplamı 256'ya tam bölünür → toplam 0.*

**Kod — bayt bayt ayrıştırma durum makinesi** (gerçek denetleyici hattı böyle okur):

```cpp
enum CanRxState { CAN_SYNC_WAIT, CAN_IDH, CAN_IDL, CAN_DLC, CAN_DATA, CAN_CHK };
CanRxState canRx = CAN_SYNC_WAIT;
uint16_t   canIdBuf;
uint8_t    canDlcBuf, canDataBuf[8], canDataIdx, canSumBuf;

void canTick() {                       // her loop turunda çağrılacak
  while (Serial2.available()) {
    uint8_t b = Serial2.read();
    switch (canRx) {
      case CAN_SYNC_WAIT:                       // 0) senkron baytı bekle
        if (b == CAN_SYNC) { canSumBuf = b; canRx = CAN_IDH; }
        break;                                 // değilse: çöptür, sessiz kal
      case CAN_IDH: canIdBuf  = (uint16_t)b << 8; canSumBuf += b; canRx = CAN_IDL; break;
      case CAN_IDL: canIdBuf |= b;               canSumBuf += b; canRx = CAN_DLC;  break;
      case CAN_DLC:                            // uzunluk: CAN'da en fazla 8!
        canDlcBuf = b; canSumBuf += b;
        if (b <= 8) { canDataIdx = 0; canRx = CAN_DATA; }
        else          { canRx = CAN_SYNC_WAIT; }   // imkansız uzunluk → çöp
        break;
      case CAN_DATA:                            // veriyi topla
        canDataBuf[canDataIdx++] = b; canSumBuf += b;
        if (canDataIdx >= canDlcBuf) canRx = CAN_CHK;
        break;
      case CAN_CHK:                             // sağlama: toplam 0 mı?
        canSumBuf += b;
        if (canSumBuf == 0) {
          canRxCount++;
          // MULTICAST: hattı herkes dinler, işi olan ID'yi "yakalar":
          if (canIdBuf == ID_OBD_TESTER) ecuOnQuery();      // ECU: soruyu cevapla
          if (canIdBuf == ID_OBD_ECU) {                     // Gateway: veriyi al
            Serial.printf("[CAN RX] ID=0x%04X  %02X %02X %02X %02X %02X %02X %02X %02X\n",
                          (unsigned)canIdBuf, canDataBuf[0], canDataBuf[1],
                          canDataBuf[2], canDataBuf[3], canDataBuf[4],
                          canDataBuf[5], canDataBuf[6], canDataBuf[7]);
          }
        } else {           // BOZUK ÇERÇEVE → reddedilir, sayaç artar
          canErrCount++;
          Serial.printf("[CAN] CHECKSUM HATASI (ID=0x%04X) -> cerceve atildi\n",
                        (unsigned)canIdBuf);
        }
        canRx = CAN_SYNC_WAIT;
        break;
    }
  }
}
```

**Kod — ECU yanıtı + ana döngü** (eski `loop()` ve satır yazan kodu sil, `rpm`/`gaz` değişkenlerini dosyanın başına taşı):

```cpp
float gaz = 0, rpm = 800;
uint32_t tQ = 0;

void ecuOnQuery() {                     // "Mode 01 PID 0C" (RPM) soruldu:
  uint8_t resp[8];
  uint16_t raw = (uint16_t)(rpm * 4);   // OBD-II GERÇEK kuralı: RPM = raw/4
  resp[0] = 0x04;  resp[1] = 0x41;  resp[2] = 0x0C;   // OBD yanıt başlığı
  resp[3] = raw >> 8;  resp[4] = raw & 0xFF;         // RPM (2 bayt)
  resp[5] = 0; resp[6] = 0; resp[7] = 0;             // sonraki adımlarda dolacak
  canSend(ID_OBD_ECU, resp, 8);
}

void loop() {
  gaz = analogRead(PIN_POT) * 100.0f / 4095.0f;
  rpm = 800 + gaz * 60;

  canTick();                            // hattı asla aç bırakma!

  if (millis() - tQ >= 200) {           // 200 ms'de bir OBD-II sorgusu
    tQ = millis();
    uint8_t q[8] = { 0x02, 0x01, 0x0C, 0, 0, 0, 0, 0 };  // "RPM nedir?"
    canSend(ID_OBD_TESTER, q, 8);
  }
  delay(2);
}
```

**Test:**
1. Monitörde 200 ms'de bir `[CAN RX] ID=0x07E8  04 41 0C 08 20 ...` akmalı.
   Pot çevir → `resp[3]`/`resp[4]` (RPM baytları) değişir.
2. **Sağlamlık dersi:** `loop()` başına şunu ekle, 5 sn sonra hattı bile bile boz:

```cpp
  static bool bozukGitti = false;
  if (!bozukGitti && millis() > 5000) {
    bozukGitti = true;
    uint8_t bad[] = { 0xAA, 0x07, 0xE8, 0x08, 1, 2, 3, 4, 5, 6, 7, 8, 0x00 };
    Serial2.write(bad, sizeof(bad));     // sağlaması bilerek yanlış!
    Serial.println("[TEST] Bilerek bozuk cerceve gonderildi");
  }
```
   → `[CAN] CHECKSUM HATASI ... cerceve atildi` satırını görmelisin.
   Gerçek CAN'daki hata çerçeveleri (error frames) ve yeniden gönderim mantığı
   tam olarak bunu donanımda yapar.

**Aklında kalsın:** *Artık veri "sokakta yalnız" değil: kimliği (ID), uzunluğu
(DLC) ve sağlaması (checksum) var. Sorgu-cevap usulü de gerçek OBD-II'nin
tarzı: 0x07E0'a sor → 0x07E8'den du.*

## Adım 4 — I2C: gösterge, saat ve kabin sensörü (~60 dk)

**Ders:** I2C = **2 tel (SDA/SCL), 7-bit ADRESLİ, master→slave**. Master kime
sesleneceğini **adresle** söyler, o adresin sahibi **ACK** sinyaliyle "buradayım"
der. Bu yüzden aynı iki tele **çok sayıda cihaz** bağlanabilir. Ayrıca hatlar
**open-drain**'dir: cihazlar hattı "0'a çeker", asla beslemez — çarpışma zararsızdır.

**Devre** (11 kablo — üç cihaz da AYNI iki tele paralel):

| Cihaz | SDA | SCL | Besleme | GND |
|---|---|---|---|---|
| oled | → esp:IO21 | → esp:IO22 | VCC → esp:3V3 | → esp:GND.1 |
| bmp1 | → esp:IO21 | → esp:IO22 | VCC → esp:3V3 | → esp:GND.1 |
| rtc1 | → esp:IO21 | → esp:IO22 | **5V → esp:VIN** | → esp:GND.2 |

**Kod ① — önce I2C tarayıcı** (adres dersinin canlı kanıtı — bu fotoğraflık bir an!):

```cpp
void i2cScan() {
  Serial.println("[I2C] Hat taraniyor...");
  uint8_t n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {          // 0 = cihaz ACK'ladı!
      Serial.printf("  -> 0x%02X adresinde cihaz var\n", a);
      n++;
    }
  }
  Serial.printf("[I2C] %u cihaz bulundu (beklenen: 0x3C, 0x68, 0x77)\n", n);
}
```

`setup()` başına ekle: `Wire.begin(21, 22);` (pinleri açıkça yaz — ezber gerekmesin) ve `i2cScan();` çağır.

**Test ①:** Monitörde üç adres: **0x3C** (OLED), **0x68** (DS1307), **0x77** (BMP180).
Üç cihaz, iki tel — adres sayesinde! *(Wokwi'de "rc1" yazarsan bulamaz: adresler hex.)*

**Kod ② — OLED göstergesi** (dosya başına include + nesne, setup()'a başlatma):

```cpp
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// setup() içinde:
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[OBD!] OLED bulunamadi (0x3C?)");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 28);
    display.print("OBD-II GATEWAY");
    display.display();   // UNUTMA: display() çağrılmadan ekranda hiçbir şey görünmez!
  }
```

⚠️ OLED'in Adafruit fontu **CP437**'dir: Türkçe harfler (İ, ş, ğ…) bozuk görünür →
ekran metinlerinde ASCII kullan ("ASIRI ISINMA" gibi). Bu da başlı başına bir kod sayfası dersi!

**Kod ③ — DS1307 gerçek zaman saati:**

```cpp
#include <RTClib.h>
RTC_DS1307 rtc;

// setup() içinde:
  if (!rtc.begin()) Serial.println("[OBD!] DS1307 bulunamadi (0x68?)");
  else if (!rtc.isrunning() || rtc.now().year() < 2020) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));   // cihaz saatini kur
  }

// 1 sn'de bir okuma örneği:
  DateTime now = rtc.now();
  Serial.printf("%02u:%02u:%02u\n", now.hour(), now.minute(), now.second());
```
*(Wokwi'de DS1307 simülasyon başlangıcında gerçek sistem saatini alır ve gerçek gibi işler.)*

**Kod ④ — BMP180 kabin sıcaklığı/basınç:**

```cpp
#include <Adafruit_Sensor.h>     // BMP085 kütüphanesinin şartı
#include <Adafruit_BMP085.h>     // BMP180 = BMP085 ailesi
Adafruit_BMP085 bmp;

// setup() içinde:
  if (!bmp.begin()) Serial.println("[OBD!] BMP180 bulunamadi (0x77?)");

// okuma:
  float   kabinC = bmp.readTemperature();
  int32_t pa     = bmp.readPressure();      // Pa cinsinden (101325 ≈ 1 atm)
```

**Test:**
1. Tarama ③ adresi listeliyor mu? 2. OLED'de açılış yazısı var mı?
3. **BMP180 parçasına tıkla → açılan pencerede sıcaklığı 30°C yap** →
   okunan değer değişiyor mu? (Canlı sensör etkileşimi!)
4. Saat saniyeleri gerçek zamanla ilerliyor mu?

**Aklında kalsın:** *I2C'de adres yanlışsa cihaz "sessizce kaybolur" — ACK gelmez.
SPI'da ise cihaz seçme telini (CS) unutursan ne olur? Sıradaki adımın konusu tam bu!*

## Adım 5 — SPI: kara kutu (SD kart) (~45 dk)

**Ders:** SPI = **4 hat: MOSI / MISO / SCK / CS** — ve burada "adres" YOKTUR:
**CS teli = donanım adresidir.** Cihazla konuşmak için CS'yi **LOW** çekersin;
CS HIGH iken cihaz MISO'yu bile bırakır (high-impedance) → başka cihaz konuşabilir.
Tam çift yönlü ve en hızlı protokoldür (onlarca MHz'e kadar). SD kart SPI'nin klasik öğrencisidir.

**Devre** (6 kablo):

```
sd1:DI  ← esp:IO23   (MOSI: master out → slave in)
sd1:DO  → esp:IO19   (MISO: master in ← slave out)
sd1:SCK ← esp:IO18   (saat: her darbe bir bit)
sd1:CS  ← esp:IO5    (cihaz seçimi: LOW = "şimdi sana konuşuyorum")
sd1:VCC → esp:3V3
sd1:GND → esp:GND.1
```

**Kod — dosya başına:**

```cpp
#include "FS.h"
#include "SD.h"
#include "SPI.h"
const int PIN_SD_CS = 5;
const char* BLACKBOX = "/TELEMETRY.CSV";
```

**Kod — setup() içine:**

```cpp
  SPI.begin(18, 19, 23, 5);                     // SCK, MISO, MOSI, CS — sıraya dikkat!
  if (!SD.begin(PIN_SD_CS)) {
    Serial.println("[OBD!] SD kart takili degil! (CS=IO5?)");
  } else {
    Serial.println("[SD] Kart OK — kara kutu hazir");
    if (!SD.exists(BLACKBOX)) {                 // başlık satırı (CSV standardı)
      File f = SD.open(BLACKBOX, FILE_WRITE);
      if (f) { f.println("timestamp,rpm,hiz,engT,cabT,fuel,dtc"); f.close(); }
    }
  }
```

**Kod — kayıt fonksiyonu** (her çağrıda bir satır ekler):

```cpp
void blackboxLog(uint16_t rpmV, uint8_t hizV, uint8_t engTV,
                 float cabTV, uint8_t fuelV, const char* dtcV) {
  DateTime now = rtc.now();
  File f = SD.open(BLACKBOX, FILE_APPEND);
  if (!f) { Serial.println("[SD] Yazma hatasi!"); return; }
  f.printf("%04u-%02u-%02uT%02u:%02u:%02u,%u,%u,%u,%.1f,%u,%s\n",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second(),
           rpmV, hizV, engTV, cabTV, fuelV, dtcV);
  f.close();
}
```

**Kod — geri okuma** (kanıtı göster!):

```cpp
void blackboxDump() {
  File f = SD.open(BLACKBOX);
  if (!f) { Serial.println("[SD] Okuma hatasi!"); return; }
  Serial.println("---- KARA KUTU ICERIGI ----");
  while (f.available()) Serial.write(f.read());
  Serial.println("---- SON ----");
  f.close();
}
```

**Test:**
1. `setup()` sonunda 1 kez `blackboxLog(3200, 90, 92, 24.5, 63, "-");` çağır → sonra `blackboxDump();`
   → monitörde başlık + satırı gör.
2. Wokwi'de SD simde yavaş olabilir → kayıtları **1 sn'de 1'e** sınırla + `loop()` başına `delay(2)`.

**Aklında kalsın:** *SPI hız canavarı ama tel manyağı — her cihaz +1 CS teli ister.
I2C ise 2 teli yüzlerce cihazla paylaşır. Mühendislik = ödünleşim: hız ↔ sadelik.
Sıradaki adım: tüm bu protokolleri TEK akışta konuşturma zamanı!*

## Adım 6 — Entegrasyon: Gateway ana akışı (~60 dk)

**Ders:** `millis()` tabanlı **delay'siz çok-görevli döngü** — gömülü sistemlerin
gerçek hayatı. `delay()` kullanırsan o sürede ne CAN dinlenebilir ne sensör okunur!
Ayrıca gerçek araçlardaki **DTC (Diagnostic Trouble Code)** mantığı: harfler
**P=Powertrain, B=Body, C=Chassis, U=Network** — bizim `P0217` = "aşırı motor sıcaklığı".

**Devre tamamı** (son 3 kablo): `esp:IO2 → r1(220Ω) → led1:A`, `led1:C → esp:GND.2`.

**Ana döngü şablonu** (tüm parçalar artık bu iskelette çalışır):

```cpp
uint32_t tQ = 0, tEkran = 0, tLog = 0;          // her görevin kendi saati

void loop() {
  uint32_t now = millis();
  gaz = analogRead(PIN_POT) * 100.0f / 4095.0f;
  rpm = 800 + gaz * 60;
  canTick();                                     // 10 ms'den sık: hattı aç bırakma!

  if (now - tQ >= 200) {                         // OBD-II sorgusu
    tQ = now;
    uint8_t q[8] = { 0x02, 0x01, 0x0C, 0, 0, 0, 0, 0 };
    canSend(ID_OBD_TESTER, q, 8);
  }

  if (now - tEkran >= 250) {                     // OLED yenileme
    tEkran = now;
    drawDashboard();                              // Adım 4 kodlarını burada topla
  }

  if (now - tLog >= 1000) {                      // kara kutu + telemetri
    tLog = now;
    blackboxLog(rpm, hiz, engT, kabinC, fuel, dtc);
    Serial.printf("{\"rpm\":%u,\"hiz\":%u,\"engT\":%u,\"cabT\":%.1f,\"fuel\":%u,\"dtc\":\"%s\"}\n",
                  (unsigned)rpm, (unsigned)hiz, (unsigned)engT, kabinC, (unsigned)fuel, dtc);
  }
  delay(2);
}
```

**DTC / uyarı mantığı** (global alana):

```cpp
uint8_t hiz = 0, engT = 21, fuel = 78;
float   kabinC = 24;
const char* dtc = "-";
bool    overheatLatch = false;                   // kara kutu gerçeği: DTC silinmez!

// ECU yanıtını işlerken:
  engT = ...;                                    // CAN'dan gelen motor sıcaklığı
  if (engT > 105 && !overheatLatch) {
    overheatLatch = true;
    dtc = "P0217";                               // gerçek Powertrain DTC'si!
    Serial.println("[DTC] P0217 ASIRI MOTOR SICAKLIGI");
  }
  digitalWrite(PIN_LED, (overheatLatch && engT > 105) ? HIGH : LOW);
```

**Simülasyon fizikleri** (yumuşatma — gerçek sensör gibi davransın):

```cpp
  // Hız: gaza basınca artar, bırakınca yavaş düşer (zıplama yok)
  float hedefHiz = gaz * 1.8f;                    // max ~180 km/h
  hiz += (hedefHiz - hiz) * 0.10f;                // filtre (LPF)
  // Sıcaklık: gaza basınca ısınır, doğaya yayılır
  float hedefT = 80 + (gaz / 100.0f) * 35.0f;      // 80..115°C bandı
  engT += (hedefT - engT) * 0.02f;                // yavaş ısınma
  // Yakıt: gazın %'sine göre azalır (bir simülasyon turu ~ birkaç saat sürer)
  fuel = constrain(fuel - gaz * 0.0005f, 0, 100);
```

**Test:**
1. Pot'u sonuna kadar aç → ~10 sn bekle → engT tırmanır (20→105+) →
   **LED yanar**, OLED'de `⚠ ASIRI ISINMA`, JSON'da `"dtc":"P0217"`.
2. Pot'u geri çek → LED söner ama **DTC kayıtta kalır** — kara kutu gerçeği!
3. 60 sn çalıştır, durdur → `blackboxDump()` ile CSV'yi doğrula.

**Aklında kalsın:** *Gerçek ECU'lar da tıpkı böyle "latch" (kilitli) DTC tutar:
arıza geçse bile kayıt, silinme komutuna kadar durur. Uçak kara kutuları gibi!*

## Adım 7 — Bonus derinleşmeler (istediğin kadar)

**a) Sinyal dedektörü (logic analyzer)** — protokolleri **gözle gör** (~30 dk):
Wokwi'de `wokwi-logic-analyzer` parçasını ekle ve kabloları çek:
`D0←esp:IO22 (SCL)`, `D1←esp:IO21 (SDA)`, `D2←esp:IO18 (SCK)`, `D3←esp:IO23 (MOSI)`,
`D4←esp:IO5 (CS)`, `D5←esp:IO17 (TX2)`. Sim biraz çalışsın → **dur** →
sağ üstten `wokwi-logic.vcd` dosyası iner → ücretsiz **PulseView** (sigrok) ile aç.
Göreceksin: I2C'de adres baytından gelen **ACK çentiği**, SPI'da **CS'nin paketi
sarması**, UART'taki **start biti**. "Protokol farkı" artık soyut değil — dalga formu!

**b) Daha gerçek OBD-II:** ECU yanıtına daha çok PID ekle: `0x05` (motor sıcaklığı:
`değer−40`), `0x2F` (yakıt seviyesi). Sorguyu da döngüsel yap: her seferinde başka PID sor.

**c) Gerçek CAN CRC-15:** checksum yerine CAN'ın polinomu `0x4599` ile
CRC-15 hesapla (15 satırlık tablo yöntemi) — gerçek MCP2515'in yaptığı budur.

**d) Gerçek donanıma geçiş notu:** `canSend/canTick` katmanı **aynen kalır**;
yalnız altına gerçek donanım konur: **MCP2515** (SPI üzerinden konuşulan CAN
denetleyicisi) + **TJA1050** transceiver. SPI dersi burada devreye girer —
MCP2515'e de SD kart gibi **CS teliyle** ("0") seçilerek komut yazılır!

---

# 📏 6. Final karşılaştırma çetveli (projeden sonra elinde kalsın)

| | **UART** | **I2C** | **SPI** | **CAN** |
|---|---|---|---|---|
| Hat sayısı | 2 (TX/RX) | 2 (SDA/SCL) | 4 + her cihaza CS | 2 (CAN-H/CAN-L, diferansiyel) |
| Cihaz kimliği | — (nokta-nokta) | 7-bit **adres** | **CS teli** | 11/29-bit **ID** (öncelik!) |
| Yönetim | serbest | master→slave | master→slave | **multi-master** (arbitration) |
| Saat? | yok (asenkron) | var (SCL) | var (SCK, daha hızlı) | yok (hatta senkron) |
| Hız | orta | yavaş-orta | **en hızlı** | orta + çok sağlam |
| Hata kontrolü | parity (ops.) | ACK biti | — (kendi düzeninde) | **CRC-15 + ACK + tekrar gönderim** |
| Bu projede | PC JSON + sanal CAN kablosu | OLED/RTC/BMP180 | SD kara kutu | araç omurgası (yazılımsal) |

**Mnemonic:** *UART iki kişilik telefon · I2C adresle çağrılan toplantı ·
SPI hızlı ama tel manyağı · CAN mahallenin afiş panosu — herkes dinler,
en önemli (düşük ID) önce konuşur.*

---

# 🔧 7. Sorun giderme tablosu

| Belirti | Olası neden | Çöz |
|---|---|---|
| Monitörde çöp karakterler | baud uyuşmazlığı | her iki tarafı da 115200 yap |
| I2C tarama boş | SDA/SCL ters ya da GND yok | IO21/IO22 ve GND bağlantılarını tek tek say |
| `0x3C` yok, OLED çalışmıyor | kütüphane eksik ya da `display.display()` unutuldu | Library Manager'ı kontrol et; kodda `display.display()` çağrıldı mı? |
| CAN'dan hiç yanıt yok | IO17→IO16 teli yok | loopback telini yeniden çiz |
| `CHECKSUM HATASI` her çerçevede | `canSend` ile alıcının formatı farklı | iki tarafın da aynı `CAN_SYNC`/biçimi kullandığından emin ol |
| SD başlatılamadı | CS pininden `SD.begin`'e farklı pin | `SPI.begin(18,19,23,5)` + `SD.begin(5)` — ikisi de IO5! |
| SD yazma çok yavaş | Wokwi sim hız sınırı | 1 sn'de bir kaydet, `delay(2)` bırak |
| OLED'de Türkçe harf bozuk | CP437 fontu | ASCII metin kullan |
| Her şey donuyor | `while` döngüsü + `delay()` fazlalığı | `canTick()` her turda, `delay(2)`'den fazlası yok |

---

# ✅ 8. Kilometre taşları

- [ ] Adım 0: blink çalışıyor, 6 kütüphane eklendi
- [ ] Adım 1: JSON akıyor + baud bozma deneyi yapıldı
- [ ] Adım 2: pot çevirince "HATTAN GELEN" satırları akıyor
- [ ] Adım 3: `[CAN RX] ID=0x07E8` + bilerek bozulan çerçeve reddedildi
- [ ] Adım 4: tarayıcı 3 adres buldu; OLED canlı; BMP180 slider'ı ekrana yansıyor
- [ ] Adım 5: CSV'de timestamp'li satır var ve geri okundu
- [ ] Adım 6: tam gaz → LED + P0217; kara kutuda DTC kaydı kalıcı
- [ ] Adım 7 (bonus): PulseView'da I2C/SPI/UART dalga formları görüldü

---

# 🎓 9. Final mini sınav (10 soru — cevapları sormana gerek yok, kendi kendine)

1. UART'ta saat hattı olmadığına göre iki taraf neyi bilerek anlaşır?
2. Baud'u 115200 yerine 9600 yaparsan ne görürsün, neden?
3. I2C'de aynı iki tele 3 cihaz bağladık — çakışmayı ne önler?
4. `0x77` yazınca BMP180 cevap verir de `0x78` yazınca neden sessiz kalır?
5. SPI'da cihaz seçimi hangi hat/pinle ve nasıl (hangi voltaj düzeyiyle) yapılır?
6. SD kart için neden 4+1 tel gerekir de OLED için 2 tel yeter?
7. CAN çerçevesinde ID "adres" değilse nedir? Çakışmada kim kazanır?
8. CAN'da DLC neyi sınırlar? Bizim çerçevemizde bu işi hangi bayt görüyor?
9. Checksum toplamı neden 0 (mod 256) olmak zorunda? Bozuk çerçevede ne olur?
10. `P0217`'deki "P" ne demek? DTC neden arıza geçtikten sonra da kayıtta kalır?

*(😄)*

---

**Toplam süre:** ~5-6 saat · **Çalışma tarzı:** her adımı Wokwi'de sen uygula →

Hazır `diagram.json`, `sketch.ino` ve `libraries.txt` bu klasörde duruyor —
Wokwi'ye dosya dosya kopyalayabilir ya da adım adım elle kurabilirsin. 🚗








