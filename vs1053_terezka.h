// upraveno pro esp32s3

#include <Arduino.h>
#include <SPI.h>
#include "patches.h"  // Tohle tam MUSÍ být, jinak nezná ty pole plugin a analizer
// SCI Register
const uint8_t SCI_MODE = 0x0;
const uint8_t SCI_STATUS = 0x1;
const uint8_t SCI_BASS = 0x2;
const uint8_t SCI_CLOCKF = 0x3;
const uint8_t SCI_AUDATA = 0x5;
const uint8_t SCI_WRAM = 0x6;
const uint8_t SCI_WRAMADDR = 0x7;
const uint8_t SCI_AIADDR = 0xA;
const uint8_t SCI_VOL = 0xB;
const uint8_t SCI_AICTRL0 = 0xC;
const uint8_t SCI_AICTRL1 = 0xD;
const uint8_t SCI_num_registers = 0xF;
// SCI_MODE bits
const uint8_t SM_SDINEW = 11;  // Bitnumber in SCI_MODE always on
const uint8_t SM_RESET = 2;    // Bitnumber in SCI_MODE soft reset
const uint8_t SM_CANCEL = 3;   // Bitnumber in SCI_MODE cancel song
const uint8_t SM_TESTS = 5;    // Bitnumber in SCI_MODE for tests
const uint8_t SM_LINE1 = 14;   // Bitnumber in SCI_MODE for Line input

class VS1053_Terezka {
private:
  SPIClass* _spi;
  int8_t cs_pin, dcs_pin, dreq_pin;
  uint8_t curvol;
  uint8_t endFillByte;
  SPISettings VS1053_SPI;

  // Čekací smyčka na DREQ
  inline void await_data_request() const {
    while (!digitalRead(dreq_pin)) {
      __asm__ __volatile__("nop");
    }
  }

  // SCI mód (přepsáno na _spi->)
  inline void control_mode_on() {
    _spi->beginTransaction(SPISettings(200000, MSBFIRST, SPI_MODE0));
    digitalWrite(cs_pin, LOW);
  }

  inline void control_mode_off() {
    digitalWrite(cs_pin, HIGH);
    _spi->endTransaction();  // TADY byla chyba (bylo jen SPI)
  }

  // SDI mód (přepsáno na _spi->)
  inline void data_mode_on() {
    _spi->beginTransaction(VS1053_SPI);
    digitalWrite(dcs_pin, LOW);
  }

  inline void data_mode_off() {
    digitalWrite(dcs_pin, HIGH);
    _spi->endTransaction();  // TADY byla chyba
  }

public:
  // Konstruktor se 4 argumenty pro tvé spiTerezka
  VS1053_Terezka(SPIClass* spi_ptr, int8_t _cs, int8_t _dcs, int8_t _dreq)
    : _spi(spi_ptr), cs_pin(_cs), dcs_pin(_dcs), dreq_pin(_dreq) {
    VS1053_SPI = SPISettings(5000000, MSBFIRST, SPI_MODE0);
  }
  bool testComm(const char* header) {
    uint16_t r1, r2, cnt = 0;
    uint16_t delta = 300;
    const uint16_t vstype[] = { 1001, 1011, 1002, 1003, 1053, 1033, 0000, 1103 };

    Serial.println(header);

    // Kontrola DREQ - jestli je v nule, čip pravděpodobně nežije
    if (!digitalRead(dreq_pin)) {
      Serial.println("CHYBA: VS1053 neni spravne zapojena (DREQ je LOW)!");
      pinMode(dreq_pin, INPUT_PULLUP);  // Zabráníme záseku, ale rádio nepojede
      return false;
    }

    // Zátěžový test SPI sběrnice - budeme bušit do registru hlasitosti
    // a číst, jestli se ty data vrací správně.
    for (int i = 0; (i < 0xFFFF) && (cnt < 20); i += delta) {
      write_register(SCI_VOL, i);
      r1 = read_register(SCI_VOL);
      r2 = read_register(SCI_VOL);
      if (r1 != r2 || i != r1 || i != r2) {
        Serial.printf("VS1053 SPI error! Zapsano:%04X R1:%04X R2:%04X\n", i, r1, r2);
        cnt++;
        delay(5);
      }
    }

    if (cnt > 0) return false;

    // Kontrola verze čipu - VS1053 musí vrátit verzi 4
    r1 = (read_register(SCI_STATUS) >> 4) & 0x7;
    if (r1 != 4) {
      Serial.printf("Pozor: Tohle neni VS1053, ale VS%d!\n", vstype[r1]);
      return false;
    }

    Serial.println("VS1053 SPI komunikace je cista a OK!");
    return true;
  }
  void begin() {
    pinMode(dreq_pin, INPUT);
    pinMode(cs_pin, OUTPUT);
    digitalWrite(cs_pin, HIGH);
    pinMode(dcs_pin, OUTPUT);
    digitalWrite(dcs_pin, HIGH);

/*    delay(100);
    wram_write(0xC017, 3);
    wram_write(0xC019, 0);
    delay(100);
    softReset();
    write_register(SCI_AUDATA, 44101);
    write_register(SCI_CLOCKF, 0x6000);
    write_register(SCI_MODE, 0x4800);
    await_data_request();
    endFillByte = wram_read(0x1E06) & 0xFF;*/

    delay(20);
    //printDetails ( "20 msec after reset" ) ;
    if (testComm("Slow SPI, Testing VS1053 read/write registers...")) {
      // Most VS1053 modules will start up in midi mode.  The result is that there is no audio
      // when playing MP3.  You can modify the board, but there is a more elegant way:
      wram_write(0xC017, 3);  // GPIO DDR = 3
      wram_write(0xC019, 0);  // GPIO ODATA = 0
      delay(100);
      //printDetails ( "After test loop" ) ;
      softReset();  // Do a soft reset
      // Switch on the analog parts
      write_register(SCI_AUDATA, 44100 + 1);  // 44.1kHz + stereo
      // The next clocksetting allows SPI clocking at 5 MHz, 4 MHz is safe then.
      write_register(SCI_CLOCKF, 6 << 12);  // Normal clock settings
      // multiplyer 3.0 = 12.2 MHz
      //SPI Clock to 4 MHz. Now you can set high speed SPI clock.
      VS1053_SPI = SPISettings(5000000, MSBFIRST, SPI_MODE0);
      write_register(SCI_MODE, _BV(SM_SDINEW) | _BV(SM_LINE1));
      testComm("Fast SPI, Testing VS1053 read/write registers again...");
      delay(10);
      await_data_request();
      endFillByte = wram_read(0x1E06) & 0xFF;
      Serial.printf("endFillByte is %X", endFillByte);
      //printDetails ( "After last clocksetting" ) ;
      delay(100);
    }
  }

  void softReset() {
    write_register(SCI_MODE, 0x4804);
    delay(20);
    await_data_request();
  }

  void write_register(uint8_t reg, uint16_t value) {
    control_mode_on();
    _spi->write(0x02);  // Tady musí být _spi->
    _spi->write(reg);
    _spi->write16(value);  // S3 umí write16 přímo
    await_data_request();
    control_mode_off();
  }

  uint16_t read_register(uint8_t reg) {
    uint16_t result;
    control_mode_on();
    _spi->write(0x03);
    _spi->write(reg);
    result = (_spi->transfer(0xFF) << 8) | _spi->transfer(0xFF);
    await_data_request();
    control_mode_off();
    return result;
  }

  void wram_write(uint16_t address, uint16_t data) {
    write_register(SCI_WRAMADDR, address);
    write_register(SCI_WRAM, data);
  }

  uint16_t wram_read(uint16_t address) {
    write_register(SCI_WRAMADDR, address);
    return read_register(SCI_WRAM);
  }

  void LoadUserCode(const unsigned short* iplugin, uint16_t sizea) {
    for (uint16_t i = 0; i < sizea;) {
      unsigned short addr = iplugin[i++];
      unsigned short n = iplugin[i++];
      if (n & 0x8000U) {
        n &= 0x7FFF;
        unsigned short val = iplugin[i++];
        while (n--) write_register((uint8_t)addr, val);
      } else {
        while (n--) write_register((uint8_t)addr, iplugin[i++]);
      }
    }
  }

  void getBands(uint16_t* spectrum) {
    write_register(SCI_WRAMADDR, 0x1810 + 4);
    for (uint16_t i = 0; i < 14; i++) {
      spectrum[i] = (read_register(SCI_WRAM));
    }
  }

  void playChunk(uint8_t* data, size_t len) {
    data_mode_on();
    size_t i = 0;
    while (i < len) {
      await_data_request();
      size_t chunk = (len - i > 32) ? 32 : (len - i);
      _spi->writeBytes(data + i, chunk);  // Tady musí být _spi->
      i += chunk;
    }
    data_mode_off();
  }

  bool readyForData() {
    return digitalRead(dreq_pin) == HIGH;
  }

  void setVolume(uint8_t vol) {
    curvol = vol;
    uint16_t v = map(vol, 0, 100, 0xF8, 0x00);
    write_register(SCI_VOL, (v << 8) | v);
  }
};
