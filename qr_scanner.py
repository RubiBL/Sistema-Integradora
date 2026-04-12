import cv2
import json
import requests
from pyzbar.pyzbar import decode
from crypto_utils import load_key, encrypt

# Cargar la clave de encriptación
CRYPTO_KEY = load_key()

# URL del servidor Flask (ajusta si es necesario)
SERVER_URL = 'http://localhost:5000/scan'

def main():
    # Inicializar la cámara
    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        print("Error: No se pudo abrir la cámara.")
        return

    print("Presiona 'q' para salir.")
    scanned_tokens = set()  # Para evitar procesar el mismo QR múltiples veces

    while True:
        ret, frame = cap.read()
        if not ret:
            print("Error: No se pudo leer el frame de la cámara.")
            break

        # Decodificar códigos QR en el frame
        decoded_objects = decode(frame)
        for obj in decoded_objects:
            token = obj.data.decode('utf-8')
            if token not in scanned_tokens:
                scanned_tokens.add(token)
                print(f"QR detectado: {token}")
                # Enviar a la ruta /scan
                response = send_to_server(token)
                print(f"Respuesta del servidor: {response}")

        # Mostrar el frame en una ventana flotante
        cv2.imshow('QR Scanner', frame)

        # Salir con 'q'
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    # Liberar recursos
    cap.release()
    cv2.destroyAllWindows()

def send_to_server(token):
    # Crear el payload JSON
    data = {'token': token}
    plaintext = json.dumps(data).encode('utf-8')

    # Encriptar el payload
    encrypted_b64 = encrypt(CRYPTO_KEY, plaintext)

    # Enviar POST a /scan
    try:
        response = requests.post(SERVER_URL, json={'payload': encrypted_b64}, timeout=10)
        if response.status_code == 200:
            return response.json()
        else:
            return {'error': f'HTTP {response.status_code}', 'details': response.text}
    except requests.RequestException as e:
        return {'error': 'Error de conexión', 'details': str(e)}

if __name__ == '__main__':
    main()