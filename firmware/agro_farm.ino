#include <DHT.h>
#include <Nextion.h>

// ---- Пины
const uint8_t PIN_DHT = 4;
const uint8_t PIN_LEVEL = 7;       // поплавок, замыкает на GND
const uint8_t PIN_COLD_PWM = 9;
const uint8_t PIN_WARM_PWM = 10;
const uint8_t PIN_FAN_LAMP = 5;
const uint8_t PIN_FAN_PLANT = 6;
const uint8_t PIN_PUMP = 8;

// ---- Датчик
#define DHTTYPE DHT22
DHT dht(PIN_DHT, DHTTYPE);

// ---- Nextion: аппаратный Serial
// Компоненты
NexText tTemp  = NexText(0,  1, "tTemp");
NexText tHum   = NexText(0,  2, "tHum");
NexText tLevel = NexText(0,  3, "tLevel");
NexText tWarn  = NexText(0,  4, "tWarn");
NexButton bMode    = NexButton(0, 5, "bMode");
NexButton bPump    = NexButton(0, 6, "bPump");
NexButton bFan     = NexButton(0, 7, "bFan");
NexButton bLampFan = NexButton(0, 8, "bLampFan");
NexSlider hCold = NexSlider(0, 9, "hCold");
NexSlider hWarm = NexSlider(0,10, "hWarm");

NexTouch *nex_listen_list[] = {
  &bMode, &bPump, &bFan, &bLampFan, &hCold, &hWarm, NULL
};

// ---- Состояние
volatile uint8_t modeLamp = 0;     // 0 Off, 1 Cold, 2 Warm, 3 All
volatile uint8_t coldPct = 80;     // 0..100
volatile uint8_t warmPct = 80;     // 0..100
bool pumpOn = false;
bool fanPlantOn = false;
bool fanLampOn = false;

unsigned long pumpOnSince = 0;
const unsigned long PUMP_MAX_MS = 30UL * 1000UL; // авто-отключение 30с

// ---- Пороговые значения
const float T_LOW = 15.0, T_HIGH = 30.0;
const float H_LOW = 30.0, H_HIGH = 85.0;

bool levelLow = false;

// ---- Утилиты
uint8_t pctToPWM(uint8_t pct) {
  if (pct > 100) pct = 100;
  return map(pct, 0, 100, 0, 255);
}
void applyOutputs() {
  // Световые каналы по режиму
  uint8_t cold = 0, warm = 0;
  switch (modeLamp) {
    case 1: cold = pctToPWM(coldPct); warm = 0; break;
    case 2: cold = 0; warm = pctToPWM(warmPct); break;
    case 3: cold = pctToPWM(coldPct); warm = pctToPWM(warmPct); break;
    default: cold = 0; warm = 0; break;
  }
  analogWrite(PIN_COLD_PWM, cold);
  analogWrite(PIN_WARM_PWM, warm);

  // Вентилятор светильника включается вместе со светом, если не принудительно Off
  bool anyLight = (cold > 0 || warm > 0);
  bool lampFanShould = anyLight;
  digitalWrite(PIN_FAN_LAMP, (lampFanShould && fanLampOn) ? HIGH : LOW);

  // Прочие
  digitalWrite(PIN_FAN_PLANT, fanPlantOn ? HIGH : LOW);
  digitalWrite(PIN_PUMP, pumpOn ? HIGH : LOW);
}

void pushUI() {
  char buf[32];

  dtostrf(dht.readTemperature(), 0, 1, buf);
  String sT = String(buf) + "°C";
  tTemp.setText(sT.c_str());

  dtostrf(dht.readHumidity(), 0, 0, buf);
  String sH = String(buf) + "%";
  tHum.setText(sH.c_str());

  tLevel.setText(levelLow ? "Мало" : "Норма");

  // Обновить подпись кнопки режима
  const char* modeNames[] = {"Off","Cold","Warm","All"};
  String cap = String("Режим: ") + modeNames[modeLamp];
  bMode.setText(cap.c_str());

  // Предупреждения
  float T = dht.readTemperature();
  float H = dht.readHumidity();
  String warn = "OK";
  if (isnan(T) || isnan(H)) warn = "Датчик t/φ";
  else {
    bool tempBad = (T < T_LOW || T > T_HIGH);
    bool humBad  = (H < H_LOW || H > H_HIGH);
    if (levelLow || tempBad || humBad) {
      warn = "Тревога:";
      if (levelLow) warn += " уровень";
      if (tempBad)  warn += " t";
      if (humBad)   warn += " φ";
    }
  }
  tWarn.setText(warn.c_str());
}

// ---- Обработчики Nextion
void bModeRelease(void *ptr) {
  modeLamp = (modeLamp + 1) & 0x03;
  applyOutputs();
  pushUI();
}
void bPumpRelease(void *ptr) {
  pumpOn = !pumpOn;
  if (pumpOn) pumpOnSince = millis();
  applyOutputs();
}
void bFanRelease(void *ptr) {
  fanPlantOn = !fanPlantOn;
  applyOutputs();
}
void bLampFanRelease(void *ptr) {
  // Разрешение работы вентилятора светильника
  fanLampOn = !fanLampOn;
  applyOutputs();
}
void hColdPop(void *ptr) {
  uint32_t v=0; hCold.getValue(&v); coldPct = constrain((int)v,0,100);
  applyOutputs();
}
void hWarmPop(void *ptr) {
  uint32_t v=0; hWarm.getValue(&v); warmPct = constrain((int)v,0,100);
  applyOutputs();
}

void setup() {
  pinMode(PIN_LEVEL, INPUT_PULLUP);
  pinMode(PIN_COLD_PWM, OUTPUT);
  pinMode(PIN_WARM_PWM, OUTPUT);
  pinMode(PIN_FAN_LAMP, OUTPUT);
  pinMode(PIN_FAN_PLANT, OUTPUT);
  pinMode(PIN_PUMP, OUTPUT);

  analogWrite(PIN_COLD_PWM, 0);
  analogWrite(PIN_WARM_PWM, 0);
  digitalWrite(PIN_FAN_LAMP, LOW);
  digitalWrite(PIN_FAN_PLANT, LOW);
  digitalWrite(PIN_PUMP, LOW);
  fanLampOn = true; // разрешено по умолчанию

  Serial.begin(9600);
  nexInit();
  dht.begin();

  // Привязка обработчиков
  bMode.attachPop(bModeRelease, &bMode);
  bPump.attachPop(bPumpRelease, &bPump);
  bFan.attachPop(bFanRelease, &bFan);
  bLampFan.attachPop(bLampFanRelease, &bLampFan);
  hCold.attachPop(hColdPop, &hCold);
  hWarm.attachPop(hWarmPop, &hWarm);

  pushUI();
}

void loop() {
  nexLoop(nex_listen_list);

  // Обработка уровня воды с простым антидребезгом
  static uint8_t stable = 0xFF;
  uint8_t raw = digitalRead(PIN_LEVEL); // 0 = замкнут на GND
  stable = (stable << 1) | (raw & 1);
  if (stable == 0x00) levelLow = true;
  else if (stable == 0xFF) levelLow = false;

  // Ограничение времени работы насоса
  if (pumpOn && (millis() - pumpOnSince > PUMP_MAX_MS)) {
    pumpOn = false;
    applyOutputs();
  }

  // Обновление экрана раз в 1 с
  static uint32_t t0 = 0;
  if (millis() - t0 > 1000) {
    t0 = millis();
    pushUI();
  }
}

  Serial.println(" %");
}
