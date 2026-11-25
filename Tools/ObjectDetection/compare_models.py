"""Runs YOLO models over the frames of a detection recording from Unreal, to compare them on the same images.

For each model, writes <recording>/Models/<model>/Detections.jsonl in the same format Unreal records, so
review_recording.py can summarise it and draw contact sheets (--detections).

Usage:
	python compare_models.py <recording folder> [--models yolo11n yolo11s yolo11m] [--confidence 0.25]
		[--ignore person]
"""

import argparse
import json
import time
from pathlib import Path

from ultralytics import YOLO

SCRIPT_DIRECTORY = Path(__file__).resolve().parent
MODEL_DIRECTORY = SCRIPT_DIRECTORY / "Models"
MODEL_INPUT_SIZE_PIXELS = 640


def parse_arguments() -> argparse.Namespace:
	parser = argparse.ArgumentParser(description="Run several YOLO models over a recording's frames.")
	parser.add_argument("recording", type=Path, help="Folder with Frame*.png from Unreal.")
	parser.add_argument("--models", nargs="+", default=["yolo11n", "yolo11s", "yolo11m"])
	parser.add_argument("--confidence", type=float, default=0.25, help="Lowest confidence to keep.")
	parser.add_argument("--ignore", nargs="*", default=["person"], help="Class names to skip.")
	parser.add_argument("--size", type=int, default=MODEL_INPUT_SIZE_PIXELS,
		help="Model input size in pixels (e.g. 320 for a faster, less accurate model).")
	return parser.parse_args()


def run_model(model_name: str, frame_paths: list[Path], confidence: float, ignored_names: set[str],
		output_file: Path, size_pixels: int) -> None:
	model = YOLO(str(MODEL_DIRECTORY / f"{model_name}.pt"))
	kept_classes = [index for index, name in model.names.items() if name not in ignored_names]

	output_file.parent.mkdir(parents=True, exist_ok=True)
	inference_milliseconds = []
	start_seconds = time.perf_counter()
	with open(output_file, "w", encoding="utf-8") as file:
		for frame_path in frame_paths:
			result = model.predict(str(frame_path), imgsz=size_pixels, conf=confidence,
				classes=kept_classes, verbose=False)[0]
			inference_milliseconds.append(result.speed["inference"])
			detections = [
				{"class": result.names[int(class_index)], "confidence": round(float(score), 3),
					"box": [round(float(value), 4) for value in box]}
				for box, class_index, score in zip(
					result.boxes.xyxyn.tolist(), result.boxes.cls.tolist(), result.boxes.conf.tolist())
			]
			file.write(json.dumps({"frame": frame_path.name, "inferenceMilliseconds": result.speed["inference"],
				"detections": detections}) + "\n")

	# The first frame includes warm-up
	steady = inference_milliseconds[1:] or inference_milliseconds
	print(f"{model_name}: {len(frame_paths)} frames in {time.perf_counter() - start_seconds:.0f} s, "
		f"inference {sum(steady) / len(steady):.1f} ms on average -> {output_file}")


def main() -> None:
	arguments = parse_arguments()
	frame_paths = sorted(arguments.recording.glob("Frame*.png"))
	if not frame_paths:
		raise SystemExit(f"No Frame*.png in {arguments.recording}")
	for model_name in arguments.models:
		folder_name = model_name if arguments.size == MODEL_INPUT_SIZE_PIXELS else f"{model_name}_{arguments.size}"
		run_model(model_name, frame_paths, arguments.confidence, set(arguments.ignore),
			arguments.recording / "Models" / folder_name / "Detections.jsonl", arguments.size)


if __name__ == "__main__":
	main()
