import sqlite3
import os

DB_PATH = 'parking_system.db'

def init_db():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()

    # Table for Users
    cursor.execute('''
    CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL,
        email TEXT UNIQUE NOT NULL,
        role TEXT CHECK(role IN ('user', 'security', 'admin')) NOT NULL
    )
    ''')

    # Table for Vehicles
    cursor.execute('''
    CREATE TABLE IF NOT EXISTS vehicles (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER,
        plate TEXT UNIQUE NOT NULL,
        model TEXT,
        color TEXT,
        FOREIGN KEY (user_id) REFERENCES users (id)
    )
    ''')

    # Table for QR Codes
    cursor.execute('''
    CREATE TABLE IF NOT EXISTS qr_codes (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER,
        token TEXT UNIQUE NOT NULL,
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY (user_id) REFERENCES users (id)
    )
    ''')

    # Table for Access Logs (Entry/Exit)
    cursor.execute('''
    CREATE TABLE IF NOT EXISTS access_logs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        qr_id INTEGER,
        type TEXT CHECK(type IN ('ENTRY', 'EXIT')) NOT NULL,
        timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY (qr_id) REFERENCES qr_codes (id)
    )
    ''')

    # Seed data
    users_data = [
        ('Juan Perez', 'juan@utaltamira.edu.mx', 'user'),
        ('Maria Garcia', 'maria@utaltamira.edu.mx', 'user'),
        ('Guardia 1', 'guardia1@utaltamira.edu.mx', 'security'),
        ('Admin Root', 'admin@utaltamira.edu.mx', 'admin')
    ]
    cursor.executemany('INSERT OR IGNORE INTO users (name, email, role) VALUES (?, ?, ?)', users_data)

    vehicles_data = [
        (1, 'ABC-1234', 'Toyota Corolla', 'Blanco'),
        (2, 'XYZ-9876', 'Nissan Sentra', 'Gris')
    ]
    cursor.executemany('INSERT OR IGNORE INTO vehicles (user_id, plate, model, color) VALUES (?, ?, ?, ?)', vehicles_data)

    conn.commit()
    conn.close()
    print("Base de datos inicializada correctamente.")

if __name__ == '__main__':
    init_db()
