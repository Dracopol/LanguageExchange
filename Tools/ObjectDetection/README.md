# Object detection prototype

This is step 1 of the prototype plan. It checks that a pretrained COCO detector (Ultralytics YOLO11n) recognises our
objects from the laptop webcam. It also exports the model to ONNX for Unreal's NNE (step 2).

## Setup (once)

```powershell
cd Tools/ObjectDetection
& "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe" -m venv .venv
.venv\Scripts\python -m pip install -r requirements.txt
```

## Run

```powershell
.venv\Scripts\python detect_webcam.py                                # PyTorch model (downloads Models/yolo11n.pt)
.venv\Scripts\python detect_webcam.py --export-onnx                  # writes Models/yolo11n.onnx
.venv\Scripts\python detect_webcam.py --model Models/yolo11n.onnx    # same model through onnxruntime
```

Options: `--camera <index>` (default 0) and `--confidence <0..1>` (default 0.4).

Press Q or Esc to quit. The console then prints which classes were detected, in how many frames, and with what
best confidence. The classes we test with are highlighted in green:
- `mouse` for a computer mouse.
- `cup`, `wine glass` or `bottle` for a glass of water.

`Models/` is git-ignored because the weights can be downloaded again.

## Review a recording from Unreal

In the game, the console command `LanguageExchange.Detection.Record 1` saves each image the model sees and what it
found to `Saved/Detection/Recordings/<date-time>/`.

```powershell
.venv\Scripts\python review_recording.py <recording folder>     # per-class summary + contact sheets
.venv\Scripts\python compare_models.py <recording folder>       # run yolo11n/s/m on the same frames
.venv\Scripts\python review_recording.py <recording folder> --detections <recording folder>\Models\yolo11s\Detections.jsonl
```

The contact sheets show green boxes for detections at or above the on-screen threshold (0.6). Yellow boxes are near
misses that were only recorded.

Ultralytics YOLO is licensed under AGPL-3.0. That's fine for prototyping. A release needs a permissively licensed
detector or a commercial licence.
