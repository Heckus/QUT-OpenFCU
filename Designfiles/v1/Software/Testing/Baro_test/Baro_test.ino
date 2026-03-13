#include <SPI.h>

#define RESET 0x1E

#define CONVERT_D1_256 0x40
#define CONVERT_D1_512 0x42
#define CONVERT_D1_1024 0x44
#define CONVERT_D1_2048 0x46
#define CONVERT_D1_4096 0x48

#define CONVERT_D2_256 0x50
#define CONVERT_D2_512 0x52
#define CONVERT_D2_1024 0x54
#define CONVERT_D2_2048 0x56
#define CONVERT_D2_4096 0x58

#define ADC_READ 0x00

#define PROM_READ 0xA0

#define CS_PIN 10

#define CLOCK_SPD 8000000

uint16_t C[7]; //SENS_T1, OFF_T1, TCS, TCO, T_REF, TEMPSENS;
uint32_t D1, D2;
int32_t dT, TMEP, P;
int64_t OFF, SENS;
uint32_t start;

void baro_init() {

  digitalWrite(CS_PIN, LOW);
  SPI.beginTransaction(SPISettings(CLOCK_SPD, MSBFIRST, SPI_MODE0));

  SPI.transfer(RESET);

  SPI.endTransaction();
  digitalWrite(CS_PIN, HIGH);

  for(uint8_t reg = 0; reg < 7; reg++) {
    uint16_t tmp = 0;

    digitalWrite(CS_PIN, LOW);
    SPI.beginTransaction(SPISettings(CLOCK_SPD, MSBFIRST, SPI_MODE0));
    
    SPI.transfer(PROM_READ + 2 * reg);
    tmp += SPI.transfer(ADC_READ);
    tmp <<= 8;
    tmp += SPI.transfer(ADC_READ);

    SPI.endTransaction();
    digitalWrite(CS_PIN, HIGH);
    
    C[reg] = tmp;
    Serial.println(C[reg]);
    
  }
}

void setup() {

  Serial.begin(9600);

  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  SPI.begin();

  baro_init();
}

void loop() {
  
}
