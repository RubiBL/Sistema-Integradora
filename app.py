import os
import sqlite3
import uuid
import qrcode
from datetime import datetime, timedelta
from flask import Flask, render_template, request, jsonify, send_file, url_for
from io import BytesIO

app = Flask(__name__)
DB_PATH = 'parking_system.db'

def get_db_connection():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

@app.route('/')
def index():
    return render_template('index.html')

# --- USER ROUTES ---
@app.route('/user/<int:user_id>')
def user_dashboard(user_id):
    conn = get_db_connection()
    user = conn.execute('SELECT * FROM users WHERE id = ?', (user_id,)).fetchone()
    vehicle = conn.execute('SELECT * FROM vehicles WHERE user_id = ?', (user_id,)).fetchone()
    
    # Get today's codes for limit check
    today = datetime.now().strftime('%Y-%m-%d')
    codes_today = conn.execute(
        "SELECT count(*) as count FROM qr_codes WHERE user_id = ? AND date(created_at) = ?", 
        (user_id, today)
    ).fetchone()['count']
    
    # Get history of logs for this user
    logs = conn.execute('''
        SELECT al.*, q.token 
        FROM access_logs al 
        JOIN qr_codes q ON al.qr_id = q.id 
        WHERE q.user_id = ? 
        ORDER BY al.timestamp DESC
    ''', (user_id,)).fetchall()
    
    conn.close()
    return render_template('user.html', user=user, vehicle=vehicle, codes_today=codes_today, logs=logs)

@app.route('/generate_qr/<int:user_id>', methods=['POST'])
def generate_qr(user_id):
    conn = get_db_connection()
    
    # Check limit: 2 per day
    today = datetime.now().strftime('%Y-%m-%d')
    count = conn.execute(
        "SELECT count(*) as count FROM qr_codes WHERE user_id = ? AND date(created_at) = ?", 
        (user_id, today)
    ).fetchone()['count']
    
    if count >= 2:
        conn.close()
        return jsonify({'error': 'Límite de 2 códigos por día alcanzado.'}), 400
    
    token = str(uuid.uuid4())
    conn.execute('INSERT INTO qr_codes (user_id, token) VALUES (?, ?)', (user_id, token))
    conn.commit()
    conn.close()
    
    return jsonify({'token': token})

@app.route('/qr_image/<token>')
def qr_image(token):
    img = qrcode.make(token)
    buf = BytesIO()
    img.save(buf, format='PNG')
    buf.seek(0)
    return send_file(buf, mimetype='image/png')

# --- SECURITY ROUTES ---
@app.route('/security')
def security_dashboard():
    return render_template('security.html')

@app.route('/scan', methods=['POST'])
def scan_qr():
    token = request.json.get('token')
    if not token:
        return jsonify({'error': 'Token no proporcionado'}), 400
    
    conn = get_db_connection()
    qr = conn.execute('SELECT * FROM qr_codes WHERE token = ?', (token,)).fetchone()
    
    if not qr:
        conn.close()
        return jsonify({'error': 'Código QR no válido'}), 404
    
    # Check expiration (24h)
    created_at = datetime.strptime(qr['created_at'], '%Y-%m-%d %H:%M:%S')
    if datetime.now() > created_at + timedelta(hours=24):
        conn.close()
        return jsonify({'error': 'Código QR expirado (más de 24h)'}), 401
    
    # Check current status
    logs = conn.execute('SELECT type FROM access_logs WHERE qr_id = ? ORDER BY timestamp ASC', (qr['id'],)).fetchall()
    log_types = [l['type'] for l in logs]
    
    if not log_types:
        # First scan -> Entry
        conn.execute('INSERT INTO access_logs (qr_id, type) VALUES (?, ?)', (qr['id'], 'ENTRY'))
        status = 'ENTRADA REGISTRADA'
    elif 'EXIT' not in log_types:
        # Second scan -> Exit
        conn.execute('INSERT INTO access_logs (qr_id, type) VALUES (?, ?)', (qr['id'], 'EXIT'))
        status = 'SALIDA REGISTRADA'
    else:
        # Already has both -> Expired use
        conn.close()
        return jsonify({'error': 'Este código ya ha sido utilizado para entrada y salida'}), 401

    conn.commit()
    
    # Get user and vehicle info for response
    user_info = conn.execute('''
        SELECT u.name, v.plate, v.model 
        FROM users u 
        JOIN vehicles v ON u.id = v.user_id 
        WHERE u.id = ?
    ''', (qr['user_id'],)).fetchone()
    
    conn.close()
    return jsonify({
        'status': status,
        'user': user_info['name'],
        'plate': user_info['plate'],
        'model': user_info['model']
    })

# --- ADMIN ROUTES ---
@app.route('/admin')
def admin_dashboard():
    conn = get_db_connection()
    # Join everything for the final report
    reports = conn.execute('''
        SELECT u.name, v.plate, v.model, al.type, al.timestamp
        FROM access_logs al
        JOIN qr_codes q ON al.qr_id = q.id
        JOIN users u ON q.user_id = u.id
        JOIN vehicles v ON u.id = v.user_id
        ORDER BY al.timestamp DESC
    ''').fetchall()
    conn.close()
    return render_template('admin.html', reports=reports)

if __name__ == '__main__':
    # Ensure DB exists
    if not os.path.exists(DB_PATH):
        print("Creando base de datos...")
        # (Could call init_db locally or just assume it was run)
    app.run(debug=True, host='0.0.0.0')
