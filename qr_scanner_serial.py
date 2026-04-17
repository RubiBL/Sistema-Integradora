import argparse
import time
import cv2
from pyzbar.pyzbar import decode

try:
    import serial
except ImportError:
    serial = None


def parse_args():
    parser = argparse.ArgumentParser(
        description='Escanea QR y envía el token al Arduino por USB serial.'
    )
    parser.add_argument(
        '--serial-port',
        default=None,
        help='Puerto serial donde está conectado el Arduino (ej. COM3 o /dev/ttyUSB0).'
    )
    parser.add_argument(
        '--baud-rate',
        type=int,
        default=9600,
        help='Baud rate para la conexión con el Arduino. Por defecto 9600.'
    )
    parser.add_argument(
        '--camera',
        type=int,
        default=0,
        help='Índice de la cámara que se utilizará para leer códigos QR.'
    )
    return parser.parse_args()


def open_arduino(port: str, baud: int):
    if serial is None:
        raise RuntimeError('No se encontró la librería pyserial. Instala pyserial antes de ejecutar.')
    ser = serial.Serial(port, baud, timeout=1)
    print(f'Conectado al Arduino en {port} a {baud} baudios.')
    time.sleep(2)  # Esperar al Arduino para que inicie correctamente
    return ser


def send_to_arduino(token: str, ser):
    message = f'QR:{token}'
    ser.write((message + '\n').encode('utf-8'))
    print(f'Enviado al Arduino: {message}')

    # Leer cualquier respuesta inmediata del Arduino
    time.sleep(0.1)
    while ser.in_waiting > 0:
        response = ser.readline().decode('utf-8', errors='ignore').strip()
        if response:
            print(f'Respuesta Arduino: {response}')


def main():
    args = parse_args()
    serial_port = args.serial_port
    baud_rate = args.baud_rate

    if serial_port is None:
        print('Error: Debes pasar --serial-port para enviar mensajes al Arduino.')
        print('Ejemplo: python qr_scanner_serial.py --serial-port COM3')
        return

    ser = open_arduino(serial_port, baud_rate)
    cap = cv2.VideoCapture(args.camera)
    if not cap.isOpened():
        print('Error: No se pudo abrir la cámara.')
        return

    print('Presiona "q" para salir.')
    scanned_tokens = set()

    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                print('Error: No se pudo leer el frame de la cámara.')
                break

            decoded_objects = decode(frame)
            for obj in decoded_objects:
                token = obj.data.decode('utf-8')
                if token not in scanned_tokens:
                    scanned_tokens.add(token)
                    print(f'QR detectado: {token}')
                    send_to_arduino(token, ser)

            cv2.imshow('QR Scanner -> Arduino', frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
    except KeyboardInterrupt:
        print('\nInterrumpido por el usuario.')
    finally:
        cap.release()
        cv2.destroyAllWindows()
        if ser.is_open:
            ser.close()
            print('Puerto serial cerrado.')


if __name__ == '__main__':
    main()
