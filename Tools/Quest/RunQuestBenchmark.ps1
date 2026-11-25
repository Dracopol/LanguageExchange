# Runs the installed Quest app with several detection settings, one after another, and summarises the inference time
# (from the app's "Inference over the last 20 runs" log lines) and VR frame stats (VrApi logcat lines).
#
# Usage: install the app (BuildCookRun ... -deploy), connect the headset over USB, put it on (the app pauses when
# it's off), then run: powershell -File Tools/Quest/RunQuestBenchmark.ps1 [-SecondsPerVariant 45]
# Edit $variants below. Every model file must be in Content/LanguageExchange/Detection/Onnx when the app is packaged.
# Settings are passed as -ini: overrides in UECommandLine.txt on the headset, so no rebuild is needed between variants.
param(
	[int] $SecondsPerVariant = 45
)

$package = "com.razvan.languageexchange"
$gameDirectory = "/sdcard/Android/data/$package/files/UnrealGame/LanguageExchange"
$remoteLog = "$gameDirectory/LanguageExchange/Saved/Logs/LanguageExchange.log"
# Replaces the whole command line (system properties are limited to 92 characters, too short for -ini: overrides)
$remoteCommandLineFile = "$gameDirectory/UECommandLine.txt"
$section = "[/Script/LanguageExchange.LanguageExchangeDetectionSettings]"
$localLog = Join-Path $env:TEMP "LanguageExchangeBenchmarkLog.txt"
$localCommandLineFile = Join-Path $env:TEMP "LanguageExchangeUECommandLine.txt"

# Clear the setprop override from earlier runs; it would be appended to the file's command line
adb shell setprop debug.ue.commandline "''" | Out-Null

$modelFolder = "LanguageExchange/Detection/Onnx"
$variants = @(
	@{ Name = "YOLO11n 640"; Xnnpack = "True"; Threads = 2; Model = "$modelFolder/yolo11n.onnx" },
	@{ Name = "YOLO11n 320"; Xnnpack = "True"; Threads = 2; Model = "$modelFolder/yolo11n_320.onnx" },
	@{ Name = "YOLO11s 640"; Xnnpack = "True"; Threads = 2; Model = "$modelFolder/yolo11s_640.onnx" },
	@{ Name = "YOLO11s 320"; Xnnpack = "True"; Threads = 2; Model = "$modelFolder/yolo11s_320.onnx" }
)

$results = @()
foreach ($variant in $variants) {
	$commandLine = "-project=`"../../../LanguageExchange/LanguageExchange.uproject`" " +
		"-ini:Game:$($section):bUseXnnpack=$($variant.Xnnpack),$($section):OnnxRuntimeThreadCount=$($variant.Threads)," +
		"$($section):OnnxModelFile=$($variant.Model)"
	[IO.File]::WriteAllText($localCommandLineFile, $commandLine)
	adb push $localCommandLineFile $remoteCommandLineFile 2>&1 | Out-Null
	adb shell am force-stop $package | Out-Null
	adb logcat -c | Out-Null
	adb shell monkey -p $package -c android.intent.category.LAUNCHER 1 2>&1 | Out-Null
	Write-Host "Running '$($variant.Name)' for $SecondsPerVariant s..."
	Start-Sleep -Seconds $SecondsPerVariant

	adb pull $remoteLog $localLog 2>&1 | Out-Null
	$log = Get-Content $localLog
	$runner = ($log | Select-String -Pattern 'Detection model ready on (.+?):' | Select-Object -First 1).Matches.Groups[1].Value
	# The first window includes warm-up
	$windows = $log | Select-String -Pattern 'Inference over the last \d+ runs: average ([\d\.]+) ms, maximum ([\d\.]+) ms' | Select-Object -Skip 1
	$averages = $windows | ForEach-Object { [double] $_.Matches.Groups[1].Value }
	$maximums = $windows | ForEach-Object { [double] $_.Matches.Groups[2].Value }

	$stats = adb logcat -d -s VrApi:* 2>$null | Select-String -Pattern 'FPS=(\d+)/\d+.*?Stale=(\d+)'
	$stale = $stats | Select-Object -Skip 5 | ForEach-Object { [int] $_.Matches.Groups[2].Value }
	$fps = $stats | Select-Object -Skip 5 | ForEach-Object { [int] $_.Matches.Groups[1].Value }

	$results += [pscustomobject]@{
		Variant           = $variant.Name
		Runner            = $runner
		"Windows"         = $averages.Count
		"Average ms"      = if ($averages) { [math]::Round(($averages | Measure-Object -Average).Average, 1) } else { $null }
		"Max ms"          = if ($maximums) { [math]::Round(($maximums | Measure-Object -Maximum).Maximum, 1) } else { $null }
		"FPS"             = if ($fps) { [math]::Round(($fps | Measure-Object -Average).Average, 1) } else { $null }
		"Stale per s"     = if ($stale) { [math]::Round(($stale | Measure-Object -Average).Average, 1) } else { $null }
	}
}

adb shell rm -f $remoteCommandLineFile | Out-Null
adb shell am force-stop $package | Out-Null
$results | Format-Table -AutoSize | Out-String -Width 200
