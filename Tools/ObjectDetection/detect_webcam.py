"""Live object detection on the webcam with a pretrained COCO model (prototype step 1).

Shows the webcam image with a box and class name around each detected object. Press Q or Esc to quit; a summary of
what was detected is printed to the console on exit.

Usage:
	python detect_webcam.py                                # YOLO11n PyTorch model
	python detect_webcam.py --export-onnx                  # export Models/yolo11n.onnx, then exit
	python detect_webcam.py --model Models/yolo11n.onnx    # same model through onnxruntime
"""

import argparse
import time
from dataclasses import dataclass
from pathlib import Path

import cv2
from ultralytics import YOLO

SCRIPT_DIRECTORY = Path(__file__).resolve().parent
DEFAULT_MODEL_PATH = SCRIPT_DIRECTORY / "Models" / "yolo11n.pt"
MODEL_INPUT_SIZE_PIXELS = 640
WINDOW_TITLE = "LanguageExchange - object detection (Q to quit)"

# COCO classes for the objects we test with: a computer mouse, and a glass of water (cup, wine glass or bottle)
TARGET_CLASS_NAMES = {"mouse", "cup", "wine glass", "bottle"}

TARGET_COLOR = (0, 220, 0)
OTHER_COLOR = (160, 160, 160)
TEXT_COLOR = (0, 0, 0)
ESCAPE_KEY = 27


@dataclass
class ClassStatistics:
	frame_count: int = 0
	best_confidence: float = 0.0


def parse_arguments() -> argparse.Namespace:
	parser = argparse.ArgumentParser(description="Live webcam object detection with a pretrained COCO model.")
	parser.add_argument("--model", type=Path, default=DEFAULT_MODEL_PATH, help="A .pt or .onnx model file.")
	parser.add_argument("--camera", type=int, default=0, help="Webcam index.")
	parser.add_argument("--confidence", type=float, default=0.4, help="Minimum confidence to show a detection.")
	parser.add_argument("--export-onnx", action="store_true", help="Export the .pt model to ONNX, then exit.")
	return parser.parse_args()


def export_onnx(model_path: Path) -> None:
	if model_path.suffix != ".pt":
		raise SystemExit(f"--export-onnx needs a .pt model, got {model_path}")
	model_path.parent.mkdir(parents=True, exist_ok=True)
	model = YOLO(str(model_path))
	exported_path = model.export(format="onnx", imgsz=MODEL_INPUT_SIZE_PIXELS, simplify=True)
	print(f"Exported ONNX model: {exported_path}")


def open_camera(camera_index: int) -> cv2.VideoCapture:
	# DirectShow opens Windows webcams much faster than the default backend
	capture = cv2.VideoCapture(camera_index, cv2.CAP_DSHOW)
	if not capture.isOpened():
		raise SystemExit(f"Could not open webcam {camera_index}. Is another app using it?")
	capture.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
	capture.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)
	return capture


def draw_label(frame, text: str, left: int, top: int, color) -> None:
	font = cv2.FONT_HERSHEY_SIMPLEX
	scale = 0.6
	thickness = 2
	(text_width, text_height), baseline = cv2.getTextSize(text, font, scale, thickness)
	label_top = max(top - text_height - baseline - 4, 0)
	cv2.rectangle(frame, (left, label_top), (left + text_width + 6, label_top + text_height + baseline + 4), color, -1)
	cv2.putText(frame, text, (left + 3, label_top + text_height + 2), font, scale, TEXT_COLOR, thickness)


def draw_detections(frame, result, class_names: dict, statistics: dict) -> None:
	seen_this_frame = {}
	for box, class_index, confidence in zip(
		result.boxes.xyxy.tolist(), result.boxes.cls.tolist(), result.boxes.conf.tolist()
	):
		class_name = class_names[int(class_index)]
		is_target = class_name in TARGET_CLASS_NAMES
		color = TARGET_COLOR if is_target else OTHER_COLOR
		left, top, right, bottom = (int(value) for value in box)
		cv2.rectangle(frame, (left, top), (right, bottom), color, 3 if is_target else 1)
		draw_label(frame, f"{class_name} {confidence:.2f}", left, top, color)
		seen_this_frame[class_name] = max(seen_this_frame.get(class_name, 0.0), confidence)

	for class_name, confidence in seen_this_frame.items():
		class_statistics = statistics.setdefault(class_name, ClassStatistics())
		class_statistics.frame_count += 1
		class_statistics.best_confidence = max(class_statistics.best_confidence, confidence)


def draw_overlay(frame, model_name: str, frames_per_second: float, inference_milliseconds: float) -> None:
	text = f"{model_name} | {frames_per_second:.1f} FPS | inference {inference_milliseconds:.1f} ms"
	draw_label(frame, text, 10, 34, (255, 255, 255))


def print_summary(model_path: Path, statistics: dict, frame_count: int, inference_milliseconds_total: float,
		duration_seconds: float) -> None:
	print()
	print(f"Model: {model_path.name}")
	print(f"Frames: {frame_count} in {duration_seconds:.1f} s ({frame_count / max(duration_seconds, 0.001):.1f} FPS)")
	print(f"Average inference: {inference_milliseconds_total / max(frame_count, 1):.1f} ms")
	print()
	if not statistics:
		print("No objects detected.")
		return
	print(f"{'Class':<16}{'Frames seen':>12}{'% of frames':>13}{'Best confidence':>17}")
	for class_name, class_statistics in sorted(statistics.items(), key=lambda item: -item[1].frame_count):
		marker = "  <- target" if class_name in TARGET_CLASS_NAMES else ""
		percentage = 100 * class_statistics.frame_count / max(frame_count, 1)
		print(f"{class_name:<16}{class_statistics.frame_count:>12}{percentage:>12.0f}%"
			f"{class_statistics.best_confidence:>17.2f}{marker}")


def run(model_path: Path, camera_index: int, minimum_confidence: float) -> None:
	model_path.parent.mkdir(parents=True, exist_ok=True)
	model = YOLO(str(model_path), task="detect")
	capture = open_camera(camera_index)

	statistics = {}
	frame_count = 0
	inference_milliseconds_total = 0.0
	frames_per_second = 0.0
	start_time = time.perf_counter()
	previous_frame_time = start_time

	try:
		while True:
			has_frame, frame = capture.read()
			if not has_frame:
				print("Webcam stopped sending frames.")
				break

			result = model.predict(frame, imgsz=MODEL_INPUT_SIZE_PIXELS, conf=minimum_confidence, verbose=False)[0]
			inference_milliseconds = result.speed["inference"]
			draw_detections(frame, result, result.names, statistics)

			now = time.perf_counter()
			instant_frames_per_second = 1.0 / max(now - previous_frame_time, 0.001)
			frames_per_second = instant_frames_per_second if frame_count == 0 else (
				0.9 * frames_per_second + 0.1 * instant_frames_per_second)
			previous_frame_time = now
			frame_count += 1
			inference_milliseconds_total += inference_milliseconds

			draw_overlay(frame, model_path.name, frames_per_second, inference_milliseconds)
			cv2.imshow(WINDOW_TITLE, frame)

			key = cv2.waitKey(1) & 0xFF
			is_window_closed = cv2.getWindowProperty(WINDOW_TITLE, cv2.WND_PROP_VISIBLE) < 1
			if key in (ord("q"), ESCAPE_KEY) or is_window_closed:
				break
	finally:
		capture.release()
		cv2.destroyAllWindows()

	print_summary(model_path, statistics, frame_count, inference_milliseconds_total,
		time.perf_counter() - start_time)


def main() -> None:
	arguments = parse_arguments()
	if arguments.export_onnx:
		export_onnx(arguments.model)
	else:
		run(arguments.model, arguments.camera, arguments.confidence)


if __name__ == "__main__":
	main()
