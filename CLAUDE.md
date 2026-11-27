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
3. Done: Unreal on Quest: use the passthrough camera image and run the model on the headset; measure speed.
   Result: standalone works (device plan A). YOLO11s at 320 px on ONNX Runtime + XNNPACK (2 threads), ~120 ms.
4. Current: make detections visible in VR, then selection by pointing (and detecting on a crop around it).
   Done so far: labels in the room. The headset camera source stores the camera pose when a frame is captured
   (`TryGetCameraPose`, the pose of the latest camera image) and turns a box centre into a world ray with that pose
   (`TryGetImageRay`, MRUK's `ViewportPointToWorldSpaceRay` maths with a given pose, because detection finishes
   ~0.1 s later). `ALanguageExchangeDetectionLabels` (spawned by the player controller when a headset is active)
   listens to the subsystem's `OnDetectionsChange` and shows, per detection, a green frame around the object and a
   name panel on its upper edge, facing the camera. Both are world-space `UWidgetComponent`s with C++-only widgets
   (`ULanguageExchangeFrameWidget`, `ULanguageExchangeLabelWidget`, drawn in `NativePaint`); the engine's 3D text
   materials are lit and show black in the unlit passthrough level. The subsystem also turns the box corners into
   rays (`WorldCornerDirections`). The distance is estimated from a typical size per class and the box's angular
   size (unknown classes: 80 cm); the corners go where their rays meet a plane at that distance, and the frame is
   the upright rectangle (by world up) around them, so it doesn't tilt with the head. The frame line and the label
   keep their apparent size at any distance; the label sits on the frame's top edge.
   MRUK gotcha: its viewport points start at the image's bottom left (Unity's convention), ours at the top left, so
   `TryGetImageRay` flips Y. Without that, boxes were mirrored vertically: right straight ahead, but moving up when
   looking up.
   Tested on Quest 3: frames fit the objects, even lines, both eyes, 72 FPS.
   Smoothing: each detection is matched to a shown object by direction from the camera (within 4° or half the
   box size; closest pairs first). Its frame then glides to the new place (`GlideSeconds` 0.1), objects stay shown
   1 s after their last detection (`HoldSeconds`), and the name is the class with the highest confidence sum, fading
   over about 1 s (`ClassMemorySeconds`; it also picks the typical size for the distance). The actor ticks only while
   objects are shown. Tested on Quest 3: no more flicker.
   Console variable `LanguageExchange.Labels.ShowTestLabel 1` (at startup) shows one fixed label 1 m ahead instead
   of the detections, for checking rendering without detection.
   Depth: the labels actor creates MRUK's environment raycaster (`UMRUKSubsystem::CreateEnvironmentRaycaster`, depth
   sensing) and casts 5 rays per detection from the image's camera pose (box centre and halfway to each corner); the
   median hit, measured along the centre ray, is the distance. Without a hit it falls back to the size estimate.
   The label shows the distance (`~` = estimate); `LanguageExchange.Labels.UseDepth 0` switches depth off.
   Works over Link (depth found for 315 of 316 detections). Not yet on Quest: needs the scene permission
   (`com.oculus.permission.USE_SCENE`, manifest + runtime request). `OculusXRAnchors` is a module dependency because
   `MRUtilityKitSubsystem.h` includes its headers.
   Next: pointing (hand ray cast against the depth, then the detected object at that point).

### Testing over Link (instead of building for the Quest)

Works: passthrough, the headset camera (640 x 480) and detection on the PC (~50 ms), labels in the room.
Use it for everything except speed measurements, which need a Quest build.
- Link PC app: Meta Horizon Link set as the active OpenXR runtime; Beta > Developer Runtime Features and
  Passthrough over Meta Horizon Link on. In the headset: Quick Settings > Link.
- Start the editor while Link is running, then Play > VR Preview with the headset on. If Link restarts (e.g. another
  headset), the editor's OpenXR connection is lost (`XR_ERROR_INSTANCE_LOST`, VR Preview greyed out): restart it.
- `r.PostProcessing.PropagateAlpha=True` (desktop renderer only; the Quest uses `r.Mobile.PropagateAlpha`): without
  it the PC writes no alpha, and the headset shows passthrough but nothing of the app.
- MRUK's camera stop crashes the editor when play ends on PC, so `ULanguageExchangeHeadsetCameraSource::Close` only
  stops the camera on Android.

Open for later: LiteRT (option B) as a comparison to ONNX Runtime; display rate 90/120 Hz; a world-space status
display instead of the 2D overlay (not visible in VR).

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

- NNE has no CPU runtime for Android in the launcher's UE 5.8: `NNERuntimeORT` is desktop only, and
  `NNERuntimeIREE` (experimental) ships runtime libraries only for Windows and compile configurations only for
  desktop → desktop. `NNERuntimeBasicCpu` only handles simple networks (no convolutions). So on the Quest the model
  needs a runtime outside NNE (e.g. ONNX Runtime for Android, LiteRT), or detection on a PC (device plan B).
- Camera access in Unreal goes through Meta's Mixed Reality Utility Kit (part of the Meta XR plugin):
  `UMRUKPassthroughCameraAccess::Play()` on a `PassthroughCameraAccess` asset fills a `PassthroughCameraAccessTexture`;
  needs Horizon OS v74+ and `horizonos.permission.HEADSET_CAMERA` (or `android.permission.CAMERA`). The detection
  subsystem only needs a texture, so the webcam's `UMediaTexture` can be swapped for it.
- Per Meta's MRUK docs, Meta Horizon Link v201+ also supports the Passthrough Camera API (unverified here), which
  would allow developing camera + detection from the PC.
- The soft model reference in the settings must be cooked: add the model folder to "Additional Asset Directories to
  Cook" when packaging.

### Step 3: Quest setup (done on the user's PC)

- Device: Quest 3, Horizon OS v210, developer mode on, USB debugging authorised.
- Android toolchain for UE 5.8: Android Studio Koala 2024.1.2 Patch 1 (`winget install --id Google.AndroidStudio
  --version 2024.1.2.13 --exact`), run once for the SDK, plus "Android SDK Command-line Tools (latest)" from its SDK
  Manager; then `Engine/Extras/Android/SetupAndroid.bat android-36 36.0.0 3.22.1 27.2.12479018` (NDK r27c, sets
  `ANDROID_HOME`, `JAVA_HOME`, `NDKROOT`). The launcher's UE 5.8 also needs the **Android target platform** option.
  Check: `RunUAT Turnkey -command=VerifySdk -platforms=Android -device=Android@All -UpdateIfNeeded=false`.
- Meta XR plugin v209 (for UE 5.8.0; includes MRUK) from Meta's "Unreal Engine 5 Integration" download, copied to
  `Engine/Plugins/Marketplace/MetaXR` (Meta's documented location; not in this repo). Its build ID matches the
  launcher's 5.8 build. On PC, Meta's OVRPlugin path fails (UE's Windows OpenXR loader is 1.0.25, the plugin asks for
  OpenXR 1.1) and Unreal falls back to its own OpenXR; this doesn't affect Android builds.
- Project settings from Meta's Project Setup Tool (Meta XR Tools > Project Setup Tool): all required rules (also on
  the Quest 3 tab), recommended rules except "Framebuffer Fetch" and "Static Lighting"; mobile shading set to Forward
  (needed for MSAA, which passthrough requires). Passthrough and Passthrough Camera Access enabled; package name
  `com.razvan.languageexchange`. `Build/Android` (Horizon OS manifest additions, icons) is versioned.
- `ALanguageExchangePlayerPawn`: camera locked to the headset, floor-level tracking origin, starts Meta's persistent
  passthrough (underlay). Start level: `/Game/LanguageExchange/Maps/Main` (empty level, so passthrough shows).
- Testing the webcam on PC now needs `-nohmd`, since the project starts in VR.
- Build + install on the headset (USB): `RunUAT BuildCookRun -project=<uproject> -platform=Android -cookflavor=ASTC
  -clientconfig=Development -build -cook -stage -pak -package -deploy -NoObbs -device=<adb serial>
  -nocompileeditor` (rebuild the editor target first when C++ changed; about 7 minutes).
  `-NoObbs` matters: the install copies the data as loose files, but once `Saved/StagedBuilds/Android_ASTC.obb`
  exists, the APK otherwise expects an OBB in `/sdcard/Android/obb/` and hangs on Unreal's downloader screen
  (`DownloaderActivity`, a hidden "No OBB found" dialog). Apps started while the headset sleeps (not worn) can hang
  the same way; start them while wearing it. If the headset isn't listed by `adb devices`, check Device Manager for a
  USB root hub with error 43 (fixed by restarting Windows). The app's log on the headset:
  `/sdcard/Android/data/com.razvan.languageexchange/files/UnrealGame/LanguageExchange/LanguageExchange/Saved/Logs/`;
  frame stats: `adb logcat -s VrApi`.
- VR gotcha: don't add UMG widgets to the viewport (`AddToViewport`) when a headset is active. On Quest, Slate draws
  them into the left eye's image, which then shows none of the scene (only passthrough); the right eye looks fine.
  The 2D detection overlay is therefore only created without a headset.
- Android gotcha: the launcher's prebuilt engine uses emulated thread-locals, our code (NDK API 32, required by Meta)
  native ones. Engine header code that reads an engine `thread_local` directly fails to link
  (`undefined symbol: LowLevelTasks::FTask::ActiveTask` from `UE::Tasks::Launch`). Use
  `Async(EAsyncExecution::ThreadPool, ...)` for background work instead.

### Step 3: model on the Quest (option A: ONNX Runtime for Android)

- NNE has no Android CPU runtime, so on Android `FYoloModel` runs through `FOnnxRuntimeModelRunner` (ONNX Runtime
  1.32.0's C API; library and headers in `Source/ThirdParty/LanguageExchangeOnnxRuntimeAndroid`, see its README).
  On PC it still uses `FNneModelRunner`. Both implement `IModelRunner`; pre-processing and decoding stay in
  `FYoloModel`. The Android model is the plain file `Content/LanguageExchange/Detection/Onnx/yolo11n.onnx`, packaged
  through `DirectoriesToAlwaysStageAsUFS` (cooked NNE assets don't keep the ONNX bytes).
- Camera: `ULanguageExchangeCameraSource` with `ULanguageExchangeHeadsetCameraSource` (MRUK passthrough camera,
  640 x 480, asks for `horizonos.permission.HEADSET_CAMERA`) when a headset is active, else
  `ULanguageExchangeWebcamSource`. The headset camera images are correct (checked with a recording).
- Testing on the headset without rebuilding: push a `UECommandLine.txt` (replaces the whole command line, so start
  with `-project="../../../LanguageExchange/LanguageExchange.uproject"`) to
  `/sdcard/Android/data/com.razvan.languageexchange/files/UnrealGame/LanguageExchange/`, e.g. with
  `-ini:Game:[/Script/LanguageExchange.LanguageExchangeDetectionSettings]:bUseXnnpack=True` or
  `-DPCVars=LanguageExchange.Detection.Record=1`. (`adb shell setprop debug.ue.commandline` also works but is
  limited to 92 characters.) Recordings are under `.../LanguageExchange/Saved/Detection/Recordings/` on the headset.
- Measured on Quest 3, YOLO11n at 640 px, detection every 0.25 s, 72 Hz:

  | ONNX Runtime | Inference | Stale frames/s |
  |---|---|---|
  | CPU provider, 4 threads | 176 ms | 4.7 |
  | XNNPACK, 4 threads | 160 ms | 3.2 |
  | **XNNPACK, 2 threads (default)** | **165 ms** | **0.1** |
  | CPU provider, 2 threads | 180 ms | 0.7 |

  More threads take cores from rendering. About 6x slower than the PC (30 ms).
- Model sizes on Quest 3 (XNNPACK, 2 threads), with recognition from running the same models on PC over 209 recorded
  headset-camera frames (frames with confidence >= 0.6):

  | Model | Inference | Stale/s | cup | bottle | tv | mouse (small) | keyboard |
  |---|---|---|---|---|---|---|---|
  | YOLO11n 640 | 164 ms | 0.4 | 31 | 38 | 31 | 14 | 1 |
  | YOLO11n 320 | 50 ms | 0.6 | 36 | 28 | 30 | 1 | 3 |
  | YOLO11s 640 | 435 ms | 1.5 | 51 | 53 | 83 | 9 | 9 |
  | YOLO11s 320 | 119 ms | 0.4 | 49 | 51 | 65 | 2 | 7 |

  320 px is 3-4x faster but loses small objects. YOLO11s 320 beats YOLO11n 640 on speed and on everything but small
  objects. Idea for the selection step: detect only on a crop around where the player points, so small objects fill
  the (small, fast) model input.
- Speed tests on the headset: `Tools/Quest/RunQuestBenchmark.ps1` runs the installed app with several settings or
  models in a row (headset on, about 1 minute per variant) and prints inference time and stale frames per variant.
- Test models (`yolo11n_320.onnx`, `yolo11s_640.onnx`, `yolo11s_320.onnx`) are exported with
  `YOLO("Models/<name>.pt").export(format="onnx", imgsz=<size>, simplify=True)` in `Tools/ObjectDetection`.
- Detections from the headset camera: cup 0.94, bottle 0.92, monitor as `tv` 0.87, and the mouse lying on the desk
  as `mouse` 0.86 (it wasn't recognised held in the air). Dim, backlit objects are missed.

### Step 3 results so far

- Passthrough app runs on the Quest 3: steady 72 FPS, about 2.3 ms GPU per frame for the (empty) app. Fast head
  movement looks blurry; that's the passthrough cameras, not the app. Later: raise the display rate to 90/120 Hz.
- Expected errors until the next steps: "No camera found" (webcam path) and no NNE CPU runtime on Android (IREE not
  enabled yet). MRUK logs `PcaCameraOpenXR::isSupported XR_ERROR_FUNCTION_UNSUPPORTED` at startup; check whether
  camera access falls back to Camera2 when we use it.

## Device plan

- A (goal): standalone Quest app, camera and model both on the headset.
- B (fallback if the Quest is too slow): still a standalone app, but camera frames go over Wi-Fi to a PC/server
  for detection (adds about 50–150 ms; camera images leave the headset).
- C: PCVR via Link: not the product, but useful for development. Meta's MRUK docs say the Passthrough Camera API
  works over Meta Horizon Link v201+; confirmed here (see "Testing over Link").
  PCVR is useful for the non-camera parts too (UI, multiplayer, word lists).
