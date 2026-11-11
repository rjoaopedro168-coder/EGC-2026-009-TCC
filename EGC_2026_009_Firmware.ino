
// ============================================================
//  DISPOSITIVO INTELIGENTE DE MEDICAO LINEAR E CALCULO DE AREA
//  ESP32 + LCD I2C 16x2 + Encoder E38S6G5-400BG24N + Teclado 4x4
//  TCC - Engenharia da Computacao - Uni Salesiano - Aracatuba SP
//  Wendel Lucas Silveira  RA: 214940
//  Joao Pedro Rodrigues da Rocha  RA: 215751
// ============================================================
//
//  ESPECIFICACOES DO ENCODER E38S6G5-400BG24N:
//  - 400 pulsos por volta (PPR)
//  - Saida NPN coletor aberto (canal A e B)
//  - Alimentacao: 5 a 24 VDC
//  - Diametro corpo: 38mm | Eixo: 6mm
//
//  PINAGEM DO ENCODER (fios padrao):
//  Vermelho  = VCC  (5V a 24V)
//  Preto     = GND
//  Verde     = Canal A  -> GPIO 34 (ESP32)
//  Branco    = Canal B  -> GPIO 35 (ESP32)
//
//  ATENCAO: GPIO 34 e 35 sao somente entrada no ESP32
//  NAO conectar resistor pullup externo - usar 10k para VCC
//
//  PINAGEM LCD I2C:
//  VCC -> 3.3V ou 5V
//  GND -> GND
//  SDA -> GPIO 21
//  SCL -> GPIO 22
//
//  PINAGEM TECLADO MEMBRANA 4x4:
//  Linhas  R1=GPIO13  R2=GPIO12  R3=GPIO14  R4=GPIO27
//  Colunas C1=GPIO26  C2=GPIO25  C3=GPIO33  C4=GPIO32
//
//  PUSH BUTTON na manopla:
//  Um lado -> GPIO 4
//  Outro   -> GND
//  (INPUT_PULLUP interno do ESP32)
//
//  TECLADO - FUNCOES DAS TECLAS:
//  [0-9]  = digitar valores numericos
//  [*]    = apagar ultimo digito digitado
//  [#]    = confirmar / avancar fase
//  [A]    = nao utilizado (reservado)
//  [B]    = nao utilizado (reservado)
//  [C]    = nao utilizado (reservado)
//  [D]    = nao utilizado (reservado)
//
// ============================================================

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// ============================================================
// CONFIGURACOES DO LCD
// ============================================================
// Endereco I2C: normalmente 0x27 ou 0x3F
// Para descobrir rode o sketch I2CScanner antes
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ============================================================
// CONFIGURACOES DO TECLADO MEMBRANA 4x4
// ============================================================
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {13, 12, 14, 27};  // R1 R2 R3 R4
byte colPins[COLS] = {26, 25, 33, 32};  // C1 C2 C3 C4

Keypad teclado = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ============================================================
// PINOS DO ENCODER E BOTAO
// ============================================================
#define ENCODER_A    34   // Canal A - somente entrada
#define ENCODER_B    35   // Canal B - somente entrada
#define BOTAO_PIN     4   // Push button na manopla
#define LED_STATUS    2   // LED onboard ESP32

// ============================================================
// CONFIGURACOES DO ENCODER E38S6G5-400BG24N
// 400 pulsos por volta (PPR)
// Leitura em quadratura = 400 pulsos por volta
// ============================================================
#define PPR  400   // Pulsos Por Rotacao do encoder

// ============================================================
// CONFIGURACAO DA RODA DE MEDICAO
// ============================================================
#define DIAMETRO_RODA_MM   70.0     // <<< ALTERE AQUI com o diametro real
const float CIRC_MM = 3.14159265 * DIAMETRO_RODA_MM;
// Distancia por pulso em metros:
// dist_por_pulso = circunferencia / PPR / 1000
const float DIST_POR_PULSO_M = CIRC_MM / PPR / 1000.0;

// ============================================================
// DENSIDADE DO MATERIAL
// Massa asfaltica compactada: ~2560 kg/m3
// ============================================================
#define DENSIDADE_KG_M3  2560.0

// ============================================================
// DEBOUNCE DO BOTAO
// ============================================================
#define DEBOUNCE_MS  50

// ============================================================
// MAQUINA DE ESTADOS FINITOS (FSM)
// ============================================================
enum Estado {
  TELA_INICIAL,
  MEDIR_COMPRIMENTO,
  SALVA_COMPRIMENTO,
  MEDIR_LARGURA,
  SALVA_LARGURA,
  DIGITAR_ALTURA,
  CALCULAR,
  MOSTRAR_RESULTADO,
  NOVA_MEDICAO
};

Estado estadoAtual = TELA_INICIAL;

// ============================================================
// VARIAVEIS DO ENCODER (usadas na ISR)
// ============================================================
volatile long pulsos          = 0;
volatile int  ultimoEstadoA   = LOW;

// ============================================================
// VARIAVEIS DE MEDICAO
// ============================================================
float comprimento_m = 0.0;
float largura_m     = 0.0;
float altura_m      = 0.0;
float volume_m3     = 0.0;
float massa_kg      = 0.0;

// ============================================================
// VARIAVEIS DO TECLADO
// ============================================================
String bufferTeclado = "";

// ============================================================
// VARIAVEIS DE CONTROLE DE TELA
// ============================================================
unsigned long ultimoDisplay  = 0;
unsigned long ultimaTroca    = 0;
bool          telaVolume     = true;
bool          telaAtualizada = false;

// ============================================================
// VARIAVEIS DO BOTAO
// ============================================================
bool          botaoAnterior  = HIGH;
unsigned long ultimoDebounce = 0;

// ============================================================
// ISR - ENCODER (executada a cada borda do canal A)
// Leitura em quadratura usando canal A e B
// ============================================================
void IRAM_ATTR encoderISR() {
  int A = digitalRead(ENCODER_A);
  int B = digitalRead(ENCODER_B);
  if (A != ultimoEstadoA) {
    if (B != A) {
      pulsos++;    // Frente
    } else {
      pulsos--;    // Tras (seguranca)
    }
    ultimoEstadoA = A;
  }
}

// ============================================================
// Zera contador do encoder com seguranca
// ============================================================
void zerarEncoder() {
  noInterrupts();
  pulsos = 0;
  interrupts();
}

// ============================================================
// Retorna distancia atual em metros baseado nos pulsos
// ============================================================
float distanciaAtual() {
  noInterrupts();
  long p = pulsos;
  interrupts();
  return (float)abs(p) * DIST_POR_PULSO_M;
}

// ============================================================
// Exibe duas linhas no LCD (limpa e escreve)
// ============================================================
void lcd_print(String l0, String l1) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(l0.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(l1.substring(0, 16));
}

// ============================================================
// Le botao com debounce
// Retorna true se houve clique valido
// ============================================================
bool botaoClicado() {
  bool leitura = digitalRead(BOTAO_PIN);
  if (leitura == LOW && botaoAnterior == HIGH) {
    if ((millis() - ultimoDebounce) > DEBOUNCE_MS) {
      ultimoDebounce = millis();
      botaoAnterior  = leitura;
      return true;
    }
  }
  botaoAnterior = leitura;
  return false;
}

// ============================================================
// Formata metros para exibicao ex: 1.35 m
// ============================================================
String fmtMetros(float val) {
  return String(val, 2) + "m";
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);

  // LCD
  Wire.begin(21, 22);  // SDA=21, SCL=22
  lcd.init();
  lcd.backlight();

  // Encoder
  pinMode(ENCODER_A, INPUT);
  pinMode(ENCODER_B, INPUT);
  attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderISR, CHANGE);

  // Botao
  pinMode(BOTAO_PIN, INPUT_PULLUP);

  // LED
  pinMode(LED_STATUS, OUTPUT);

  // ----------------------------------------
  // Tela de boas-vindas
  // ----------------------------------------
  lcd_print("  Med. Linear   ", "  Iniciando...  ");
  delay(2000);
  lcd_print(" Uni Salesiano  ", "Eng. Computacao ");
  delay(2000);
  lcd_print("W.Lucas RA:4940 ", "J.Pedro RA:5751 ");
  delay(2000);

  estadoAtual    = TELA_INICIAL;
  telaAtualizada = false;

  Serial.println("Sistema pronto.");
  Serial.print("Dist por pulso: ");
  Serial.print(DIST_POR_PULSO_M * 1000.0, 4);
  Serial.println(" mm");
}

// ============================================================
// LOOP PRINCIPAL
// ============================================================
void loop() {

  char  tecla = teclado.getKey();
  bool  botao = botaoClicado();
  unsigned long agora = millis();

  // Heartbeat LED
  digitalWrite(LED_STATUS, (agora / 500) % 2);

  switch (estadoAtual) {

    // ========================================================
    case TELA_INICIAL:
      if (!telaAtualizada) {
        lcd_print("Pressione [#] ou", "botao p/ iniciar");
        telaAtualizada = true;
      }
      if (tecla == '#' || botao) {
        zerarEncoder();
        bufferTeclado  = "";
        telaAtualizada = false;
        estadoAtual    = MEDIR_COMPRIMENTO;
      }
      break;

    // ========================================================
    case MEDIR_COMPRIMENTO:
      // Atualiza display em tempo real
      if (agora - ultimoDisplay > 200) {
        float d = distanciaAtual();
        lcd_print("COMPRIMENTO:", String(d, 2) + "m [#]OK [*]Zer");
        ultimoDisplay = agora;
      }
      // Zerar medicao
      if (tecla == '*') {
        zerarEncoder();
        lcd_print("COMPRIMENTO:", "Zerado! Recomece");
        delay(800);
      }
      // Confirmar comprimento
      if (tecla == '#' || botao) {
        comprimento_m  = distanciaAtual();
        telaAtualizada = false;
        estadoAtual    = SALVA_COMPRIMENTO;
      }
      break;

    // ========================================================
    case SALVA_COMPRIMENTO:
      if (!telaAtualizada) {
        lcd_print("Comp: " + fmtMetros(comprimento_m), "OK! -> Largura");
        telaAtualizada = true;
        Serial.print("Comprimento salvo: ");
        Serial.println(comprimento_m, 3);
        delay(1800);
        zerarEncoder();
        bufferTeclado  = "";
        telaAtualizada = false;
        estadoAtual    = MEDIR_LARGURA;
      }
      break;

    // ========================================================
    case MEDIR_LARGURA:
      if (agora - ultimoDisplay > 200) {
        float d = distanciaAtual();
        lcd_print("LARGURA:", String(d, 2) + "m [#]OK [*]Zer");
        ultimoDisplay = agora;
      }
      if (tecla == '*') {
        zerarEncoder();
        lcd_print("LARGURA:", "Zerado! Recomece");
        delay(800);
      }
      if (tecla == '#' || botao) {
        largura_m      = distanciaAtual();
        telaAtualizada = false;
        estadoAtual    = SALVA_LARGURA;
      }
      break;

    // ========================================================
    case SALVA_LARGURA:
      if (!telaAtualizada) {
        lcd_print("Larg: " + fmtMetros(largura_m), "OK! -> Altura");
        telaAtualizada = true;
        Serial.print("Largura salva: ");
        Serial.println(largura_m, 3);
        delay(1800);
        bufferTeclado  = "";
        telaAtualizada = false;
        estadoAtual    = DIGITAR_ALTURA;
        lcd_print("ALTURA/PROF(cm):", "Digite: _       ");
      }
      break;

    // ========================================================
    // DIGITAR ALTURA pelo teclado (em centimetros)
    // Usuario digita ex: 8 = 8 cm | 12 = 12 cm | 5 = 5 cm
    // [*] apaga | [#] confirma | [botao] confirma
    // ========================================================
    case DIGITAR_ALTURA:
      if (tecla) {
        if (tecla >= '0' && tecla <= '9') {
          if (bufferTeclado.length() < 4) {
            bufferTeclado += tecla;
            lcd_print("ALTURA/PROF(cm):", "Digite: " + bufferTeclado + " cm    ");
          }
        }
        else if (tecla == '*') {
          if (bufferTeclado.length() > 0) {
            bufferTeclado.remove(bufferTeclado.length() - 1);
          }
          String exib = (bufferTeclado.length() > 0) ? bufferTeclado + " cm" : "_";
          lcd_print("ALTURA/PROF(cm):", "Digite: " + exib + "       ");
        }
        else if (tecla == '#') {
          if (bufferTeclado.length() > 0) {
            altura_m = bufferTeclado.toFloat() / 100.0;
            estadoAtual = CALCULAR;
          } else {
            lcd_print("ALTURA/PROF(cm):", "! Informe valor!");
            delay(1200);
            lcd_print("ALTURA/PROF(cm):", "Digite: _       ");
          }
        }
      }
      // Botao tambem confirma altura digitada
      if (botao) {
        if (bufferTeclado.length() > 0) {
          altura_m    = bufferTeclado.toFloat() / 100.0;
          estadoAtual = CALCULAR;
        } else {
          lcd_print("ALTURA/PROF(cm):", "! Informe valor!");
          delay(1200);
          lcd_print("ALTURA/PROF(cm):", "Digite: _       ");
        }
      }
      break;

    // ========================================================
    case CALCULAR:
      volume_m3 = comprimento_m * largura_m * altura_m;
      massa_kg  = volume_m3 * DENSIDADE_KG_M3;

      Serial.println("=========== RESULTADO ===========");
      Serial.print("Comprimento  : "); Serial.print(comprimento_m, 3); Serial.println(" m");
      Serial.print("Largura      : "); Serial.print(largura_m, 3);     Serial.println(" m");
      Serial.print("Altura/Prof  : "); Serial.print(altura_m * 100, 1);Serial.println(" cm");
      Serial.print("Volume       : "); Serial.print(volume_m3, 4);     Serial.println(" m3");
      Serial.print("Massa (~2560): "); Serial.print(massa_kg, 1);      Serial.println(" kg");
      if (massa_kg >= 1000) {
        Serial.print("Massa (ton)  : "); Serial.print(massa_kg/1000.0,3); Serial.println(" t");
      }
      Serial.println("=================================");

      telaVolume  = true;
      ultimaTroca = millis();
      estadoAtual = MOSTRAR_RESULTADO;
      break;

    // ========================================================
    // MOSTRAR RESULTADO
    // Alterna entre volume e massa a cada 3 segundos
    // [#] = nova medicao completa
    // [botao] = nova medicao completa
    // ========================================================
    case MOSTRAR_RESULTADO:
      if (agora - ultimaTroca > 3000) {
        if (telaVolume) {
          lcd_print("Vol:" + String(volume_m3, 3) + " m3",
                    "[#]Novo [*]Reset");
        } else {
          String mStr;
          if (massa_kg >= 1000)
            mStr = String(massa_kg / 1000.0, 2) + " t";
          else
            mStr = String(massa_kg, 1) + " kg";
          lcd_print("Massa: " + mStr, "[#]Novo [*]Reset");
        }
        telaVolume  = !telaVolume;
        ultimaTroca = agora;
      }

      // Nova medicao completa
      if (tecla == '#' || botao) {
        comprimento_m  = 0;
        largura_m      = 0;
        altura_m       = 0;
        bufferTeclado  = "";
        zerarEncoder();
        telaAtualizada = false;
        estadoAtual    = TELA_INICIAL;
      }

      // Reiniciar tudo
      if (tecla == '*') {
        comprimento_m  = 0;
        largura_m      = 0;
        altura_m       = 0;
        volume_m3      = 0;
        massa_kg       = 0;
        bufferTeclado  = "";
        zerarEncoder();
        telaAtualizada = false;
        lcd_print("  Reiniciando   ", "                ");
        delay(1000);
        estadoAtual    = TELA_INICIAL;
      }
      break;

  } // fim switch
}
// ============================================================
// FIM DO CODIGO
// ============================================================
