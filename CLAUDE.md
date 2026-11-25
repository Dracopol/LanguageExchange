# LanguageExchange

Mixed-reality language exchange app for Meta Quest, built in Unreal Engine 5.8 (C++).
Code conventions: the global Unreal C++ conventions in `~/.claude/CLAUDE.md` apply. This file only adds what's
specific to this project.

## Idea

Two people, each learning the other's language. A player points at (or faces) a real object; the app recognises it
with computer vision and shows its spelling in the other player's language. The player says the word to their
partner, who checks it.

Planned pieces:
- Passthrough mixed reality on Quest 3 / 3S (Meta XR plugin).
- Object recognition on the headset's passthrough camera image (Meta Passthrough Camera API), with a pretrained
  detection model (e.g. YOLO trained on COCO, exported to ONNX) run through Unreal's NNE.
- Selection by hand-tracking pointing ray, or by head direction (Quest 3 has no eye tracking).
- A curated word list per object class and language (not live machine translation).
- Multiplayer in the same room: Unreal replication + Meta shared spatial anchors (colocation).
- Later: speech recognition to check the spoken word.

## Project setup

- Engine: Unreal Engine 5.8, C++ project created from the Blank template.
- Target platform: Mobile, quality preset Scalable (target device is Quest; keep rendering mobile-friendly).
- Primary module: `LanguageExchange` (`Source/LanguageExchange`), export macro `LANGUAGEEXCHANGE_API`.
- Class prefix: the project name after the type prefix, e.g. `ALanguageExchangePlayerPawn`.
- Log category: `LogLanguageExchange`.
- Code is edited in VS Code (workspace generated via Tools > Refresh Visual Studio Code Project);
  Visual Studio 2022 (MSVC 14.44) is installed for the compiler.
- Git with LFS for binary assets (see `.gitattributes`); the user commits and pushes via GitHub Desktop.
  Finished, working steps go on `main`; each new step gets a feature branch (e.g. `feature/nne-detection`).

## Prototype plan (current step first)

1. Done: PC, Python, webcam (`Tools/ObjectDetection`, see its README).
2. Done: Unreal on PC, webcam: run the same model as ONNX through NNE and show the detected names.
3. Current: Unreal on Quest: use the passthrough camera image and run the model on the headset; measure speed.

Riskiest open questions: passthrough camera access from Unreal (Meta's samples target Unity first), and NNE
performance on Quest.

### Step 1 results

- YOLO11n (COCO, no training) recognises the test objects: cup / glass works; the computer mouse is sometimes
  labelled `remote` depending on the angle. Person and other objects are detected too.
- False positive seen: a printed banner detected as `chair` at 0.50 confidence.
- Speed on the user's PC: about 29 ms per frame (about 25 FPS) with YOLO11n.
- Planned fixes, cheapest first: confidence threshold about 0.6; smooth labels over time (keep the label that wins
  over the last 0.5–1 s, which fits "point at an object for a moment"); try YOLO11s if Quest speed allows;
  restrict classes to the word list; fine-tune on own photos only once the object set is fixed.
- Detection doesn't need to run every frame; a few times per second is enough.
- Licence: Ultralytics YOLO is AGPL-3.0 — fine for prototyping, a release needs another detector or a licence.

### Step 2: how detection works in Unreal

- `ULanguageExchangeObjectDetectionSubsystem` (world subsystem, game and PIE only): webcam through the Media Framework
  (`UMediaPlayer` + `UMediaTexture`) → letterboxed into a 640 x 640 render target (grey 114 bars, as Ultralytics
  does) → async GPU readback → `FYoloModel` on a `UE::Tasks` worker → results back on the game thread. Runs on a
  timer (every 0.25 s); ticks only while a frame is in flight.
- `FYoloModel` (`Private/Detection`): NNE CPU model; RGB 0..1 planar input, decodes the 1 x 84 x 8400 output, per-class
  non-maximum suppression. Ignored classes are dropped after picking each box's best class (as Ultralytics does).
- `ULanguageExchangeDetectionWidget`: C++-only overlay (no Widget Blueprint) drawing the camera image, boxes, labels
  and a status line; created by `ALanguageExchangePlayerController`, set via `ALanguageExchangeGameMode`
  (the global default game mode).
- Settings: Project Settings > LanguageExchange > Object Detection (`DefaultGame.ini`): model
  (`/Game/LanguageExchange/Detection/NNE_Yolo11n`), NNE runtime name, camera index, minimum confidence 0.6, box
  overlap 0.45, interval 0.25 s, ignored classes (currently `person`, for testing objects held up to the webcam).
- The model asset was imported with the `ImportAssets` commandlet (`UnrealEditor-Cmd.exe <uproject>
  -run=ImportAssets -source=<file>.onnx -dest=/Game/LanguageExchange/Detection -replaceexisting`); dragging the
  `.onnx` into the Content Browser works too. Asset prefix for NNE model data: `NNE_`.
- NNE's ONNX Runtime defaults to one thread in game builds; `DefaultEngine.ini` sets `GameThreadingOptions` to all
  cores (PC only).
- Debugging: console variable `LanguageExchange.Detection.Record 1` saves every model input plus all detections down
  to 0.25 to `Saved/Detection/Recordings/<date-time>/`. `Tools/ObjectDetection/review_recording.py` turns that into
  contact sheets (green = shown, yellow = near miss) and a per-class summary; `compare_models.py` runs other YOLO
  sizes on the same frames. On shutdown the log prints the average inference time.

### Step 2 results

- Unreal gives exactly the same detections as Ultralytics in Python on the same frames (checked on a recording).
- Speed on the user's PC (CPU, ONNX Runtime): YOLO11n about 30 ms per frame with the game capped at 60 FPS
  (`t.MaxFPS 60`); about 42 ms uncapped, because the game competes for CPU cores.
- Lighting matters most: at home in dim light the webcam blurs every movement and results drop a lot; in a bright
  room the glass was recognised (`cup` / `wine glass`, 0.6–0.86) almost whenever it was held still.
- The hand-held mouse (black, blue LEDs) was never `mouse` in any model size (medium: `donut` 0.92); an upright spoon
  in front of the body became `tie`. COCO shows these objects in their usual context (mouse on a desk, cutlery on a
  table).
- Objects outside COCO get confidently mapped to look-alikes: glue stick → `toothbrush` (0.87), pen → `toothbrush`,
  printed paper → `book`. The word list must only use classes whose mapping fits; other objects need fine-tuning.
- Model sizes on the same frames (Python, CPU): YOLO11n 29 ms, YOLO11s 67 ms, YOLO11m 180 ms. Small finds spoon,
  fork and printed paper (`book` 0.97) far more often than nano; medium is better again but likely too slow for
  Quest. Test both nano and small on Quest.

### Notes for step 3

- NNE's ONNX Runtime plugin (`NNERuntimeORT`) is desktop only (Win64, Linux, Mac). On Quest (Android), try
  `NNERuntimeIREECpu` (experimental plugin `NNERuntimeIREE`, compiles the model ahead of time per platform); the
  runtime is a setting, so the code doesn't change.
- The engine has an `AndroidCamera` media plugin; if the Passthrough Camera API frames can come through the Media
  Framework, the capture code stays the same too.
- The Android SDK isn't set up in Unreal yet (`Failed to SetupSDK for platform 'Android'`).
- The soft model reference in the settings must be cooked: add the model folder to "Additional Asset Directories to
  Cook" when packaging.

## Device plan

- A (goal): standalone Quest app, camera and model both on the headset.
- B (fallback if the Quest is too slow): still a standalone app, but camera frames go over Wi-Fi to a PC/server
  for detection (adds about 50–150 ms; camera images leave the headset).
- C: PCVR via Link is not an option for detection — as far as known, the Passthrough Camera API only gives frames
  to apps running on the headset (verify against Meta's current docs). PCVR stays useful for developing the
  non-camera parts (UI, multiplayer, word lists).
