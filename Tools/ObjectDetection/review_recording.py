"""Turns a detection recording from Unreal into contact sheets and a per-class summary.

Record in the game with the console command `LanguageExchange.Detection.Record 1` (or start the game with
-ExecCmds="LanguageExchange.Detection.Record 1"). Frames and Detections.jsonl land in
Saved/Detection/Recordings/<date-time>/.

Usage:
	python review_recording.py <recording folder> [--every 2] [--confidence 0.6]

Green boxes reached the on-screen threshold; yellow boxes are near misses that were only recorded.
"""

import argparse
import json
from collections import defaultdict
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

THUMBNAIL_SIZE_PIXELS = 320
COLUMNS = 4
ROWS = 3
SHOWN_COLOR = (0, 220, 0)
NEAR_MISS_COLOR = (255, 210, 0)
LABEL_TEXT_COLOR = (0, 0, 0)


def parse_arguments() -> argparse.Namespace:
	parser = argparse.ArgumentParser(description="Contact sheets and a summary for a detection recording.")
	parser.add_argument("recording", type=Path, help="Folder with Frame*.png and Detections.jsonl.")
	parser.add_argument("--every", type=int, default=2, help="Use every Nth frame in the sheets.")
	parser.add_argument("--confidence", type=float, default=0.6, help="The on-screen threshold of that run.")
	parser.add_argument("--detections", type=Path,
		help="Detections file (default: <recording>/Detections.jsonl; compare_models.py writes others).")
	parser.add_argument("--output", type=Path,
		help="Where to write the sheets (default: a Sheets folder next to the detections file).")
	return parser.parse_args()


def load_frames(detections_file: Path) -> list[dict]:
	with open(detections_file, encoding="utf-8") as file:
		return [json.loads(line) for line in file if line.strip()]


def print_summary(frames: list[dict], shown_confidence: float) -> None:
	frames_seen = defaultdict(int)
	frames_shown = defaultdict(int)
	best_confidence = defaultdict(float)
	for frame in frames:
		seen = {}
		for detection in frame["detections"]:
			seen[detection["class"]] = max(seen.get(detection["class"], 0.0), detection["confidence"])
		for class_name, confidence in seen.items():
			frames_seen[class_name] += 1
			frames_shown[class_name] += confidence >= shown_confidence
			best_confidence[class_name] = max(best_confidence[class_name], confidence)

	inference_milliseconds = [frame["inferenceMilliseconds"] for frame in frames[1:]]
	print(f"Frames: {len(frames)}, average inference after the first: "
		f"{sum(inference_milliseconds) / max(len(inference_milliseconds), 1):.1f} ms")
	print(f"{'Class':<16}{'Seen':>8}{'Shown':>8}{'Best':>8}   (seen: any confidence recorded; "
		f"shown: >= {shown_confidence})")
	for class_name in sorted(frames_seen, key=lambda name: -frames_seen[name]):
		print(f"{class_name:<16}{frames_seen[class_name]:>8}{frames_shown[class_name]:>8}"
			f"{best_confidence[class_name]:>8.2f}")


def draw_frame(recording: Path, frame: dict, shown_confidence: float, font) -> Image.Image:
	image = Image.open(recording / frame["frame"]).convert("RGB")
	draw = ImageDraw.Draw(image)
	width, height = image.size
	# Weakest first, so the strongest boxes end up on top
	for detection in sorted(frame["detections"], key=lambda item: item["confidence"]):
		color = SHOWN_COLOR if detection["confidence"] >= shown_confidence else NEAR_MISS_COLOR
		left, top, right, bottom = detection["box"]
		box = (left * width, top * height, right * width, bottom * height)
		draw.rectangle(box, outline=color, width=4)
		label = f"{detection['class']} {detection['confidence']:.2f}"
		label_box = draw.textbbox((box[0], box[1]), label, font=font)
		label_top = max(box[1] - (label_box[3] - label_box[1]) - 6, 0)
		draw.rectangle((box[0], label_top, label_box[2] + 6, label_top + label_box[3] - label_box[1] + 6), fill=color)
		draw.text((box[0] + 3, label_top + 1), label, fill=LABEL_TEXT_COLOR, font=font)
	draw.text((8, 8), frame["frame"], fill=(255, 255, 255), font=font, stroke_width=2, stroke_fill=(0, 0, 0))
	return image.resize((THUMBNAIL_SIZE_PIXELS, THUMBNAIL_SIZE_PIXELS))


def write_sheets(recording: Path, frames: list[dict], every: int, shown_confidence: float, output: Path) -> None:
	output.mkdir(parents=True, exist_ok=True)
	font = ImageFont.load_default(size=28)
	selected = frames[::every]
	frames_per_sheet = COLUMNS * ROWS
	for sheet_index in range(0, len(selected), frames_per_sheet):
		sheet = Image.new("RGB", (COLUMNS * THUMBNAIL_SIZE_PIXELS, ROWS * THUMBNAIL_SIZE_PIXELS), (40, 40, 40))
		for position, frame in enumerate(selected[sheet_index:sheet_index + frames_per_sheet]):
			if not (recording / frame["frame"]).exists():
				continue
			thumbnail = draw_frame(recording, frame, shown_confidence, font)
			sheet.paste(thumbnail, ((position % COLUMNS) * THUMBNAIL_SIZE_PIXELS,
				(position // COLUMNS) * THUMBNAIL_SIZE_PIXELS))
		sheet_path = output / f"Sheet{sheet_index // frames_per_sheet + 1:02d}.png"
		sheet.save(sheet_path)
	print(f"Wrote {(len(selected) + frames_per_sheet - 1) // frames_per_sheet} sheets to {output}")


def main() -> None:
	arguments = parse_arguments()
	detections_file = arguments.detections or arguments.recording / "Detections.jsonl"
	frames = load_frames(detections_file)
	print_summary(frames, arguments.confidence)
	write_sheets(arguments.recording, frames, arguments.every, arguments.confidence,
		arguments.output or detections_file.parent / "Sheets")


if __name__ == "__main__":
	main()
