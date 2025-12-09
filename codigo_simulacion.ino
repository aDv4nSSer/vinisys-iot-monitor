/*
  PROYECTO: Bodega 4.0 - VERSIÓN FINAL (MASTER)
  HARDWARE: Arduino UNO + Sensores Reales
  FUNCIONES: Menú Cepa, Climatización Híbrida, Alarma Gas, LCD Full Info.
*/

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// --- 1. DISEÑO DE ICONOS ---
byte IconoTemp[8]  = {B00100, B01010, B01010, B01110, B01110, B11111, B11111, B01110}; 
byte IconoFuego[8] = {B00100, B01010, B10001, B10101, B10101, B01110, B00100, B00000}; // HEAT
byte IconoFan[8]   = {B00000, B10001, B01010, B00100, B01010, B10001, B00000, B00000}; // COOL
byte IconoGas[8]   = {B00000, B01110, B10001, B10101, B10001, B01110, B00000, B00000}; // GAS

// --- 2. MAPA DE PINES (Verificado) ---
// Botones (Conectar a GND)
#define BTN_PREV   11  
#define BTN_SELECT 12  
#define BTN_NEXT   13  

// Sensores
#define PIN_PH A0       
#define PIN_GAS A1      
#define PIN_TEMP A2     // Sensor TMP36
#define PIN_TRIG 3
#define PIN_ECHO 4

// Actuadores
#define PIN_MOTOR 7      
#define PIN_LED_ROJO 8   
#define PIN_LED_VERDE 9  
#define PIN_BUZZER 10    

// --- 3. CONFIGURACIÓN ---
struct PerfilVino {
  String nombre;
  int tempIdeal;
};

PerfilVino cepas[3] = {
  {"Cabernet", 29}, // Tinto Robusto
  {"Merlot", 25},   // Tinto Medio
  {"Sauvignon", 18} // Blanco Frio
};

const int TOLERANCIA = 2; // Banda muerta de +/- 2 grados

LiquidCrystal_I2C lcd(0x20, 16, 2); 

// Variables
int modoPantalla = 0; 
int cepaSeleccionada = 0; 
int indiceMenu = 0; 

// Lecturas
int temp = 0;
int gas = 0;
float ph = 0;
int nivel = 0;
String estadoSistema = "OK"; 

void setup() {
  Serial.begin(9600);
  
  lcd.init();
  lcd.backlight();

  // Cargar Iconos
  lcd.createChar(0, IconoTemp);
  lcd.createChar(1, IconoFuego); 
  lcd.createChar(2, IconoFan);
  lcd.createChar(3, IconoGas);

  // Pines Input Pullup (Activos en LOW)
  pinMode(BTN_PREV, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_MOTOR, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_ROJO, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);

  lcd.setCursor(0,0);
  lcd.print("Bodega 4.0");
  lcd.setCursor(0,1);
  lcd.print("Iniciando...");
  delay(1000);
}

void loop() {
  if (modoPantalla == 0) {
    runModoMenu();
  } else {
    runModoMonitor();
  }
}

// --- MENÚ DE SELECCIÓN ---
void runModoMenu() {
  static int ultimoIndice = -1;
  
  if (indiceMenu != ultimoIndice) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ELEGIR CEPA:");
    lcd.setCursor(0, 1);
    lcd.print("< "); 
    lcd.print(cepas[indiceMenu].nombre);
    lcd.print(" >");
    ultimoIndice = indiceMenu;
  }

  if (digitalRead(BTN_PREV) == LOW) {
    indiceMenu--; if (indiceMenu < 0) indiceMenu = 2; delay(200);
  }
  if (digitalRead(BTN_NEXT) == LOW) {
    indiceMenu++; if (indiceMenu > 2) indiceMenu = 0; delay(200);
  }
  if (digitalRead(BTN_SELECT) == LOW) {
    cepaSeleccionada = indiceMenu;
    modoPantalla = 1; 
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Meta: "); lcd.print(cepas[cepaSeleccionada].tempIdeal); lcd.print("C");
    tone(PIN_BUZZER, 2000, 100); 
    delay(1500);
    lcd.clear();
  }
}

// --- PANTALLA DE MONITORIZACIÓN ---
void runModoMonitor() {
  leerSensores();
  controlarLogica();

  // --- FILA 0: TEMP Y ESTADO ---
  lcd.setCursor(0, 0);
  lcd.write(0); // Icono Temp
  lcd.print(temp);
  lcd.print("/");
  lcd.print(cepas[cepaSeleccionada].tempIdeal);
  
  lcd.setCursor(9, 0); 
  if (estadoSistema == "HEAT") {
    lcd.write(1); lcd.print("HEAT ");
  } else if (estadoSistema == "COOL") {
    lcd.write(2); lcd.print("COOL ");
  } else if (estadoSistema == "GAS") {
    lcd.print("!GAS!");
  } else {
    lcd.print("  OK   ");
  }

  // --- FILA 1: GAS, pH, NIVEL ---
  lcd.setCursor(0, 1);
  
  // Gas (Formato corto)
  lcd.print("G:"); 
  if(gas > 999) lcd.print("999"); else lcd.print(gas);
  
  // pH
  lcd.print(" p:"); 
  lcd.print(ph, 1);
  
  // Nivel
  lcd.print(" L:"); 
  lcd.print(nivel);

  // SALIR AL MENÚ
  if (digitalRead(BTN_SELECT) == LOW) {
    modoPantalla = 0; delay(300);
  }
  
  enviarJSON(); 
  delay(100);
}

void leerSensores() {
  // Temp (Suavizada)
  long suma = 0;
  for(int i=0; i<5; i++) { suma += analogRead(PIN_TEMP); delay(2); }
  float voltage = (suma / 5.0) * 5.0 / 1024.0;
  temp = (int)((voltage - 0.5) * 100); 

  // Gas
  gas = analogRead(PIN_GAS);
  
  // pH
  ph = map(analogRead(PIN_PH), 0, 1023, 0, 140) / 10.0; 

  // Nivel
  digitalWrite(PIN_TRIG, LOW); delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  nivel = pulseIn(PIN_ECHO, HIGH) * 0.034 / 2; 
}

void controlarLogica() {
  int objetivo = cepas[cepaSeleccionada].tempIdeal;
  bool motorOn = false;

  // 1. SEGURIDAD GAS
  if (gas > 550) {
    estadoSistema = "GAS";
    motorOn = true;
    tone(PIN_BUZZER, 1000);
    digitalWrite(PIN_LED_ROJO, HIGH);
    digitalWrite(PIN_LED_VERDE, LOW);
  } 
  // 2. CLIMATIZACIÓN
  else {
    noTone(PIN_BUZZER);
    
    if (temp > objetivo + TOLERANCIA) {
      estadoSistema = "COOL"; // Enfriar
      motorOn = true;
      digitalWrite(PIN_LED_ROJO, HIGH); 
      digitalWrite(PIN_LED_VERDE, LOW);
    }
    else if (temp < objetivo - TOLERANCIA) {
      estadoSistema = "HEAT"; // Calentar
      motorOn = true;
      digitalWrite(PIN_LED_ROJO, HIGH); 
      digitalWrite(PIN_LED_VERDE, LOW);
    }
    else {
      estadoSistema = "OK";
      motorOn = false;
      digitalWrite(PIN_LED_ROJO, LOW);
      digitalWrite(PIN_LED_VERDE, HIGH);
    }
  }

  if (motorOn) digitalWrite(PIN_MOTOR, HIGH);
  else digitalWrite(PIN_MOTOR, LOW);
}

void enviarJSON() {
  Serial.print("{\"vino\":\""); Serial.print(cepas[cepaSeleccionada].nombre); Serial.print("\",");
  Serial.print("\"temp\":"); Serial.print(temp); Serial.print(",");
  Serial.print("\"gas\":"); Serial.print(gas); Serial.print(",");
  Serial.print("\"ph\":"); Serial.print(ph); Serial.print(",");
  Serial.print("\"estado\":\""); Serial.print(estadoSistema); Serial.print("\"}");
  Serial.println();
}
