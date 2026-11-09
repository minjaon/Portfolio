
import os
import time
import random
import cv2
from ultralytics import YOLO


_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
MODEL_PATH = os.path.join(_THIS_DIR, "best.onnx")
CONF_TH = 0.5
IOU_TH = 0.45
IMG_SIZE = 640
COUNTDOWN_SEC = 3
CAM_WIDTH, CAM_HEIGHT = 640, 480

model = YOLO(MODEL_PATH)


raw_names = model.names
NAME_MAP = {}
for idx, n in raw_names.items():
    key = str(n).strip().lower()
    if 'rock' in key:
        NAME_MAP[idx] = 'rock'
    elif 'paper' in key:
        NAME_MAP[idx] = 'paper'
    elif 'scissor' in key:
        NAME_MAP[idx] = 'scissors'
    else:
        NAME_MAP[idx] = key

moveColor = {'rock': (0, 255, 0), 'paper': (0, 0, 255), 'scissors': (255, 0, 0)}

WIN_RULE = {
    ('scissors', 'paper'): True,
    ('rock', 'scissors'): True,
    ('paper', 'rock'): True,
}


def detect(frame):
   
    results = model.predict(frame, imgsz=IMG_SIZE, conf=CONF_TH, iou=IOU_TH, verbose=False)
    dets = []
    r = results[0]
    if r.boxes is None:
        return dets
    for box in r.boxes:
        cid = int(box.cls[0])
        sc = float(box.conf[0])
        x1, y1, x2, y2 = map(int, box.xyxy[0])
        move = NAME_MAP.get(cid)
        if move not in ('rock', 'paper', 'scissors'):
            continue
        dets.append((x1, y1, x2, y2, move, sc))
    return dets


def pick_two_players(dets):

    if len(dets) < 2:
        return None, None

    # 신뢰도 상위 2개만 사용 (손이 아닌 잡음 검출 배제)
    top2 = sorted(dets, key=lambda d: d[5], reverse=True)[:2]
    # 왼쪽/오른쪽 정렬 (x1 좌표 기준)
    top2_sorted = sorted(top2, key=lambda d: d[0])
    p1_det, p2_det = top2_sorted[0], top2_sorted[1]
    return p1_det, p2_det


def draw_detections(frame, dets):
    for (x1, y1, x2, y2, move, sc) in dets:
        color = moveColor.get(move, (255, 255, 255))
        cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
        cv2.putText(frame, f"{move} {sc:.2f}", (x1, max(0, y1 - 7)),
                    cv2.FONT_HERSHEY_PLAIN, 1.5, color, 2)


def judge(p1_move, p2_move):
    """P1 관점에서 판정: 'P1', 'P2', 'DRAW'"""
    if p1_move == p2_move:
        return 'DRAW'
    if (p1_move, p2_move) in WIN_RULE:
        return 'P1'
    return 'P2'


def put_center_text(frame, text, y, scale=2.0, color=(255, 255, 255), thickness=3):
    (tw, th), _ = cv2.getTextSize(text, cv2.FONT_HERSHEY_DUPLEX, scale, thickness)
    x = (frame.shape[1] - tw) // 2
    cv2.putText(frame, text, (x, y), cv2.FONT_HERSHEY_DUPLEX, scale, color, thickness, cv2.LINE_AA)


STATE_IDLE, STATE_COUNTDOWN, STATE_RESULT = 'IDLE', 'COUNTDOWN', 'RESULT'
state = STATE_IDLE
countdown_start = 0.0
p1_move, p2_move, verdict = None, None, None
p1_score = p2_score = draw_score = 0

cap = cv2.VideoCapture(0)
cap.set(cv2.CAP_PROP_FRAME_WIDTH, CAM_WIDTH)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, CAM_HEIGHT)
cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

if not cap.isOpened():
    raise RuntimeError("Cannot open camera. Check VideoCapture(0) index.")

cv2.namedWindow('RPS Game - P1 vs P2', cv2.WINDOW_NORMAL)
startTime = time.time()

while cap.isOpened():
    ret, frame = cap.read()
    if not ret:
        break
    frame = cv2.flip(frame, 1)  # 거울 모드
    now = time.time()

    if state == STATE_IDLE:
        dets = detect(frame)
        draw_detections(frame, dets)
        p1_det, p2_det = pick_two_players(dets)
        if p1_det is not None:
            put_center_text(frame, "READY! (P1 left / P2 right)", 40, scale=1.0, color=(0, 255, 0), thickness=2)
        else:
            put_center_text(frame, "Show BOTH hands to start", 40, scale=1.0, color=(0, 165, 255), thickness=2)
        put_center_text(frame, "SPACE: START", 75, scale=1.3, color=(0, 255, 255), thickness=2)

    elif state == STATE_COUNTDOWN:
        remaining = COUNTDOWN_SEC - (now - countdown_start)
        if remaining > 0:
            put_center_text(frame, str(int(remaining) + 1), frame.shape[0] // 2,
                             scale=6, color=(0, 255, 255), thickness=6)
        else:
            dets = detect(frame)
            p1_det, p2_det = pick_two_players(dets)
            if p1_det is None:
                p1_move = p2_move = None
                verdict = "Could not detect 2 hands! Press R to retry"
            else:
                p1_move = p1_det[4]
                p2_move = p2_det[4]
                v = judge(p1_move, p2_move)
                if v == 'P1':
                    verdict = "P1 WINS!"
                    p1_score += 1
                elif v == 'P2':
                    verdict = "P2 WINS!"
                    p2_score += 1
                else:
                    verdict = "DRAW"
                    draw_score += 1
            state = STATE_RESULT

    elif state == STATE_RESULT:
        if p1_move is None:
            put_center_text(frame, verdict, frame.shape[0] // 2, scale=1.0, color=(0, 0, 255), thickness=2)
        else:
            put_center_text(frame, f"P1: {p1_move.upper()}", 80, scale=1.3, color=(0, 255, 0), thickness=2)
            put_center_text(frame, f"P2: {p2_move.upper()}", 140, scale=1.3, color=(0, 128, 255), thickness=2)
            put_center_text(frame, verdict, frame.shape[0] // 2 + 40, scale=1.8, color=(255, 255, 255), thickness=3)
        put_center_text(frame, "R: Restart / Q: Quit", frame.shape[0] - 20, scale=0.9, color=(200, 200, 200), thickness=2)

    # 스코어보드
    cv2.putText(frame, f"P1 {p1_score}   P2 {p2_score}   DRAW {draw_score}",
                (10, frame.shape[0] - 10), cv2.FONT_HERSHEY_PLAIN, 1.5, (255, 255, 0), 2)

    # FPS
    curTime = time.time()
    dt = curTime - startTime
    fps = 1 / dt if dt > 0 else 0.0
    startTime = curTime
    cv2.putText(frame, f'FPS: {fps:.1f}', (10, 25), cv2.FONT_HERSHEY_PLAIN, 1.5, (0, 255, 255), 2)

    cv2.imshow('RPS Game - P1 vs P2', frame)
    key = cv2.waitKey(1) & 0xFF
    if key == ord('q'):
        break
    elif key == ord(' ') and state == STATE_IDLE:
        state = STATE_COUNTDOWN
        countdown_start = time.time()
    elif key == ord('r') and state == STATE_RESULT:
        state = STATE_IDLE
        p1_move = p2_move = verdict = None

cap.release()
cv2.destroyAllWindows()