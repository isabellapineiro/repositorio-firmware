/*
 * S2-CP02 - Projeto Motiva | Firmware 1.0
 * Monitoramento de vegetacao (simulado) com verificacao de atualizacao OTA.
 *
 * - Gera 5 leituras pseudoaleatorias (10-20 cm) por sessao, a cada 2s
 * - Calcula e exibe a media da sessao
 * - Nova sessao a cada 48s, contados a partir do INICIO da sessao anterior
 * - LED azul indica que a versao 1.0 esta em execucao
 * - Apos pelo menos 3 sessoes, consulta o manifesto remoto (version.json)
 *   e, se houver versao mais nova, baixa o .bin e executa a atualizacao OTA
 *
 * Bibliotecas necessarias (Library Manager): ArduinoJson (Benoit Blanchon)
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <ArduinoJson.h>

// ===================== Configuracoes =====================
const char* WIFI_SSID     = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// Substitua pela URL RAW do seu version.json no GitHub
const char* VERSION_URL = "https://raw.githubusercontent.com/SEU_USUARIO/SEU_REPO/main/version.json";

const String FIRMWARE_VERSION = "1.0";
const int MIN_SESSIONS_BEFORE_CHECK = 3; // minimo de ciclos antes de checar update

// LED RGB (catodo comum) - indicacao visual da versao
const int LED_RED_PIN   = 25;
const int LED_GREEN_PIN = 26;
const int LED_BLUE_PIN  = 27;

const unsigned long SESSION_INTERVAL_MS = 48000; // 48 segundos
const unsigned long READING_INTERVAL_MS = 2000;  // 2 segundos
const int READINGS_PER_SESSION = 5;

// ===================== Estado global =====================
float readings[READINGS_PER_SESSION];
unsigned long sessionStartTime = 0;
int sessionCount = 0;

// ===================== Setup =====================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  setLED(false, false, true); // azul = Firmware 1.0

  randomSeed(analogRead(0));

  Serial.println("========================================");
  Serial.println("MONITORAMENTO DE VEGETACAO - FW 1.0");
  Serial.println("========================================");

  connectWiFi();
  sessionStartTime = millis();
}

// ===================== Loop principal =====================
void loop() {
  runSession();
  sessionCount++;

  if (sessionCount >= MIN_SESSIONS_BEFORE_CHECK) {
    checkForUpdate();
  }

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

  float sum = 0;
  for (int i = 0; i < READINGS_PER_SESSION; i++) sum += readings[i];
  float average = sum / READINGS_PER_SESSION;

  Serial.print("Media da sessao: ");
  Serial.print(average, 1);
  Serial.println(" cm");
  Serial.println("Proxima sessao em 48 segundos.");
}

// ===================== Temporizacao =====================
void waitForNextSession() {
  unsigned long elapsed = millis() - sessionStartTime;
  if (elapsed < SESSION_INTERVAL_MS) {
    delay(SESSION_INTERVAL_MS - elapsed);
  }
  sessionStartTime += SESSION_INTERVAL_MS; // evita drift acumulado
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
    Serial.println("ERRO: Nao foi possivel conectar ao WiFi.");
  }
}

// ===================== Verificacao de atualizacao =====================
void checkForUpdate() {
  Serial.println();
  Serial.println("--- Verificando atualizacao de firmware ---");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ERRO: Sem conexao Wi-Fi. Verificacao cancelada.");
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) return;
  }

  WiFiClientSecure client;
  client.setInsecure(); // simplificacao para ambiente de simulacao Wokwi

  HTTPClient http;
  if (!http.begin(client, VERSION_URL)) {
    Serial.println("ERRO: Nao foi possivel acessar o manifesto de versao.");
    return;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.print("ERRO: Manifesto retornou codigo HTTP ");
    Serial.println(httpCode);
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.println("ERRO: Falha ao interpretar o manifesto (JSON invalido).");
    return;
  }

  String remoteVersion = doc["version"].as<String>();
  String firmwareUrl   = doc["url"].as<String>();

  Serial.print("Versao instalada: ");
  Serial.println(FIRMWARE_VERSION);
  Serial.print("Versao disponivel: ");
  Serial.println(remoteVersion);

  if (remoteVersion.length() == 0 || firmwareUrl.length() == 0) {
    Serial.println("ERRO: Manifesto incompleto (versao ou URL ausente).");
    return;
  }

  if (remoteVersion == FIRMWARE_VERSION) {
    Serial.println("A versao instalada ja e a mais recente.");
    return;
  }

  Serial.println("Nova versao encontrada! Iniciando atualizacao OTA...");
  performOTAUpdate(firmwareUrl);
}

// ===================== Atualizacao OTA =====================
void performOTAUpdate(String firmwareUrl) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, firmwareUrl)) {
    Serial.println("ERRO: Nao foi possivel acessar o arquivo de firmware.");
    return;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.print("ERRO: Download do firmware falhou. Codigo HTTP ");
    Serial.println(httpCode);
    http.end();
    return;
  }

  int contentLength = http.getSize();
  if (contentLength <= 0) {
    Serial.println("ERRO: Tamanho do firmware invalido.");
    http.end();
    return;
  }

  if (!Update.begin(contentLength)) {
    Serial.println("ERRO: Nao ha espaco suficiente para a atualizacao.");
    http.end();
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t written = Update.writeStream(*stream);

  if (written != (size_t)contentLength) {
    Serial.print("ERRO: Gravacao incompleta (");
    Serial.print(written);
    Serial.print("/");
    Serial.print(contentLength);
    Serial.println(" bytes).");
    http.end();
    return;
  }

  Serial.println("Firmware baixado e gravado com sucesso.");

  if (Update.end() && Update.isFinished()) {
    Serial.println("Atualizacao concluida. Reiniciando...");
    http.end();
    delay(1000);
    ESP.restart();
  } else {
    Serial.print("ERRO na atualizacao: ");
    Serial.println(Update.getError());
  }

  http.end();
}

// ===================== LED =====================
void setLED(bool red, bool green, bool blue) {
  digitalWrite(LED_RED_PIN,   red   ? HIGH : LOW);
  digitalWrite(LED_GREEN_PIN, green ? HIGH : LOW);
  digitalWrite(LED_BLUE_PIN,  blue  ? HIGH : LOW);
}
