"""
train_pc.py
------------
download_dataset.py 로 받은 데이터셋으로 YOLO11n을 학습합니다.
VS Code 터미널에서: python train_pc.py

GPU가 없다면 device="cpu" 로 바꾸세요 (느리지만 동작은 함).
"""

from ultralytics import YOLO

# download_dataset.py 실행 후 출력된 경로로 수정하세요
DATA_YAML = "Paper-Rock-Scissors-2/data.yaml"


def main():
    model = YOLO("yolo11n.pt")  # COCO 사전학습 가중치 (최초 실행시 자동 다운로드)

    model.train(
        data=DATA_YAML,
        imgsz=640,
        epochs=50,
        batch=16,
        device=0,        # GPU 없으면 "cpu" 로 변경
        project="runs_rps",
        name="rps_yolo11n",
        exist_ok=True,
    )

    metrics = model.val()
    print(metrics)

    print("\n학습 완료!")
    print("가중치 위치: runs_rps/rps_yolo11n/weights/best.pt")
    print("이 경로를 rps_game_vscode.py 의 MODEL_PATH 에 넣어주세요.")


if __name__ == "__main__":
    main()
