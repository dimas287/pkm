from cvlib.object_detection import YOLO
import cv2
import time
import serial

# Inisialisasi komunikasi serial dengan Arduino (ganti 'COM3' atau '/dev/ttyUSB0' sesuai dengan port Anda)
# arduino = serial.Serial('/dev/ttyACM0', 9600, timeout=1)
time.sleep(2)  # Beri waktu untuk memastikan serial terhubung

# Inisialisasi kamera
cap = cv2.VideoCapture(0)  # Ganti 0 dengan 1 jika perlu
weights = "/home/aurora/yolov4tinyrpi4-main/yolov4-tiny-custom_last.weights"
config = "/home/aurora/yolov4tinyrpi4-main/yolov4-tiny-custom.cfg"
labels = "/home/aurora/yolov4tinyrpi4-main/obj.names"
count = 0
frame_skip = 5  # Mengatur berapa banyak frame yang akan dilewati
resize_width = 240  # Ukuran lebih kecil untuk pemrosesan lebih cepat
resize_height = 180

# Inisialisasi YOLO hanya satu kali
print("[INFO] Initializing YOLO ...")
yolo = YOLO(weights, config, labels)

# Fungsi untuk menggerakkan kamera berdasarkan lokasi bola
def move_camera(balls_positions, width, bbox):
    # Definisikan batas untuk masing-masing kotak
    section_0_left = width // 4        # Kotak 0 (kiri)
    section_1_right = width // 2        # Kotak 1 (tengah kiri)
    section_2_left = section_1_right    # Kotak 2 (tengah kanan)
    section_2_right = (3 * width) // 4  # Kotak 2 (tengah kanan)
    
    # Variabel untuk menyimpan status deteksi bola di tiap bagian
    detected_sections = [False, False, False, False]

    # Cek bounding box dari bola
    for ball in bbox:
        ball_left, ball_top, ball_right, ball_bottom = ball

        # Tentukan di bagian mana bola berada
        if ball_right > section_0_left and ball_left < section_1_right:
            detected_sections[1] = True  # Kotak 1 terdeteksi
        if ball_right > section_1_right and ball_left < section_2_right:
            detected_sections[2] = True  # Kotak 2 terdeteksi
        if ball_left >= section_2_right:
            detected_sections[3] = True  # Kotak 3 terdeteksi
        if ball_right <= section_0_left:
            detected_sections[0] = True  # Kotak 0 terdeteksi

    # Logika tambahan: Jika ada bola di kotak 1 dan 2, hitung jarak dan gerakkan kamera ke tengah
    if detected_sections[1] and detected_sections[2]:
        print("Bola di kotak 1 dan 2, menggerakkan kamera ke tengah bola")
        
        # Ambil bola yang berada di kotak 1 dan 2
        ball1_center_x = ball2_center_x = None
        for ball in bbox:
            ball_left, ball_top, ball_right, ball_bottom = ball
            center_x = (ball_left + ball_right) // 2  # Hitung titik tengah bola
            
            if ball_right > section_0_left and ball_left < section_1_right:  # Bola di kotak 1
                ball1_center_x = center_x
            if ball_right > section_1_right and ball_left < section_2_right:  # Bola di kotak 2
                ball2_center_x = center_x

        # Pastikan kedua bola ditemukan
        if ball1_center_x is not None and ball2_center_x is not None:
            # Hitung posisi tengah antara kedua bola
            middle_x = (ball1_center_x + ball2_center_x) // 2
            print(f"Jarak horizontal antara bola 1 dan bola 2: {abs(ball1_center_x - ball2_center_x)}")
            print(f"Menggerakkan kamera ke posisi tengah: {middle_x}")
            # Kirim perintah ke Arduino untuk menggerakkan kamera ke posisi tengah
            # arduino.write(f'm{middle_x}'.encode())
        else:
            print("Bola di kotak 1 dan 2 tidak lengkap")
    
    # Logika normal jika hanya ada satu bola di kotak 1 atau 2
    elif detected_sections[1] or detected_sections[2]:
        if detected_sections[1]:
            print("Bola di kotak 1, belok kanan untuk menghindar")
            # arduino.write(b'a')  # Kirim perintah untuk belok kiri
        if detected_sections[2]:
            print("Bola di kotak 2, belok kiri untuk menghindar")
            # arduino.write(b'b')  # Kirim perintah untuk belok kanan
    elif detected_sections[3]:  # Kotak 3 terdeteksi
        print("Bola di kotak 3 (kanan), lurus")
        # arduino.write(b'c')  # Kirim perintah untuk lurus
    elif detected_sections[0]:  # Kotak 0 terdeteksi
        print("Bola di kotak 0 (kiri), lurus")
        # arduino.write(b'c')  # Kirim perintah untuk lurus
    else:
        print("Tidak ada bola terdeteksi, jalan lurus")
        # arduino.write(b'c')  # Kirim perintah untuk stop atau lurus jika tidak ada bola

# Loop utama
while True:
    ret, img = cap.read()
    if not ret:
        print("Gagal mengambil gambar dari kamera.")
        continue

    count += 1
    if count % frame_skip != 0:  # Hanya proses setiap frame ke-5
        continue

    # Resize gambar untuk mempercepat pemrosesan
    img = cv2.resize(img, (resize_width, resize_height))  # Ukuran lebih kecil untuk pemrosesan lebih cepat

    # Deteksi objek menggunakan YOLO
    bbox, label, conf = yolo.detect_objects(img)
    
    # Variabel untuk menyimpan koordinat bola hijau dan merah
    balls_positions = []  # Daftar untuk menyimpan posisi tengah bola

    # Loop melalui label yang terdeteksi
    for i, lbl in enumerate(label):
        if lbl == "0" or lbl == "1":  # Deteksi bola hijau atau bola merah
            ball = bbox[i]
            balls_positions.append(ball)

    height, width, _ = img.shape

    # Jika ada bola yang terdeteksi, gerakkan kamera
    if bbox:  # Periksa apakah ada bounding box
        move_camera(balls_positions, width, bbox)
    else:
        print("Tidak ada bola terdeteksi, jalan lurus")

    # Gambar garis vertikal untuk mewakili kotak
    num_sections = 4  # Jumlah kotak yang diinginkan
    for i in range(1, num_sections):
        x = i * width // num_sections
        cv2.line(img, (x, 0), (x, height), (0, 0, 0), 2)  # Garis vertikal

    # Tampilkan gambar
    cv2.imshow("img1", img)

    if cv2.waitKey(1) & 0xFF == 27:  # Tekan 'ESC' untuk keluar
        break

# Tutup kamera dan jendela
cap.release()
cv2.destroyAllWindows()
