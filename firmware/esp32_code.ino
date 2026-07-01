#define BLYNK_TEMPLATE_ID "......"
#define BLYNK_TEMPLATE_NAME "Motor Predictive Maintenance"
#define BLYNK_AUTH_TOKEN "........"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// ================= WIFI =================
#define WIFI_SSID "..."
#define WIFI_PASSWORD "......."

// ================= SENSOR PINS =================
#define LM35_PIN 34
#define CURRENT_PIN 35
#define VIB_PIN 27
#define IR_PIN 14

// ================= MOTOR DRIVER PINS =================
#define RPWM 25
#define LPWM 26

BlynkTimer timer;

// ================= RPM VARIABLES =================
volatile int pulseCount = 0;

unsigned long lastPulseTime = 0;
unsigned long lastRPMTime = 0;

int rpm = 0;

// ================= MOTOR SPEED =================
int motorSpeed = 180;

// ================= CURRENT SENSOR =================
float offset = 0;
float sensitivity = 0.066;

// ================= INTERRUPT FUNCTION =================
void IRAM_ATTR countPulse()
{
  unsigned long currentTime = millis();

  if (currentTime - lastPulseTime > 5)
  {
    pulseCount++;
    lastPulseTime = currentTime;
  }
}

// ================= BLYNK MOTOR CONTROL =================
BLYNK_WRITE(V6)
{
  motorSpeed = param.asInt();

  ledcWrite(0, motorSpeed);
  ledcWrite(1, 0);
}

// ================= CURRENT SENSOR CALIBRATION =================
void calibrateCurrentSensor()
{
  float sum = 0;

  Serial.println("Calibrating current sensor...");

  for (int i = 0; i < 100; i++)
  {
    sum += analogRead(CURRENT_PIN);
    delay(5);
  }

  float avg = sum / 100.0;

  offset = avg * 3.3 / 4095.0;

  Serial.print("Offset voltage: ");
  Serial.println(offset);
}

// ================= MAIN FUNCTION =================
void sendData()
{
  // ================= TEMPERATURE =================
  float temp = analogRead(LM35_PIN) * 3.3 / 4095.0 * 100;

  // ================= CURRENT =================
  float total = 0;

  for (int i = 0; i < 50; i++)
  {
    total += analogRead(CURRENT_PIN);
    delay(1);
  }

  float avg = total / 50.0;

  float voltage = avg * 3.3 / 4095.0;

  float current = abs((voltage - offset) / sensitivity);

  // Remove tiny noise
  if (current < 0.03)
  {
    current = 0;
  }

  // ================= RPM =================
  if (millis() - lastRPMTime >= 1000)
  {
    if (pulseCount == 0)
    {
      rpm = 0;
    }
    else
    {
      rpm = pulseCount * 20;
    }

    Serial.print("Pulse Count: ");
    Serial.println(pulseCount);

    pulseCount = 0;

    lastRPMTime = millis();
  }

  // No pulse = stopped motor
  if (millis() - lastPulseTime > 1500)
  {
    rpm = 0;
  }

  // ================= VIBRATION =================
  int vibration = digitalRead(VIB_PIN);

  // ================= SERIAL MONITOR =================
  Serial.print("Temp: ");
  Serial.print(temp);

  Serial.print(" | Current: ");
  Serial.print(current);

  Serial.print(" | RPM: ");
  Serial.print(rpm);

  Serial.print(" | Vib: ");
  Serial.println(vibration);

  // ================= UNITY SERIAL FORMAT =================
  Serial.print(rpm);
  Serial.print(",");

  Serial.print(temp);
  Serial.print(",");

  Serial.print(current);
  Serial.print(",");

  Serial.println(vibration);

  // ================= BLYNK =================
  Blynk.virtualWrite(V1, temp);
  Blynk.virtualWrite(V2, current);
  Blynk.virtualWrite(V3, rpm);
  Blynk.virtualWrite(V4, vibration);
}

// ================= SETUP =================
void setup()
{
  Serial.begin(115200);

  pinMode(VIB_PIN, INPUT);
  pinMode(IR_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(IR_PIN), countPulse, FALLING);

  // ================= MOTOR PWM =================
  ledcSetup(0, 5000, 8);
  ledcAttachPin(RPWM, 0);

  ledcSetup(1, 5000, 8);
  ledcAttachPin(LPWM, 1);

  // Start motor
  ledcWrite(0, motorSpeed);
  ledcWrite(1, 0);

  // Calibration
  delay(2000);

  calibrateCurrentSensor();

  // ================= WIFI =================
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");

  // ================= BLYNK =================
  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect();

  // ================= TIMER =================
  timer.setInterval(2000L, sendData);
}

// ================= LOOP =================
void loop()
{
  Blynk.run();
  timer.run();
}
