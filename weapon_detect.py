import torch
import cv2
import serial
import time

# ==== SERIAL CONFIG ====
SERIAL_PORT = "COM5"
BAUD_RATE = 115200

ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
time.sleep(2)

# ==== LOAD YOLOv5 MODEL ====
model = torch.hub.load('yolov5', 'custom', path='weapons.pt', source='local')

cap = cv2.VideoCapture(0)

print("Press 'c' to capture and detect weapon")
print("Press 'q' to quit")

while True:
    ret, frame = cap.read()
    if not ret:
        break

    cv2.imshow("Weapon Detection - Live Feed", frame)

    key = cv2.waitKey(1) & 0xFF

    # Quit
    if key == ord('q'):
        break

    # Capture on pressing 'c'
    if key == ord('c'):
        print("Captured! Running detection...")

        results = model(frame)
        detections = results.xyxy[0]

        weapon_detected = 0

        for *box, conf, cls in detections:
            conf = float(conf)

            if conf > 0.5:
                weapon_detected = 1
                x1, y1, x2, y2 = map(int, box)

                cv2.rectangle(frame, (x1, y1), (x2, y2),
                              (0, 0, 255), 2)

                label = f"{model.names[int(cls)]} {conf:.2f}"
                cv2.putText(frame, label,
                            (x1, y1 - 10),
                            cv2.FONT_HERSHEY_SIMPLEX,
                            0.7, (0, 0, 255), 2)

        # Send serial value ONLY after capture
        ser.write(f"{weapon_detected}\n".encode())
        print("Serial Sent:", weapon_detected)

        cv2.imshow("Captured Result", frame)
        cv2.waitKey(0)   # Wait until any key is pressed
        print("Returning to live feed...")

cap.release()
ser.close()
cv2.destroyAllWindows()