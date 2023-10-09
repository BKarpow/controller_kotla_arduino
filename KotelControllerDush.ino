#include <Arduino.h>
#include <TM1637Display.h>
#include <AnalogKey.h>
#include <GyverButton.h>
#include <GyverNTC.h>
#include <EEPROM.h>
#include <TimerMs.h>

TimerMs timerHold(3000, 1, 0);
TimerMs timerNTC(500, 1, 0);
TimerMs timerPotVal(250, 1, 0);
TimerMs timerShowError(5000, 1, 1);

TimerMs triplePisk(300, 1, 0);
bool toggleTriplePisk = 0;
byte iteratorTriplePisk = 0;


#define BUZZ_PIN 2
#define BTN_PIN 3
#define RELE_PIN 4
#define CLK 5 //Display
#define DIO 6 //Display
#define BLU_LED 9 // Blu led intocator

GButton butt1(BTN_PIN);
GyverNTC therm(1, 10000, 3450);
TM1637Display display(CLK, DIO);

// uint32_t timerHold; // для таймеру утримання кнопки
// uint16_t periodTimerHold = 3000; // період за який рахуєтся одне утримання кнопки
// uint32_t timerNTC;
// uint16_t periodReadNTC = 500; // період опитування термистора
// uint32_t timerPotVal;
uint32_t timerDushActive;
uint16_t periodDushActive = 1200; // період пікання при активації режиму душу
byte gister = 3; // розмір петлі гістерезаса
bool maxTempTrigger = 0; // прапорець перевищення температури
bool dushActive = 0; // прапорець режиму душу
int potVal; // для значення потенціометра
uint8_t potValByte;
bool setPotVal = 0;
int tempNTC; // для температури термистора
uint32_t maxTimeDushActive = 2700000; // період через який автоматично вимкнется режим душу
uint32_t timerTimeDushActive;
uint32_t maxTimeReleActive = 1800000; // період через який автоматично вимкнется реле
uint32_t timerTimeRelehActive;
bool releActive;
uint32_t timerCheck;
uint16_t periodCheck = 10000; // Період перевірки температури при звичайному режимі
byte tempMscTogglePisk = 89; // Температура при якій біде подаватися тривожний сигнал
uint8_t trasholdTempReleActibe = 35; // Поріг спрацювання відмикання насосу!
int fadeAmount = 2; // Крок зміни яскравості LED
int brightness = 0; // Початкова яскравість LED
uint32_t timerIntervalBlinkLed; // таймер для мигання LED
uint8_t intervalBlinkLed = 28; // інтервал зміни яскравості LED
uint32_t timerShowValPot;
uint16_t intervalShowValPot = 2000;
bool showValPot = 0;
bool showTrashold = 0;
bool showError = 0;


void setup() {
  Serial.begin(9600);
  butt1.setDebounce(50);        // настройка антидребезга (по умолчанию 80 мс)
  butt1.setTimeout(1000);        // настройка таймаута на удержание (по умолчанию 500 мс)
  butt1.setClickTimeout(600);   // настройка таймаута между кликами (по умолчанию 300 мс)
  butt1.setType(HIGH_PULL);
  butt1.setDirection(NORM_OPEN);
  pinMode(RELE_PIN, OUTPUT);
  digitalWrite(RELE_PIN, HIGH);
  // attachInterrupt(1, isr, FALLING);
  tone(BUZZ_PIN, 2100, 750);
  // power.setSleepMode(POWERDOWN_SLEEP);
  Serial.println("Start system");
  pinMode(BLU_LED, OUTPUT);
}


void loop() {
  butt1.tick();
  display.setBrightness(1);
  mainBuzzerLoop();

  // ReadData
  readData();

  if (tempNTC <= 0 || tempNTC >= 95)  {
    dspe(1);
    // alarmBuzz();
    Serial.println("Alarm !");
  } else {
    //checks
    checkTemp();
    if (releActive) { 
      checkTimeRele();
      blinkLed();
    } else {
      digitalWrite(BLU_LED, LOW);
    }
    // power.sleepDelay(12000);
    //Button helpers
    btnHelper();
    // WORK
    if (dushActive) {
      tempControl();
      dushActiveHelper();
    } 
  }

  


}

void dushActiveHelper() {
  if (millis() - timerDushActive >= periodDushActive ) {
    timerDushActive = millis();
    piBuzz();
  }
  if (millis() - timerTimeDushActive >= maxTimeDushActive) {
    dushActive = 0;
    disableRele();
    toggleTriplePisk = 1;
  }
}

void readData() {
  int potValTemp = potVal;
  if (timerPotVal.tick()) {
    potVal = analogRead(A0);
    potValByte = map(potVal, 0, 1023, 20, 75); // potVal = map(potVal, 0, 1023, 45, 72);
    
  }
  dspPot();
  if (timerNTC.tick()) {
    tempNTC = int(therm.getTempAverage());
    displayTemp(tempNTC);
    dspTrashold();
  }

}

void tempControl() {
  byte ptVal = EEPROM.read(0);
  if (tempNTC >= ptVal && !maxTempTrigger) {
    maxTempTrigger = 1;
    periodDushActive = int(periodDushActive/2);
    disableRele();
  }
  // if () {
    // tripleBuzz();
    // periodDushActive = int(periodDushActive/2);
    if (tempNTC <= (ptVal - gister) && maxTempTrigger) {
      maxTempTrigger = 0;
      periodDushActive = int(periodDushActive*2);
      enableRele();
    }
  // }
}

void btnHelper() {
  if (butt1.isClick() ) {
    piBuzz();
    if (showTrashold) {
      showTrashold = 0;
    }
    if (setPotVal) {
      EEPROM.put(0, potValByte);
      toggleTriplePisk = 1;
      setPotVal = 0;
      showValPot = 0;
    }
    if (dushActive) {
      dushActive = 0;
      disableRele();
      toggleTriplePisk = 1;
      if (maxTempTrigger) {
        periodDushActive = int(periodDushActive/2);
      }
    }
  }

  if (butt1.isDouble()) {
    showValPot = 1;
    setPotVal = 1;
    
  }
  if (butt1.isTriple()) {
    showTrashold = 1;
    
  }
  if (timerHold.tick()) {
    if (butt1.isHold()) {
      dushActive = 1;
      timerTimeDushActive = millis();
      toggleTriplePisk = 1;
      enableRele();
    }
  }
}

void piBuzz() {
  tone(BUZZ_PIN, 2100, 100);
}

// void tripleBuzz() {
//     for (int i = 0; i<=3; i++) {
//       piBuzz();
//     }
// }

// void alarmBuzz() {
//   disableRele();
//   for (int i = 0; i <=10; i++) {
//       tone(BUZZ_PIN, 3000, 300);
//       // delay(300);
//       tone(BUZZ_PIN, 2500, 300);
//       // delay(300);
//   }
// }

// void piskNumber(int digit) {
//   if (digit >= 10 && digit <= 99) {
//     int first = digit / 10;
//     int two = digit % 10;
//     delay(600);
//     for (int iter1 = 0; iter1 < first; iter1++){
//       piBuzz();
//       delay(650);
//     }
//     delay(1200);
//     if (two == 0) {
//       tone(BUZZ_PIN, 800, 600);
//       delay(650);
//     }
//     for (int iter2 = 0; iter2 < two; iter2++){
//       piBuzz();
//       delay(650);
//     }
//   } else if (digit > 0 && digit <= 9) {
//     for (int iter1 = 0; iter1 < digit; iter1++){
//       piBuzz();
//       delay(650);
//     }
//   } else {
//     tripleBuzz();
//   }
  
// }

void enableRele() {
  if (tempNTC < trasholdTempReleActibe ) { 
   dspe(4);
    // dushActive = 0;
  } else {
    digitalWrite(RELE_PIN, LOW);
    releActive = 1;
    timerTimeRelehActive = millis();
    Serial.println("Rele ENABLE!");
  }
  
}

void disableRele() {
  digitalWrite(RELE_PIN, HIGH);
  releActive = 0;
  Serial.println("Rele DISABLE!");
  // digitalWrite(BLU_LED, LOW);
}

void checkTimeRele() {
  if (millis() - timerTimeRelehActive >= maxTimeReleActive ) {
    disableRele();
    Serial.println("Timer rele disable!");
  }
}

void checkTemp() {
  if (millis() - timerCheck >= periodCheck) {
    timerCheck = millis();
    // dspe(2);
    if (tempNTC >= tempMscTogglePisk) {
      tone(BUZZ_PIN, 3000, (periodCheck - int(periodCheck/4) ) );
    }
    
  }
}


void displayTemp(int temp) {
  uint8_t dsp[] = {0x00, 0x00, 0x00, 0x00};
  dsp[0] = SEG_F | SEG_G | SEG_E | SEG_D; // t
  if (temp >= 10 && temp <= 99) {
    dsp[2] = display.encodeDigit(int(temp / 10));
    dsp[3] = display.encodeDigit(int(temp % 10));
  } else if (temp >= 0 && temp <= 9) {
    dsp[2] = display.encodeDigit(0);
    dsp[3] = display.encodeDigit(int(temp));
  } else {
    dspe(3);
  }

  if (dushActive) {
    dsp[0] = SEG_B | SEG_C | SEG_D | SEG_E | SEG_G; // d
    dsp[1] = SEG_E | SEG_D | SEG_C ; // u
  }
  if (!showValPot && !showTrashold && !showError) display.setSegments(dsp);
}

void dspe(int errorCode) {
  showError = 1;
  uint8_t dsp[] = {                        
    SEG_A | SEG_F | SEG_G | SEG_E | SEG_D, // E
    SEG_E | SEG_G,                         // r
    SEG_E | SEG_G,                         // r
    display.encodeDigit(errorCode)        // 0
  };
  if (showError) {
    display.setSegments(dsp);
    if (timerShowError.tick() && showError) showError = 0;
  }
  
}

void dspPot() {
  uint8_t dsp[] = {
    SEG_A | SEG_G | SEG_F | SEG_E | SEG_B, // P
    SEG_F | SEG_G | SEG_E | SEG_D, // t
    display.encodeDigit( potValByte / 10),
    display.encodeDigit( potValByte % 10)
  };
  if (showValPot && !showTrashold && !showError) display.setSegments(dsp);
}

void dspTrashold() {
  byte p = EEPROM.read(0);
  uint8_t dsp[] = {
    SEG_F | SEG_G | SEG_E | SEG_D, // t
    SEG_E | SEG_G, // r
    display.encodeDigit( p / 10),
    display.encodeDigit( p % 10)
  };
  if (showTrashold && !showError) display.setSegments(dsp);
}

void blinkLed() {
  uint32_t currentMillis = millis();
  if (currentMillis - timerIntervalBlinkLed >= intervalBlinkLed) {
    timerIntervalBlinkLed = currentMillis;

    // Збільшуємо або зменшуємо яскравість
    brightness = brightness + fadeAmount;

    // Перевірка діапазону (0-255) для яскравості
    if (brightness <= 0 || brightness >= 255) {
      fadeAmount = -fadeAmount; // Зміна напрямку зміни яскравості
    }

    analogWrite(BLU_LED, brightness);
  }
}

void mainBuzzerLoop() {
  triplePiskHelper();
}

void triplePiskHelper() {
  if (iteratorTriplePisk >= 3) {
    iteratorTriplePisk = 0;
    toggleTriplePisk = 0;
  }
  if (triplePisk.tick() && toggleTriplePisk) {
    piBuzz();
    iteratorTriplePisk++;
  }
}

