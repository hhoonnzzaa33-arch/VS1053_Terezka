//setVS1053.h
// POZOR:
// Tohle je pro ESP32-S3.
// Pokud ti nejede FSPI, změň FSPI na HSPI nebo si nastav vlastní SPI.
// Piny nejsou univerzální, jsou podle mého zapojení.
//SPIClass *spiTerezka = new SPIClass(FSPI);

#pragma once
#include <Arduino.h>
#include <SPI.h>
extern byte hlasitost, basy, basyF, vysky, vyskyF, aktivniStanice;//uprav si to
// Definice sběrnice pro VS1053 (Terezka)
SPIClass *spiTerezka = new SPIClass(FSPI);
// Definice objektu 
#define VS_MOSI   11
#define VS_MISO   13
#define VS_SCK    12
#define VS_CS     10
#define VS_DCS    9
#define VS_DREQ   8
#define VS_RESET  14
VS1053_Terezka player(spiTerezka, VS_CS, VS_DCS, VS_DREQ);

void setupVS1053() {
  pinMode(VS_RESET, OUTPUT);
  digitalWrite(VS_RESET,LOW);   //reset je0
  delay(100);                   
  digitalWrite(VS_RESET,HIGH);  // provoz je 1
  // 1. Nastartujeme SPI (už máme hotovo)
  spiTerezka->begin(VS_SCK, VS_MISO, VS_MOSI, VS_CS);
  
  // 2. Inicializace VS1053 třídy (ta tvoje Terezka)
  player.begin(); 
  
  // 3. Nahrání hlavního pluginu (pro stabilitu a funkce)
  Serial.println("Nahravam hlavni plugin...");
  player.LoadUserCode(plugin, PLUGIN_SIZE);
  
  // 4. Nahrání ANALYZÉRU (aby fungovalo getBands)
  Serial.println("Nahravam analyzer...");
  player.LoadUserCode(analizer, ANALIZER_SIZE);
  
  Serial.println("Terezka je nactena a ready!");
  player.setVolume(100);
}
void nastavBassTreble() {
  // Registr 0x02 (SCI_BASS):
  // [15:12] Treble Control (0.5dB steps, -8..7)
  // [11:8]  Lower limit frequency (kHz, 1..15)
  // [7:4]   Bass Enhancement (1dB steps, 0..15)
  // [3:0]   Lower limit frequency (10Hz steps, 2..15)
  uint16_t val = ((vysky & 0x0F) << 12) | ((vyskyF & 0x0F) << 8) | ((basy & 0x0F) << 4) | (basyF & 0x0F);
  player.write_register(0x02, val);
  Serial.printf("SCI_BASS: 0x%04X\n", val);
}

