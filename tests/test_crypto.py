import os
import base64
from crypto_utils import generate_and_store_key, load_key, encrypt, decrypt


def test_encrypt_decrypt_roundtrip(tmp_path, monkeypatch):
    # Ensure a fresh key file in tmp path to avoid touching repo files
    key_path = tmp_path / 'secret.key'
    # generate key and save to the temp location
    key = generate_and_store_key(str(key_path))

    # Monkeypatch KEY_FILE location by setting PSK_B64 to the base64 of our key
    monkeypatch.setenv('PSK_B64', base64.b64encode(key).decode())

    # load_key should now return the same key
    loaded = load_key()
    assert loaded == key

    # Encrypt and decrypt
    plaintext = b'hello-encrypted-world'
    token = encrypt(loaded, plaintext)
    out = decrypt(loaded, token)
    assert out == plaintext
