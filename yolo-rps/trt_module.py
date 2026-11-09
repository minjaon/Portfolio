
import os
import time
import cv2
from ultralytics import YOLO


_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
MODEL_PATH = os.path.join(_THIS_DIR, "best.engine")
CONF_TH = 0.5
IOU_TH = 0.45
IMG_SIZE = 640


class TRTDetector:
    """TensorRT(.engine)로 변환된 YOLO 모델을 로드해서 추론하는 클래스."""

    def __init__(self, model_path=MODEL_PATH, conf=CONF_TH, iou=IOU_TH, imgsz=IMG_SIZE):
        self.model = YOLO(model_path) 
        self.conf = conf
        self.iou = iou
        self.imgsz = imgsz


        raw_names = self.model.names
        self.name_map = {}
        for idx, n in raw_names.items():
            key = str(n).strip().lower()
            if 'rock' in key:
                self.name_map[idx] = 'rock'
            elif 'paper' in key:
                self.name_map[idx] = 'paper'
            elif 'scissor' in key:
                self.name_map[idx] = 'scissors'
            else:
                self.name_map[idx] = key

    def detect(self, frame):

        results = self.model.predict(
            frame, imgsz=self.imgsz, conf=self.conf, iou=self.iou, verbose=False
        )
        dets = []
        r = results[0]
        if r.boxes is None:
            return dets

        for box in r.boxes:
            cid = int(box.cls[0])
            score = float(box.conf[0])
            x1, y1, x2, y2 = map(int, box.xyxy[0])
            move = self.name_map.get(cid)
            if move not in ('rock', 'paper', 'scissors'):
                continue
            dets.append((x1, y1, x2, y2, move, score))
        return dets

    def draw(self, frame, dets):
        """검출 결과를 프레임 위에 박스+라벨로 그려줌 (디버깅/시각화용)."""
        color_map = {'rock': (0, 255, 0), 'paper': (0, 0, 255), 'scissors': (255, 0, 0)}
        for (x1, y1, x2, y2, move, score) in dets:
            color = color_map.get(move, (255, 255, 255))
            cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
            cv2.putText(frame, f"{move} {score:.2f}", (x1, max(0, y1 - 7)),
                        cv2.FONT_HERSHEY_PLAIN, 1.5, color, 2)
        return frame


def _standalone_test():

    detector = TRTDetector(MODEL_PATH)

    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        raise RuntimeError("카메라를 열 수 없습니다. VideoCapture(0) 인덱스를 확인하세요.")

    print("TensorRT engine 로드 완료. 'q'를 누르면 종료합니다.")
    start_time = time.time()

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            break
        frame = cv2.flip(frame, 1)

        dets = detector.detect(frame)
        detector.draw(frame, dets)

        now = time.time()
        fps = 1 / (now - start_time) if now > start_time else 0.0
        start_time = now
        cv2.putText(frame, f'FPS: {fps:.1f}', (10, 25),
                    cv2.FONT_HERSHEY_PLAIN, 1.5, (0, 255, 255), 2)

        cv2.imshow('trt_module test', frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    _standalone_test()
