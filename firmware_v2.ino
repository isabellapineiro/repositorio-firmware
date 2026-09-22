/*
 * S2-CP02 - Projeto Motiva | Firmware 2.0
 * Evolucao do Firmware 1.0: mantem leituras/media e adiciona
 * ordenacao, mediana e logica de histerese com estados NORMAL/ALERTA.
 *
 * - Mantem as 5 leituras por sessao (10-20 cm), a cada 2s
 * - Ordena uma copia das leituras e exibe original + ordem crescente
 * - Calcula a mediana (3o elemento do vetor ordenado)
 * - Histerese baseada na mediana:
 *     mediana >= 16      -> ALERTA
 *     14 < mediana < 16  -> mantem estado anterior
 *     mediana <= 14      -> NORMAL
 * - LED: verde = NORMAL, vermelho = ALERTA
 */

#include <WiFi.h>

// ===================== Configuracoes =====================
const char* WIFI_SSID     = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";
const String FIRMWARE_VERSION = "2.0";

// LED RGB (catodo comum)
const int LED_RED_PIN   = 25;
const int LED_GREEN_PIN = 26;
const int LED_BLUE_PIN  = 27;

const unsigned long SESSION_INTERVAL_MS = 48000; // 48 segundos
const unsigned long READING_INTERVAL_MS = 2000;  // 2 segundos
const int READINGS_PER_SESSION = 5;

const float ALERT_THRESHOLD  = 16.0; // mediana >= 16 -> ALERTA
const float NORMAL_THRESHOLD = 14.0; // mediana <= 14 -> NORMAL

enum SystemState { NORMAL, ALERTA };
SystemState currentState = NORMAL;

// ===================== Estado global =====================
float readings[READINGS_PER_SESSION];
float sortedReadings[READINGS_PER_SESSION];
unsigned long sessionStartTime = 0;

// ===================== Setup =====================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);

  randomSeed(analogRead(0));

  Serial.println("========================================");
  Serial.println("MONITORAMENTO DE VEGETACAO - FW 2.0");
  Serial.println("========================================");
  Serial.println("Atualizacao OTA aplicada com sucesso!");

  updateLED(); // estado inicial NORMAL -> verde
  connectWiFi();
  sessionStartTime = millis();
}

// ===================== Loop principal =====================
void loop() {
  runSession();
  waitForNextSession();
}

// ===================== Sessao de leituras =====================
void runSession() {
  Serial.println();
  for (int i = 0; i < READINGS_PER_SESSION; i++) {
    readings[i] = random(100, 201) / 10.0; // 10.0 a 20.0 cm

    Serial.print("Leitura ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(readings[i], 1);
    Serial.println(" cm");

    if (i < READINGS_PER_SESSION - 1) {
      delay(READING_INTERVAL_MS);
    }
  }

  // Copia e ordena (bubble sort simples, suficiente para 5 elementos)
  for (int i = 0; i < READINGS_PER_SESSION; i++) sortedReadings[i] = readings[i];
  bubbleSort(sortedReadings, READINGS_PER_SESSION);

  float sum = 0;
  for (int i = 0; i < READINGS_PER_SESSION; i++) sum += readings[i];
  float average = sum / READINGS_PER_SESSION;
  float median = sortedReadings[2]; // terceiro elemento do vetor ordenado

  Serial.print("Ordem original:  ");
  printArray(readings, READINGS_PER_SESSION);
  Serial.print("Ordem crescente: ");
  printArray(sortedReadings, READINGS_PER_SESSION);

  Serial.print("Media da sessao: ");
  Serial.print(average, 1);
  Serial.println(" cm");
  Serial.print("Mediana da sessao: ");
  Serial.print(median, 1);
  Serial.println(" cm");

  updateState(median);
  updateLED();

  Serial.print("Estado do sistema: ");
  Serial.println(currentState == ALERTA ? "ALERTA" : "NORMAL");
  Serial.println("Proxima sessao em 48 segundos.");
}

// ===================== Utilitarios =====================
void bubbleSort(float arr[], int n) {
  for (int i = 0; i < n - 1; i++) {
    for (int j = 0; j < n - i - 1; j++) {
      if (arr[j] > arr[j + 1]) {
        float tmp = arr[j];
        arr[j] = arr[j + 1];
        arr[j + 1] = tmp;
      }
    }
  }
}

void printArray(float arr[], int n) {
  for (int i = 0; i < n; i++) {
    Serial.print(arr[i], 1);
    if (i < n - 1) Serial.print(", ");
  }
  Serial.println(" cm");
}

// ===================== Histerese =====================
void updateState(float median) {
  if (median >= ALERT_THRESHOLD) {
    currentState = ALERTA;
  } else if (median <= NORMAL_THRESHOLD) {
    currentState = NORMAL;
  }
  // Entre 14 e 16 cm: mantem o estado anterior (nenhuma acao)
}

// ===================== Temporizacao =====================
void waitForNextSession() {
  unsigned long elapsed = millis() - sessionStartTime;
  if (elapsed < SESSION_INTERVAL_MS) {
    delay(SESSION_INTERVAL_MS - elapsed);
  }
  sessionStartTime += SESSION_INTERVAL_MS;
}

// ===================== Wi-Fi =====================
void connectWiFi() {
  Serial.print("Conectando ao WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi conectado. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("AVISO: Wi-Fi nao conectado (nao critico para FW 2.0).");
  }
}

// ===================== LED =====================
void setLED(bool red, bool green, bool blue) {
  digitalWrite(LED_RED_PIN,   red   ? HIGH : LOW);
  digitalWrite(LED_GREEN_PIN, green ? HIGH : LOW);
  digitalWrite(LED_BLUE_PIN,  blue  ? HIGH : LOW);
}

void updateLED() {
  if (currentState == ALERTA) {
    setLED(true, false, false);  // vermelho
  } else {
    setLED(false, true, false);  // verde
  }
}
