"""
merge_own_data.py
-------------------
01_collect_data.py 로 모은 dataset/(scissors=0, rock=1, paper=2)를
Roboflow에서 받은 데이터셋(Paper-Rock-Scissors-2)의 클래스 순서에 맞게
라벨 번호를 다시 매핑한 뒤, 같은 폴더 구조로 합쳐줍니다.

*** 왜 필요한가 ***
Roboflow 데이터셋의 클래스 순서(0,1,2가 무엇을 가리키는지)가
01_collect_data.py 의 순서(scissors=0, rock=1, paper=2)와 다를 수 있습니다.
순서를 안 맞추고 그냥 섞으면 모델이 완전히 잘못된 걸 학습하게 됩니다.
이 스크립트가 data.yaml을 읽어서 자동으로 올바르게 변환해줍니다.

사용법 (Rps_Game 폴더에서 실행):
  pip install pyyaml   # 안 깔려있다면
  python merge_own_data.py

실행 전 확인:
  - dataset/            (01_collect_data.py 로 만든 폴더)
  - Paper-Rock-Scissors-2/   (Roboflow 다운로드 폴더, 이름이 다르면 아래 ROBOFLOW_DIR 수정)
  가 이 스크립트와 같은 위치에 있어야 합니다.
"""

import os
import shutil
import yaml

OWN_DATASET_DIR = "dataset"                 # 01_collect_data.py 결과 폴더
ROBOFLOW_DIR = "Paper-Rock-Scissors-2"      # roboflow 다운로드 폴더명 (다르면 수정)

# 01_collect_data.py 에서 정의한 내 데이터의 클래스 번호
OWN_CLASSES = {0: 'scissors', 1: 'rock', 2: 'paper'}


def load_roboflow_names(data_yaml_path):
    with open(data_yaml_path, 'r') as f:
        data = yaml.safe_load(f)
    names = data['names']
    if isinstance(names, dict):
        names = [names[i] for i in sorted(names)]
    return names


def find_split_dirs(base_dir, split_names):
    """roboflow 폴더에서 이미지/라벨 폴더를 찾아줌 (train/valid 등 이름이 다를 수 있어 유연하게 탐색)"""
    for split in split_names:
        img_dir = os.path.join(base_dir, split, 'images')
        lbl_dir = os.path.join(base_dir, split, 'labels')
        if os.path.isdir(img_dir) and os.path.isdir(lbl_dir):
            return img_dir, lbl_dir
    return None, None


def main():
    data_yaml_path = os.path.join(ROBOFLOW_DIR, 'data.yaml')
    if not os.path.exists(data_yaml_path):
        raise FileNotFoundError(f"'{data_yaml_path}' 를 찾을 수 없습니다. ROBOFLOW_DIR 값을 확인하세요.")

    rf_names = load_roboflow_names(data_yaml_path)
    print("Roboflow 클래스 순서:", rf_names)

    # 이름(소문자, 부분일치) -> roboflow class id
    name_to_rf_id = {}
    for idx, name in enumerate(rf_names):
        name_to_rf_id[name.strip().lower()] = idx

    def match_id(own_name):
        for key, idx in name_to_rf_id.items():
            if own_name in key or key in own_name:
                return idx
        return None

    remap = {}
    for own_id, own_name in OWN_CLASSES.items():
        rf_id = match_id(own_name)
        if rf_id is None:
            raise ValueError(f"'{own_name}' 클래스를 Roboflow data.yaml에서 못 찾았습니다: {rf_names}")
        remap[own_id] = rf_id
    print("클래스 매핑 (내 라벨 -> Roboflow 라벨):", remap)

    train_img_dir, train_lbl_dir = find_split_dirs(ROBOFLOW_DIR, ['train'])
    valid_img_dir, valid_lbl_dir = find_split_dirs(ROBOFLOW_DIR, ['valid', 'val'])

    if not train_img_dir:
        raise FileNotFoundError(
            f"{ROBOFLOW_DIR}/train/images 폴더를 찾을 수 없습니다. "
            f"폴더 구조를 확인하고 find_split_dirs 부분을 실제 폴더명에 맞게 수정하세요."
        )

    merged_train, merged_val = 0, 0

    split_map = [('train', train_img_dir, train_lbl_dir)]
    if valid_img_dir:
        split_map.append(('val', valid_img_dir, valid_lbl_dir))

    for split, img_dst, lbl_dst in split_map:
        own_img_dir = os.path.join(OWN_DATASET_DIR, 'images', split)
        own_lbl_dir = os.path.join(OWN_DATASET_DIR, 'labels', split)
        if not os.path.isdir(own_img_dir):
            continue

        for fname in os.listdir(own_img_dir):
            if not fname.lower().endswith(('.jpg', '.jpeg', '.png')):
                continue
            name_no_ext, ext = os.path.splitext(fname)
            lbl_fname = name_no_ext + '.txt'

            src_img = os.path.join(own_img_dir, fname)
            src_lbl = os.path.join(own_lbl_dir, lbl_fname)
            if not os.path.exists(src_lbl):
                continue

            # 파일명 충돌 방지를 위해 접두어 부여
            new_base = f"custom_{name_no_ext}"
            dst_img = os.path.join(img_dst, new_base + ext)
            dst_lbl = os.path.join(lbl_dst, new_base + '.txt')

            shutil.copy(src_img, dst_img)

            with open(src_lbl, 'r') as f_in, open(dst_lbl, 'w') as f_out:
                for line in f_in:
                    parts = line.strip().split()
                    if not parts:
                        continue
                    own_cid = int(parts[0])
                    new_cid = remap[own_cid]
                    f_out.write(' '.join([str(new_cid)] + parts[1:]) + '\n')

            if split == 'train':
                merged_train += 1
            else:
                merged_val += 1

    print(f"\n병합 완료! train {merged_train}장, val {merged_val}장 추가됨")
    print(f"이제 '{ROBOFLOW_DIR}' 폴더 전체를 압축해서 Colab에 업로드하면 됩니다:")
    print(f"  zip -r merged_dataset.zip {ROBOFLOW_DIR}")


if __name__ == '__main__':
    main()
