# Dispositivo Inteligente de Medição Linear e Cálculo de Área — EGC-2026-009

Trabalho de Conclusão de Curso de Engenharia da Computação, UniSALESIANO Araçatuba (2026).

**Integrantes:** Wendel Lucas Silveira Penteado (RA 214940) e João Pedro Rodrigues da Rocha (RA 215751).

## Descrição

Firmware para ESP32 com leitura de encoder incremental, teclado matricial 4×4 e display LCD I2C 16×2. O programa mede o comprimento e a largura pelo deslocamento da roda, recebe a altura/profundidade pelo teclado em centímetros e calcula o volume (`comprimento × largura × altura`) e a massa estimada (`volume × densidade de referência`). A versão enviada não exibe a área separadamente no LCD.

## Arquivos

- `EGC_2026_009_Firmware.ino`: código-fonte do protótipo, preparado para upload no Arduino IDE.

O arquivo `.ino` conserva a lógica original. Foi corrigida **somente a etiqueta de texto do monitor serial** de `Massa (~2350)` para `Massa (~2560)`, correspondente ao valor **já utilizado** pelo cálculo original. Nenhuma fórmula, pino ou rotina foi modificado.

## Hardware descrito no código

- ESP32;
- Encoder incremental E38S6G5-400BG24N, canais A (GPIO 34) e B (GPIO 35);
- Display LCD 16×2 I2C (SDA GPIO 21; SCL GPIO 22; endereço configurado `0x27`);
- Teclado matricial 4×4 (linhas 13, 12, 14, 27; colunas 26, 25, 33, 32);
- Botão de confirmação no GPIO 4;
- LED de status no GPIO 2;
- Alimentação com duas células 18650, BMS 2S e LM2596, conforme o relatório técnico.

## Como abrir e compilar

1. Instale o **Arduino IDE** e o pacote de placas **ESP32 by Espressif Systems** no Gerenciador de Placas.
2. Instale, pelo Gerenciador de Bibliotecas, bibliotecas compatíveis com `LiquidCrystal_I2C.h` e `Keypad.h`. A biblioteca `Wire.h` já acompanha a plataforma Arduino/ESP32.
3. Salve `EGC_2026_009_Firmware.ino` dentro de uma pasta chamada `EGC_2026_009_Firmware` e abra o arquivo no Arduino IDE.
4. Selecione o modelo correto de placa ESP32 e a porta serial correspondente. Verifique o endereço do LCD e a pinagem antes de enviar ao equipamento.
5. Compile e grave o firmware; para acompanhar as mensagens, use o Monitor Serial em **115200 baud**.

**Observação:** o nome exato e a versão das bibliotecas utilizadas pela equipe não constam no arquivo original. Antes de reproduzir o projeto, confirme com a equipe a seleção de bibliotecas e a configuração da placa. As instruções acima descrevem o procedimento de compilação, mas não significam que a compilação foi executada neste pacote.

## Parâmetros usados na versão recebida

- Encoder: `PPR = 400`, leitura por interrupção `CHANGE` no canal A;
- Parâmetro de conversão: `DIAMETRO_RODA_MM = 70.0` no firmware (diâmetro equivalente empregado na compensação das duas bordas do canal A, considerando roda de referência de 140 mm);
- Densidade de referência para massa: `DENSIDADE_KG_M3 = 2560.0` kg/m³, **parâmetro de cálculo**, não densidade determinada experimentalmente do material;
- O LCD apresenta comprimento, largura e, após digitação da altura, volume e massa estimada.

## Uso das teclas

- `#` ou botão: iniciar e confirmar etapas;
- `*`: zerar medição ou apagar último dígito, conforme a etapa;
- `0` a `9`: digitar altura/profundidade em centímetros.
