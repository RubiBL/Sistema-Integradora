#include <SoftwareSerial.h>

#define PIN_TX_FPGA 3
#define PIN_RX_FPGA 4
#define SERIAL_BAUD 9600
#define FPGA_BAUD 9600
#define MAX_BUFFER 128

SoftwareSerial SerialFPGA(PIN_RX_FPGA, PIN_TX_FPGA);  // RX, TX
char usbBuffer[MAX_BUFFER];
int usbIndex = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);
  SerialFPGA.begin(FPGA_BAUD);
  delay(100);

  Serial.println(F("=== Arduino puente USB -> FPGA listo ==="));
  Serial.println(F("Esperando mensajes desde el PC..."));
  SerialFPGA.println(F("READY"));
}

void loop() {
  processUsbInput();
  processFpgaInput();
}

void processUsbInput() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      if (usbIndex > 0) {
        usbBuffer[usbIndex] = '\0';
        forwardToFpga(usbBuffer);
        usbIndex = 0;
      }
    } else if (usbIndex < MAX_BUFFER - 1) {
      usbBuffer[usbIndex++] = c;
    }
  }
}

void processFpgaInput() {
  while (SerialFPGA.available() > 0) {
    char c = SerialFPGA.read();
    Serial.write(c);
  }
}

void forwardToFpga(const char* message) {
  Serial.print(F("[USB -> FPGA] "));
  Serial.println(message);
  SerialFPGA.println(message);
}
