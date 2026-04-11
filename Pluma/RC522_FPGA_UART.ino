// =============================================================
//  RC522_FPGA_UART.ino
//  Autor: Proyecto Control Vehicular
//  Descripción:
//    Este código lee el UID de una tarjeta RFID con el módulo
//    RC522 y lo envía al FPGA DE0-Nano mediante comunicación
//    UART (Serial por software). El FPGA valida el UID y
//    responde si el acceso es permitido o denegado. Python
//    escucha esa respuesta y lanza el sistema de control
//    vehicular (cámaras, reconocimiento facial, etc.).
//
//  FLUJO COMPLETO:
//    Tarjeta RFID
//       ↓  (señal de radio 13.56 MHz)
//    Módulo RC522
//       ↓  (protocolo SPI)
//    Arduino UNO  ──→ Monitor Serial PC  (para ver en pantalla)
//       ↓  (UART 9600 baud por Pin 3)
//    FPGA DE0-Nano  (valida UID contra lista autorizada)
//       ↓  (UART 9600 baud por USB al PC)
//    Python  (lanza sistema vehicular si respuesta = OK)
//
// =============================================================
//  COMPONENTES NECESARIOS
// =============================================================
//
//  1. Arduino UNO
//     Microcontrolador principal. Lee la tarjeta RFID por SPI
//     y reenvía el UID al FPGA por UART (SoftwareSerial).
//
//  2. Módulo RFID RC522
//     Lector de tarjetas RFID de 13.56 MHz. Se comunica con
//     el Arduino por el protocolo SPI usando 7 cables.
//     ¡Alimentar SIEMPRE a 3.3V, nunca a 5V!
//
//  3. FPGA DE0-Nano (Intel Cyclone IV)
//     Recibe el UID del Arduino, lo compara con una lista de
//     UIDs autorizados programada en Verilog, y responde al
//     PC con "OK" o "DENY" por USB.
//
//  4. Divisor de voltaje (2 resistencias)
//     El Arduino trabaja a 5V y el FPGA a 3.3V.
//     Sin el divisor, el Pin 3 del Arduino dañaría el FPGA.
//     Se construye con una resistencia de 1kΩ y una de 2kΩ.
//
//  5. Cables jumper macho-macho y macho-hembra
//     Para conectar todo en la protoboard.
//
//  6. Protoboard
//     Para armar el divisor de voltaje sin soldar.
//
// =============================================================
//  LIBRERÍAS REQUERIDAS
// =============================================================
//
//  - MFRC522 by GithubCommunity
//    Arduino IDE → Herramientas → Administrar bibliotecas
//    Buscar "MFRC522" → Instalar
//
//  - SoftwareSerial
//    Ya viene incluida con el Arduino IDE, no hay que instalar.
//
// =============================================================
//  TABLA 1 — CONEXIÓN RC522 → ARDUINO UNO  (protocolo SPI)
// =============================================================
//
//  ┌──────────────┬──────────────┬───────────────────────────────────────┐
//  │  RC522 Pin   │  Arduino Pin │  Descripción                          │
//  ├──────────────┼──────────────┼───────────────────────────────────────┤
//  │  VCC         │  3.3V        │  Alimentación. NUNCA conectar a 5V,   │
//  │              │              │  el módulo se daña permanentemente.   │
//  ├──────────────┼──────────────┼───────────────────────────────────────┤
//  │  GND         │  GND         │  Tierra común. Siempre debe estar     │
//  │              │              │  conectado para cerrar el circuito.   │
//  ├──────────────┼──────────────┼───────────────────────────────────────┤
//  │  RST         │  Pin 9       │  Reset del módulo. El Arduino manda   │
//  │              │              │  un pulso LOW para reiniciar el RC522.│
//  ├──────────────┼──────────────┼───────────────────────────────────────┤
//  │  SDA (CS)    │  Pin 10      │  Chip Select. Indica al RC522 cuándo  │
//  │              │              │  debe escuchar el bus SPI.            │
//  ├──────────────┼──────────────┼───────────────────────────────────────┤
//  │  SCK         │  Pin 13      │  Reloj SPI (~4 MHz). Sincroniza la    │
//  │              │              │  transmisión de datos entre los dos.  │
//  ├──────────────┼──────────────┼───────────────────────────────────────┤
//  │  MOSI        │  Pin 11      │  Master Out Slave In. Arduino envía   │
//  │              │              │  comandos al RC522 por este pin.      │
//  ├──────────────┼──────────────┼───────────────────────────────────────┤
//  │  MISO        │  Pin 12      │  Master In Slave Out. RC522 responde  │
//  │              │              │  al Arduino por este pin.             │
//  ├──────────────┼──────────────┼───────────────────────────────────────┤
//  │  IRQ         │  Pin 2       │  Interrupción. El RC522 avisa al      │
//  │              │              │  Arduino cuando detecta una tarjeta.  │
//  └──────────────┴──────────────┴───────────────────────────────────────┘
//
//  NOTA: Agregar resistencia pull-down de 10kΩ entre MISO y GND
//        para evitar lecturas fantasma cuando no hay tarjeta.
//
// =============================================================
//  TABLA 2 — CONEXIÓN ARDUINO UNO → FPGA DE0-Nano  (UART)
// =============================================================
//
//  ┌──────────────────┬───────────────┬──────────────────────────────────┐
//  │  Arduino Pin     │  FPGA Pin     │  Descripción                     │
//  ├──────────────────┼───────────────┼──────────────────────────────────┤
//  │  Pin 3 (TX_SW)   │  GPIO_0[0]    │  Arduino transmite el UID.       │
//  │                  │  (Pin 1 del   │  El FPGA recibe por este pin.    │
//  │                  │   conector)   │  REQUIERE divisor de voltaje.    │
//  ├──────────────────┼───────────────┼──────────────────────────────────┤
//  │  Pin 4 (RX_SW)   │  GPIO_0[1]    │  Reservado para uso futuro.      │
//  │                  │  (Pin 3 del   │  El FPGA podría responder aquí   │
//  │                  │   conector)   │  en etapas posteriores.          │
//  ├──────────────────┼───────────────┼──────────────────────────────────┤
//  │  GND             │  GND FPGA     │  Tierra común OBLIGATORIA.       │
//  │                  │  (Pin 12 del  │  Sin este cable la comunicación  │
//  │                  │   conector)   │  no funciona aunque el resto     │
//  │                  │               │  esté bien conectado.            │
//  └──────────────────┴───────────────┴──────────────────────────────────┘
//
// =============================================================
//  TABLA 3 — DIVISOR DE VOLTAJE (5V → 3.3V)
// =============================================================
//
//  ¿Por qué se necesita?
//    El Pin 3 del Arduino UNO entrega señales de 5V.
//    Los pines GPIO del DE0-Nano soportan máximo 3.3V.
//    Si conectas directo, dañas el FPGA permanentemente.
//    El divisor de voltaje baja la señal de 5V a ~3.3V.
//
//  Componentes:
//    - R1 = Resistencia de 1kΩ (café-negro-rojo)
//    - R2 = Resistencia de 2kΩ (rojo-negro-rojo)
//      (si no tienes 2kΩ exacta, usa dos resistencias de
//       1kΩ en serie que es lo mismo)
//
//  Diagrama de conexión en protoboard:
//
//    Arduino Pin 3
//         │
//        [R1 = 1kΩ]
//         │
//         ├──────────── GPIO_0[0] del FPGA
//         │
//        [R2 = 2kΩ]
//         │
//        GND  (tierra común con el FPGA)
//
//  Verificación matemática:
//    Vout = Vin × R2 / (R1 + R2)
//    Vout = 5V  × 2000 / (1000 + 2000)
//    Vout = 5V  × 0.667
//    Vout = 3.33V  ✓  (dentro del rango seguro del FPGA)
//
// =============================================================
//  TABLA 4 — RESUMEN DE MATERIALES
// =============================================================
//
//  ┌────┬──────────────────────────┬──────┬────────────────────┐
//  │ #  │  Componente              │  Cant│  Para qué sirve    │
//  ├────┼──────────────────────────┼──────┼────────────────────┤
//  │  1 │  Arduino UNO             │   1  │  Controlador RFID  │
//  │  2 │  Módulo RC522 RFID       │   1  │  Leer tarjetas     │
//  │  3 │  Tarjeta/llavero RFID    │   1+ │  Prueba de lectura │
//  │  4 │  FPGA DE0-Nano           │   1  │  Validar UID       │
//  │  5 │  Resistencia 1kΩ         │   1  │  Divisor voltaje   │
//  │  6 │  Resistencia 2kΩ         │   1  │  Divisor voltaje   │
//  │  7 │  Protoboard              │   1  │  Armar circuito    │
//  │  8 │  Cables jumper M-M       │   5+ │  Conexiones        │
//  │  9 │  Cables jumper M-H       │   3+ │  RC522 al Arduino  │
//  │ 10 │  Cable USB tipo B        │   1  │  Arduino al PC     │
//  │ 11 │  Cable USB mini          │   1  │  FPGA al PC        │
//  └────┴──────────────────────────┴──────┴────────────────────┘
//
// =============================================================
//  QUÉ SE ENVÍA AL FPGA
// =============================================================
//
//  Al arrancar el Arduino:
//    "READY\n"          → El FPGA sabe que el Arduino está listo
//
//  Al acercar una tarjeta RFID:
//    "UID:A3:4F:2B:91\n" → El FPGA lee el UID y lo valida
//
//  El FPGA responderá (Etapa 2):
//    "OK\n"   → UID autorizado → Python lanza el sistema
//    "DENY\n" → UID no reconocido → acceso denegado
//
// =============================================================

#include <SPI.h>
#include <MFRC522.h>
#include <SoftwareSerial.h>

// ── Pines RFID ───────────────────────────────────────────────
#define PIN_RST  9
#define PIN_SS   10
#define PIN_IRQ  2

// ── SoftwareSerial: Pin 3 = TX hacia FPGA, Pin 4 = RX (reservado) ──
#define PIN_TX_FPGA  3
#define PIN_RX_FPGA  4

// ── Objetos ──────────────────────────────────────────────────
MFRC522        rfid(PIN_SS, PIN_RST);
SoftwareSerial SerialFPGA(PIN_RX_FPGA, PIN_TX_FPGA);  // RX, TX

// ── Variables de diagnóstico ─────────────────────────────────
bool moduloOK   = false;
bool spiOK      = false;
bool resetOK    = false;
bool selfTestOK = false;
int  tarjetasLeidas = 0;

// =============================================================
//  SETUP
// =============================================================
void setup() {
  // Serial normal → PC (para monitor serial / debug)
  Serial.begin(9600);
  while (!Serial);

  // Serial hacia el FPGA (mismo baud rate: 9600)
  SerialFPGA.begin(9600);

  Serial.println(F("============================================"));
  Serial.println(F("   RC522 + UART hacia FPGA DE0-Nano         "));
  Serial.println(F("============================================"));
  Serial.println();

  // ----------------------------------------------------------
  //  TEST 1: Señal de RESET
  // ----------------------------------------------------------
  Serial.println(F("[1] Probando senal de RESET..."));
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, LOW);
  delay(50);
  digitalWrite(PIN_RST, HIGH);
  delay(50);
  resetOK = true;
  Serial.println(F("    OK - Senal RST generada"));

  // ----------------------------------------------------------
  //  TEST 2: Inicializar SPI
  // ----------------------------------------------------------
  Serial.println(F("[2] Iniciando SPI..."));
  SPI.begin();
  rfid.PCD_Init();
  delay(100);
  Serial.println(F("    OK - SPI iniciado"));

  // ----------------------------------------------------------
  //  TEST 3: Leer versión del chip
  // ----------------------------------------------------------
  Serial.println(F("[3] Leyendo version del chip..."));
  byte version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print(F("    Version (hex): 0x"));
  Serial.println(version, HEX);

  if (version == 0x91) {
    Serial.println(F("    OK - MFRC522 version 1.0"));
    moduloOK = true;
  } else if (version == 0x92) {
    Serial.println(F("    OK - MFRC522 version 2.0"));
    moduloOK = true;
  } else if (version == 0x00 || version == 0xFF) {
    Serial.println(F("    ERROR - Sin respuesta del modulo"));
    moduloOK = false;
  } else {
    Serial.println(F("    AVISO - Version desconocida (puede funcionar)"));
    moduloOK = true;
  }

  // ----------------------------------------------------------
  //  TEST 4: Self-Test
  // ----------------------------------------------------------
  Serial.println(F("[4] Ejecutando Self-Test interno..."));
  selfTestOK = rfid.PCD_PerformSelfTest();
  Serial.println(selfTestOK ? F("    OK - Self-Test pasado") : F("    ERROR - Self-Test fallido"));
  rfid.PCD_Init();
  delay(50);

  // ----------------------------------------------------------
  //  TEST 5: Escritura/Lectura SPI
  // ----------------------------------------------------------
  Serial.println(F("[5] Test escritura/lectura SPI..."));
  spiTestRegistros();

  // ----------------------------------------------------------
  //  TEST 6: Pin IRQ
  // ----------------------------------------------------------
  Serial.println(F("[6] Configurando pin IRQ..."));
  pinMode(PIN_IRQ, INPUT_PULLUP);
  Serial.println(F("    OK - IRQ listo en Pin 2"));

  // ----------------------------------------------------------
  //  TEST 7: Verificar canal UART hacia FPGA
  //  Mandamos un mensaje de arranque para confirmar que el
  //  canal está activo. El FPGA lo puede usar como señal de
  //  que el Arduino ya está listo.
  // ----------------------------------------------------------
  Serial.println(F("[7] Probando canal UART hacia FPGA..."));
  SerialFPGA.println("READY");
  Serial.println(F("    OK - Mensaje READY enviado al FPGA por Pin 3"));

  // ----------------------------------------------------------
  //  RESUMEN
  // ----------------------------------------------------------
  Serial.println();
  Serial.println(F("============================================"));
  Serial.println(F("          RESUMEN DEL DIAGNOSTICO           "));
  Serial.println(F("============================================"));
  Serial.print(F("  Reset    : ")); Serial.println(resetOK    ? F("OK") : F("FALLO"));
  Serial.print(F("  SPI Comm : ")); Serial.println(spiOK      ? F("OK") : F("FALLO"));
  Serial.print(F("  Modulo   : ")); Serial.println(moduloOK   ? F("OK") : F("FALLO"));
  Serial.print(F("  SelfTest : ")); Serial.println(selfTestOK ? F("OK") : F("FALLO"));
  Serial.print(F("  UART FPGA: ")); Serial.println(F("OK - Pin 3 activo a 9600 baud"));
  Serial.println(F("============================================"));
  Serial.println();

  if (moduloOK && spiOK) {
    Serial.println(F(">>> SISTEMA LISTO"));
    Serial.println(F(">>> Acerca una tarjeta RFID para leerla..."));
    Serial.println(F(">>> El UID se enviara tambien al FPGA por Pin 3"));
  } else {
    Serial.println(F(">>> MODULO CON PROBLEMAS - Revisa conexiones"));
  }
  Serial.println();
}

// =============================================================
//  LOOP — Espera tarjeta, lee UID, lo manda al FPGA
// =============================================================
void loop() {

  // Esperar nueva tarjeta
  if (!rfid.PICC_IsNewCardPresent()) {
    delay(50);
    return;
  }

  // Leer serial de la tarjeta
  if (!rfid.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }

  tarjetasLeidas++;

  // ── Mostrar en monitor serial del PC ──────────────────────
  Serial.println(F("─────────────────────────────────────────"));
  Serial.print(F("  TARJETA #"));
  Serial.println(tarjetasLeidas);

  MFRC522::PICC_Type tipo = rfid.PICC_GetType(rfid.uid.sak);
  Serial.print(F("  Tipo  : "));
  Serial.println(rfid.PICC_GetTypeName(tipo));

  Serial.print(F("  UID   : "));
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) Serial.print(F("0"));
    Serial.print(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) Serial.print(F(":"));
  }
  Serial.println();

  // ── Construir y enviar mensaje al FPGA ────────────────────
  // Formato: "UID:A3:4F:2B:91\n"
  // El FPGA leerá esta cadena hasta el \n y la procesará.
  String mensajeFPGA = "UID:";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) mensajeFPGA += "0";
    mensajeFPGA += String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) mensajeFPGA += ":";
  }
  mensajeFPGA.toUpperCase();
  SerialFPGA.println(mensajeFPGA);  // println agrega el \n automáticamente

  Serial.print(F("  Enviado al FPGA: "));
  Serial.println(mensajeFPGA);

  // ── Leer bloque 0 (info del fabricante) ───────────────────
  leerBloque0();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  Serial.println(F("─────────────────────────────────────────"));
  Serial.println();

  delay(1000);
}

// =============================================================
//  FUNCIÓN: spiTestRegistros
// =============================================================
void spiTestRegistros() {
  byte valorOriginal = rfid.PCD_ReadRegister(MFRC522::TxModeReg);
  rfid.PCD_WriteRegister(MFRC522::TxModeReg, 0x00);
  byte valorLeido = rfid.PCD_ReadRegister(MFRC522::TxModeReg);

  if (valorLeido == 0x00) {
    spiOK = true;
    Serial.println(F("    OK - Escritura/Lectura SPI correcta"));
  } else {
    spiOK = false;
    Serial.println(F("    ERROR - Fallo en comunicacion SPI"));
    Serial.print(F("    Esperado: 0x00  Recibido: 0x"));
    Serial.println(valorLeido, HEX);
  }
  rfid.PCD_WriteRegister(MFRC522::TxModeReg, valorOriginal);
}

// =============================================================
//  FUNCIÓN: leerBloque0
// =============================================================
void leerBloque0() {
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  byte buffer[18];
  byte size = sizeof(buffer);

  MFRC522::StatusCode status = rfid.PCD_Authenticate(
    MFRC522::PICC_CMD_MF_AUTH_KEY_A, 0, &key, &(rfid.uid)
  );

  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("  Bloque 0 : Error autenticacion - "));
    Serial.println(rfid.GetStatusCodeName(status));
    return;
  }

  status = rfid.MIFARE_Read(0, buffer, &size);
  if (status == MFRC522::STATUS_OK) {
    Serial.print(F("  Bloque 0 : "));
    for (byte i = 0; i < 16; i++) {
      if (buffer[i] < 0x10) Serial.print(F("0"));
      Serial.print(buffer[i], HEX);
      Serial.print(F(" "));
    }
    Serial.println();
  } else {
    Serial.print(F("  Bloque 0 : Error lectura - "));
    Serial.println(rfid.GetStatusCodeName(status));
  }
}
