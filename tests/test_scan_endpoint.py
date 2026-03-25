import os
import base64
import json
import importlib
from cryptography.hazmat.primitives.ciphers.aead import AESGCM


def make_key_b64():
    return base64.b64encode(AESGCM.generate_key(bit_length=256)).decode()


def test_scan_accepts_encrypted_payload(monkeypatch):
    # 1) Provision a deterministic PSK via env before importing the app
    key_b64 = make_key_b64()
    monkeypatch.setenv('PSK_B64', key_b64)

    # 2) Import app AFTER setting PSK_B64 so CRYPTO_KEY in app is derived from it
    import importlib
    appmod = importlib.import_module('app')

    # 3) Build encrypted payload using same key
    key = base64.b64decode(key_b64)
    aesgcm = AESGCM(key)
    nonce = os.urandom(12)
    payload = json.dumps({'token': 'NOT-IN-DB-123'}).encode()
    ct = aesgcm.encrypt(nonce, payload, None)
    token_b64 = base64.b64encode(nonce + ct).decode()

    # 4) Use Flask test client to POST and assert we get a 404 (token won't exist in DB)
    client = appmod.app.test_client()
    resp = client.post('/scan', json={'payload': token_b64})
    assert resp.status_code == 404
    data = resp.get_json()
    assert data is not None
    assert data.get('error') == True or 'status' in data
