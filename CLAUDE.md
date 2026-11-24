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

## Prototype plan (current step first)

1. PC, Python, webcam: check that a pretrained detector recognises the user's objects (computer mouse, glass of water).
2. Unreal on PC, webcam: run the same model as ONNX through NNE and show the detected names.
3. Unreal on Quest: use the passthrough camera image and run the model on the headset; measure speed.

Riskiest open questions: passthrough camera access from Unreal (Meta's samples target Unity first), and NNE
performance on Quest.
