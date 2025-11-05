from flask import Flask, request, jsonify
import cv2
import numpy as np
import face_recognition
import os
import requests
from time import time
import socket
from waitress import serve

app = Flask(__name__)

# ================== Konfigurasi ==================
# Ganti sesuai host Laravel Anda
LARAVEL_URL = "http://10.110.171.150:8000/api/attendance/tap"
DEVICE_ID = "ESP32_001"
POST_TIMEOUT = 5           # detik
DEBOUNCE_SECONDS = 3       # cegah posting berulang wajah sama terlalu cepat

# Folder database wajah (nama file = face_uid, contoh: ndul1.jpg)
IMAGES_PATH = "ImagesBasic"
# =================================================

images = []
classNames = []  # ini akan berisi face_uid dari nama file
last_post_time_by_face = {}

# Inisialisasi folder & muat gambar
if not os.path.exists(IMAGES_PATH):
    os.makedirs(IMAGES_PATH)
    print(f"📁 Folder '{IMAGES_PATH}' dibuat. Masukkan foto wajah ke dalam folder ini.")
else:
    print(f"📁 Memuat data dari folder: {IMAGES_PATH}")

for file in os.listdir(IMAGES_PATH):
    img_path = os.path.join(IMAGES_PATH, file)
    img = cv2.imread(img_path)
    if img is None:
        continue
    images.append(img)
    classNames.append(os.path.splitext(file)[0])

print(f"✅ Ditemukan {len(images)} wajah dikenal: {classNames}")

def findEncodings(images_list):
    encodeList = []
    for img in images_list:
        img_rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        encodes = face_recognition.face_encodings(img_rgb)
        if len(encodes) > 0:
            encodeList.append(encodes[0])
    return encodeList

encodeListKnown = findEncodings(images)
print("🔍 Encoding wajah selesai!")

def post_attendance_face(face_uid: str):
    now = time()
    last_ts = last_post_time_by_face.get(face_uid, 0)
    if now - last_ts < DEBOUNCE_SECONDS:
        return  # skip untuk mencegah duplikasi terlalu cepat
    last_post_time_by_face[face_uid] = now

    payload = {
        "device_id": DEVICE_ID,
        "face_uid": face_uid
    }
    try:
        r = requests.post(LARAVEL_URL, json=payload, timeout=POST_TIMEOUT)
        print("Laravel response:", r.status_code, r.text)
    except Exception as e:
        print("❌ Post to Laravel failed:", e)

@app.route("/recognize", methods=["POST"])
def recognize():
    try:
        # Baca data mentah JPEG dari ESP32-CAM
        file_bytes = np.frombuffer(request.data, np.uint8)
        img = cv2.imdecode(file_bytes, cv2.IMREAD_COLOR)

        if img is None:
            return jsonify({"error": "Gambar tidak valid"}), 400

        # Deteksi dan encoding wajah
        faces = face_recognition.face_locations(img)
        encodings = face_recognition.face_encodings(img, faces)

        recognized_names = []
        for encodeFace, _ in zip(encodings, faces):
            matches = face_recognition.compare_faces(encodeListKnown, encodeFace)
            faceDis = face_recognition.face_distance(encodeListKnown, encodeFace)
            if len(faceDis) == 0:
                recognized_names.append("Unknown")
                continue
            matchIndex = np.argmin(faceDis)
            if matches[matchIndex]:
                name = classNames[matchIndex]  # ini adalah face_uid sesuai nama file
                recognized_names.append(name)
                # Kirim ke Laravel untuk tiap wajah yang dikenali
                post_attendance_face(name)
            else:
                recognized_names.append("Unknown")

        print(f"📸 Wajah terdeteksi: {recognized_names}")
        return jsonify({"recognized": recognized_names})
    except Exception as e:
        print("❌ Error:", str(e))
        return jsonify({"error": str(e)}), 500

if __name__ == "__main__":
    # Ambil IP lokal dari Wi-Fi untuk informasi
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
    except Exception:
        local_ip = "127.0.0.1"

    print("🚀 Menjalankan Face Recognition Server...")
    print(f"🌐 Akses endpoint: http://{local_ip}:5000/recognize")
    print("📡 Siap menerima data dari ESP32-CAM!")

    serve(app, host="0.0.0.0", port=5000)