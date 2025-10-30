// Код для Arduino
// Работа с датчиками температуры (DHT22), уровнем воды (поплавковый датчик)
// Управление светом (фито-лампа, холодный и тёплый свет), насосом, вентиляторами

#define DHTPIN A1 // Пин для датчика DHT22
#define DHTTYPE DHT22 // Тип датчика
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(9600);
  dht.begin();
  pinMode(9, OUTPUT); // Пин для холодного света
  pinMode(10, OUTPUT); // Пин для тёплого света
  pinMode(8, OUTPUT); // Пин для насоса
}

void loop() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Ошибка чтения с датчика!");
    return;
  }
  Serial.print("Температура: ");
  Serial.print(temperature);
  Serial.print(" °C  Влажность: ");
  Serial.print(humidity);
  Serial.println(" %");
}
