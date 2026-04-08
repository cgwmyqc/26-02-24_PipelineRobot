#!/usr/bin/env bash
set -euo pipefail

# =========================
# Config
# =========================
DATASET_ROOT=/home/hit/pipe_dataset
YOLO_IMAGES_DIR=$DATASET_ROOT/yolo_images
YOLO_PROJECT_DIR=$DATASET_ROOT/yolo_out
YOLO_NAME=predict

FIT_SCRIPT=/home/hit/fit_cylinder_robust.py
YOLO_MODEL=/home/hit/ultralytics/best.pt
FULL_PIPELINE_SCRIPT=/home/hit/run_pipe_full_pipeline.sh

CONDA_SH=/home/hit/anaconda3/etc/profile.d/conda.sh
CONDA_ENV=yolo11

OPEN_MESHLAB=0   # 1=自动打开meshlab，0=不打开

# =========================
# Checks
# =========================
echo "========================================================="
echo "[CHECK] basic paths"
echo "========================================================="

[ -d "$DATASET_ROOT" ] || { echo "[ERR] dataset root not found: $DATASET_ROOT"; exit 1; }
[ -f "$FIT_SCRIPT" ] || { echo "[ERR] fit script not found: $FIT_SCRIPT"; exit 1; }
[ -f "$YOLO_MODEL" ] || { echo "[ERR] yolo model not found: $YOLO_MODEL"; exit 1; }
[ -f "$FULL_PIPELINE_SCRIPT" ] || { echo "[ERR] full pipeline script not found: $FULL_PIPELINE_SCRIPT"; exit 1; }
[ -f "$CONDA_SH" ] || { echo "[ERR] conda.sh not found: $CONDA_SH"; exit 1; }

# =========================
# Step 1: batch cylinder fit
# =========================
echo "========================================================="
echo "[STEP 1] batch cylinder fitting"
echo "========================================================="

for d in "$DATASET_ROOT"/stop_*; do
  [ -d "$d" ] || continue

  pcd="$d/cloud_accum.pcd"
  out="$d/cylinder_fit_robust.json"

  if [ -f "$pcd" ]; then
    echo "[FIT] $d"
    python3 "$FIT_SCRIPT" "$pcd" "$out"
  else
    echo "[SKIP] missing pcd: $pcd"
  fi
done

# =========================
# Step 2: build yolo_images links
# =========================
echo "========================================================="
echo "[STEP 2] build yolo_images links"
echo "========================================================="

mkdir -p "$YOLO_IMAGES_DIR"

for d in "$DATASET_ROOT"/stop_*; do
  [ -d "$d" ] || continue
  sid=$(basename "$d")
  img="$d/image_best.png"

  if [ -f "$img" ]; then
    ln -sf "$img" "$YOLO_IMAGES_DIR/${sid}.png"
  else
    echo "[SKIP] missing image: $img"
  fi
done

echo "[INFO] linked images:"
ls -lah "$YOLO_IMAGES_DIR" | head

# =========================
# Step 3: YOLO batch predict
# =========================
echo "========================================================="
echo "[STEP 3] yolo batch predict"
echo "========================================================="

source "$CONDA_SH"
conda activate "$CONDA_ENV"

PRED_DIR="$YOLO_PROJECT_DIR/$YOLO_NAME"

# 先删除旧的 predict 输出目录
if [ -d "$PRED_DIR" ]; then
  echo "[INFO] remove old prediction dir: $PRED_DIR"
  rm -rf "$PRED_DIR"
fi

yolo predict \
  model="$YOLO_MODEL" \
  source="$YOLO_IMAGES_DIR" \
  save_txt=True \
  save_conf=True \
  project="$YOLO_PROJECT_DIR" \
  name="$YOLO_NAME" \
  device=0

# =========================
# Step 3.5: extract annotated images with defects
# =========================
echo "========================================================="
echo "[STEP 3.5] extract annotated images with defects"
echo "========================================================="

DEFECT_IMG_DIR=$DATASET_ROOT/defect_images_boxed
LABELS_DIR=$YOLO_PROJECT_DIR/$YOLO_NAME/labels
PRED_IMG_DIR=$YOLO_PROJECT_DIR/$YOLO_NAME

mkdir -p "$DEFECT_IMG_DIR"
find "$DEFECT_IMG_DIR" -type f -delete

copied_count=0

for lbl in "$LABELS_DIR"/*.txt; do
  [ -f "$lbl" ] || continue
  [ -s "$lbl" ] || continue   # 只处理非空标签

  base=$(basename "$lbl" .txt)

  pred_img=""
  for ext in jpg png jpeg bmp webp; do
    if [ -f "$PRED_IMG_DIR/${base}.${ext}" ]; then
      pred_img="$PRED_IMG_DIR/${base}.${ext}"
      break
    fi
  done

  if [ -z "$pred_img" ]; then
    echo "[WARN] predicted boxed image not found for $base"
    continue
  fi

  has_PL=0
  has_BX=0
  has_ZAW=0
  has_SG=0

  while read -r cls rest; do
    case "$cls" in
      0) has_PL=1 ;;
      1) has_BX=1 ;;
      2) has_ZAW=1 ;;
      3) has_SG=1 ;;
    esac
  done < "$lbl"

  suffix=""
  [ "$has_PL" -eq 1 ] && suffix="${suffix}_PL"
  [ "$has_BX" -eq 1 ] && suffix="${suffix}_BX"
  [ "$has_ZAW" -eq 1 ] && suffix="${suffix}_ZAW"
  [ "$has_SG" -eq 1 ] && suffix="${suffix}_SG"

  ext="${pred_img##*.}"
  out_name="${base}${suffix}.${ext}"

  cp -f "$pred_img" "$DEFECT_IMG_DIR/$out_name"
  echo "[COPY] $out_name"
  copied_count=$((copied_count + 1))
done

echo "[INFO] copied boxed defect images: $copied_count"
echo "[INFO] saved to: $DEFECT_IMG_DIR"

# =========================
# Step 4+5: bbox->3D + global mesh
# =========================
echo "========================================================="
echo "[STEP 4+5] 3D mapping + global mesh"
echo "========================================================="

bash "$FULL_PIPELINE_SCRIPT"

# =========================
# Final info
# =========================
echo "========================================================="
echo "[DONE] full postprocess finished"
echo "========================================================="
echo "[INFO] labels dir: $YOLO_PROJECT_DIR/$YOLO_NAME/labels"
echo "[INFO] global json: $DATASET_ROOT/defects_global.json"
echo "[INFO] mesh ply   : $DATASET_ROOT/pipe_mesh_labeled.ply"

if [ "$OPEN_MESHLAB" = "1" ]; then
  if command -v meshlab >/dev/null 2>&1; then
    meshlab "$DATASET_ROOT/pipe_mesh_labeled.ply" >/dev/null 2>&1 &
  else
    echo "[WARN] meshlab not found, skip opening mesh viewer."
  fi
fi
