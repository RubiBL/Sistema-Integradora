import serial
import time
import subprocess
import sys
import os

# Configuración del puerto serial (ajusta según tu sistema)
# En Windows, podría ser 'COM3', 'COM4', etc.
# En Linux/Mac, '/dev/ttyUSB0', etc.
SERIAL_PORT = 'COM3'  # Cambia esto al puerto correcto del FPGA
BAUD_RATE = 9600

def main():
    try:
        # Abrir puerto serial
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Conectado al puerto {SERIAL_PORT} a {BAUD_RATE} baud.")
        print("Esperando respuesta del FPGA...")

        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8').strip()
                print(f"Recibido del FPGA: {line}")

                if line == "OK":
                    print("Acceso autorizado por FPGA. Iniciando escáner QR...")
                    # Lanzar el escáner QR
                    try:
                        # Ejecutar qr_scanner.py
                        result = subprocess.run([sys.executable, 'qr_scanner.py'], cwd=os.getcwd())
                        print("Escáner QR terminado.")
                    except Exception as e:
                        print(f"Error al ejecutar qr_scanner.py: {e}")
                elif line == "DENY":
                    print("Acceso denegado por FPGA.")
                # Ignorar otros mensajes como "READY" o "UID:..."

            time.sleep(0.1)  # Pequeña pausa para no saturar CPU

    except serial.SerialException as e:
        print(f"Error de puerto serial: {e}")
        print("Asegúrate de que el puerto esté correcto y el FPGA esté conectado.")
    except KeyboardInterrupt:
        print("Interrumpido por usuario.")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("Puerto serial cerrado.")

if __name__ == '__main__':
    main()