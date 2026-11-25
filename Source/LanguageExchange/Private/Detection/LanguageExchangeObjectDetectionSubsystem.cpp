#include "Detection/LanguageExchangeObjectDetectionSubsystem.h"

#include "Async/Async.h"
#include "Camera/LanguageExchangeHeadsetCameraSource.h"
#include "Camera/LanguageExchangeWebcamSource.h"
#include "Detection/CocoClassNames.h"
#include "Detection/DetectionJob.h"
#include "Detection/LanguageExchangeDetectionSettings.h"
#include "Detection/YoloModel.h"
#include "Engine/AssetManager.h"
#include "Engine/Canvas.h"
#include "Engine/StreamableManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "LanguageExchange.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NNEModelData.h"
#include "RenderingThread.h"
#include "RHIGPUReadback.h"
#include "TextureResource.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "LanguageExchangeObjectDetection"

using LanguageExchange::Detection::FDetectionJob;
using LanguageExchange::Detection::FYoloModel;

namespace LanguageExchange::Detection
{
	namespace
	{
		// Ultralytics pads letterboxed images with this grey, so the model is used to it
		const FColor LetterboxColor(114, 114, 114);

		TAutoConsoleVariable<bool> RecordVariable(
			TEXT("LanguageExchange.Detection.Record"),
			false,
			TEXT("While on, saves each image the detection model sees and what it found to "
				"Saved/Detection/Recordings/<date-time>/ (see Tools/ObjectDetection/review_recording.py)"));

		// Render thread: copies the model input to the job once the GPU readback has finished
		void ReadPixelsIfReady(FDetectionJob& Job)
		{
			if (!Job.Readback->IsReady())
			{
				return;
			}

			int32 RowPitchInPixels = 0;
			const FColor* const Source = static_cast<const FColor*>(Job.Readback->Lock(RowPitchInPixels));
			if (Source == nullptr)
			{
				UE_LOG(LogLanguageExchange, Error, TEXT("Reading the model input back from the GPU failed"));
				Job.bArePixelsReady = true;
				return;
			}

			const int32 Size = Job.InputSizePixels;
			Job.Pixels.SetNumUninitialized(Size * Size);
			for (int32 Row = 0; Row < Size; ++Row)
			{
				FMemory::Memcpy(&Job.Pixels[Row * Size], Source + Row * RowPitchInPixels, Size * sizeof(FColor));
			}
			Job.Readback->Unlock();

			if (Job.bIsRedFirst)
			{
				for (FColor& Pixel : Job.Pixels)
				{
					Swap(Pixel.R, Pixel.B);
				}
			}
			Job.bArePixelsReady = true;
		}

		void SaveModelInput(const FDetectionJob& Job, const FString& FilePath)
		{
			const FImageView Image(Job.Pixels.GetData(), Job.InputSizePixels, Job.InputSizePixels);
			if (!FImageUtils::SaveImageByExtension(*FilePath, Image))
			{
				UE_LOG(LogLanguageExchange, Error, TEXT("Saving the model input to %s failed"), *FilePath);
			}
		}

		// One JSON line per frame; bounds are normalised to the model input image
		FString DescribeFrameAsJson(const FString& FrameFileName, const FDetectionJob& Job)
		{
			TArray<FString> DetectionEntries;
			for (const FLanguageExchangeDetection& Detection : Job.Detections)
			{
				const FBox2D& Bounds = Detection.NormalizedBounds;
				DetectionEntries.Add(FString::Printf(
					TEXT("{\"class\":\"%s\",\"confidence\":%.3f,\"box\":[%.4f,%.4f,%.4f,%.4f]}"),
					*Detection.ClassName.ToString(), Detection.Confidence,
					Bounds.Min.X, Bounds.Min.Y, Bounds.Max.X, Bounds.Max.Y));
			}
			return FString::Printf(TEXT("{\"frame\":\"%s\",\"inferenceMilliseconds\":%.1f,\"detections\":[%s]}\n"),
				*FrameFileName, Job.InferenceMilliseconds, *FString::Join(DetectionEntries, TEXT(",")));
		}
	}
}

void ULanguageExchangeObjectDetectionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	OpenCamera();
	LoadModelAsync();

	const ULanguageExchangeDetectionSettings* const Settings = GetDefault<ULanguageExchangeDetectionSettings>();
	InWorld.GetTimerManager().SetTimer(DetectionTimerHandle, this, &ThisClass::HandleDetectionTimer,
		Settings->GetDetectionIntervalSeconds(), true);
}

void ULanguageExchangeObjectDetectionSubsystem::Deinitialize()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	if (ModelLoadHandle.IsValid())
	{
		ModelLoadHandle->CancelHandle();
		ModelLoadHandle.Reset();
	}

	if (bIsInferenceRunning)
	{
		InferenceFuture.Wait();
		bIsInferenceRunning = false;
	}

	if (CurrentJob.IsValid())
	{
		// Queued render commands still use the render target
		FlushRenderingCommands();
		CurrentJob.Reset();
	}

	if (IsValid(CameraSource))
	{
		CameraSource->Close();
	}

	if (InferenceCount > 1)
	{
		UE_LOG(LogLanguageExchange, Log,
			TEXT("Detection summary: first run %.1f ms, then %d runs averaging %.1f ms"),
			FirstInferenceMilliseconds, InferenceCount - 1, InferenceMillisecondsTotal / (InferenceCount - 1));
	}

	Model.Reset();

	Super::Deinitialize();
}

void ULanguageExchangeObjectDetectionSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsInferenceRunning)
	{
		if (InferenceFuture.IsReady())
		{
			FinishInference();
		}
		return;
	}

	if (CurrentJob->bArePixelsReady)
	{
		StartInference();
		return;
	}

	QueueReadbackPoll();
}

bool ULanguageExchangeObjectDetectionSubsystem::IsTickable() const
{
	return CurrentJob.IsValid();
}

TStatId ULanguageExchangeObjectDetectionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULanguageExchangeObjectDetectionSubsystem, STATGROUP_Tickables);
}

UTexture* ULanguageExchangeObjectDetectionSubsystem::GetCameraTexture() const
{
	if (!IsValid(CameraSource))
	{
		return nullptr;
	}
	return CameraSource->GetTexture();
}

FIntPoint ULanguageExchangeObjectDetectionSubsystem::GetCameraSizePixels() const
{
	if (!IsValid(CameraSource))
	{
		return FIntPoint::ZeroValue;
	}
	return CameraSource->GetSizePixels();
}

FText ULanguageExchangeObjectDetectionSubsystem::GetStatusText() const
{
	if (!ErrorText.IsEmpty())
	{
		return ErrorText;
	}
	if (IsValid(CameraSource) && !CameraSource->GetErrorText().IsEmpty())
	{
		return CameraSource->GetErrorText();
	}
	if (!IsValid(CameraSource) || !CameraSource->IsOpen())
	{
		return LOCTEXT("OpeningCamera", "Opening the camera...");
	}
	if (!Model.IsValid())
	{
		return LOCTEXT("LoadingModel", "Loading the detection model...");
	}
	return FText::GetEmpty();
}

bool ULanguageExchangeObjectDetectionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULanguageExchangeObjectDetectionSubsystem::HandleModelLoad()
{
	ModelLoadHandle.Reset();

	const ULanguageExchangeDetectionSettings* const Settings = GetDefault<ULanguageExchangeDetectionSettings>();
	UNNEModelData* const ModelData = Settings->GetModel().Get();
	if (!IsValid(ModelData))
	{
		UE_LOG(LogLanguageExchange, Error, TEXT("Loading the detection model %s failed"),
			*Settings->GetModel().ToString());
		ErrorText = LOCTEXT("ModelLoadFail", "The detection model couldn't be loaded (see the log)");
		return;
	}

	Model = FYoloModel::Create(ModelData, Settings->GetRuntimeName());
	if (!Model.IsValid())
	{
		ErrorText = LOCTEXT("ModelCreateFail", "The detection model couldn't be started (see the log)");
		return;
	}

	TArray<int32> IgnoredClassIndices;
	for (const FName ClassName : Settings->GetIgnoredClassNames())
	{
		const int32 ClassIndex = LanguageExchange::Detection::FindCocoClassIndex(ClassName);
		if (ClassIndex == INDEX_NONE)
		{
			UE_LOG(LogLanguageExchange, Warning, TEXT("Ignored class '%s' isn't a COCO class"), *ClassName.ToString());
			continue;
		}
		IgnoredClassIndices.Add(ClassIndex);
		UE_LOG(LogLanguageExchange, Log, TEXT("Ignoring class '%s'"), *ClassName.ToString());
	}
	Model->SetIgnoredClassIndices(IgnoredClassIndices);

	const int32 InputSizePixels = Model->GetInputSizePixels();
	ModelInputTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, InputSizePixels, InputSizePixels,
		RTF_RGBA8_SRGB, FLinearColor(LanguageExchange::Detection::LetterboxColor));

	const EPixelFormat PixelFormat = ModelInputTarget->GetFormat();
	if (!ensureMsgf(PixelFormat == PF_B8G8R8A8 || PixelFormat == PF_R8G8B8A8,
		TEXT("Unexpected model input pixel format %s"), GetPixelFormatString(PixelFormat)))
	{
		Model.Reset();
		ErrorText = LOCTEXT("PixelFormatFail", "Unsupported render target format (see the log)");
	}
}

void ULanguageExchangeObjectDetectionSubsystem::HandleDetectionTimer()
{
	// Skip this round if the previous frame is still on its way
	if (!IsValid(CameraSource) || !CameraSource->IsOpen() || !Model.IsValid() || CurrentJob.IsValid())
	{
		return;
	}
	StartFrameCapture();
}

void ULanguageExchangeObjectDetectionSubsystem::OpenCamera()
{
	// With a headset (on the Quest, or over Link) detect on what the player sees; otherwise use a webcam
	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		UE_LOG(LogLanguageExchange, Log, TEXT("Headset active: using the headset's passthrough camera"));
		CameraSource = NewObject<ULanguageExchangeHeadsetCameraSource>(this);
	}
	else
	{
		UE_LOG(LogLanguageExchange, Log, TEXT("No headset active: using a webcam"));
		CameraSource = NewObject<ULanguageExchangeWebcamSource>(this);
	}
	CameraSource->Open();
}

void ULanguageExchangeObjectDetectionSubsystem::LoadModelAsync()
{
	const TSoftObjectPtr<UNNEModelData>& ModelReference = GetDefault<ULanguageExchangeDetectionSettings>()->GetModel();
	if (ModelReference.IsNull())
	{
		UE_LOG(LogLanguageExchange, Error, TEXT("No detection model set in the project settings"));
		ErrorText = LOCTEXT("NoModel",
			"No detection model set (Project Settings > LanguageExchange > Object Detection)");
		return;
	}

	ModelLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(ModelReference.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &ThisClass::HandleModelLoad));
}

void ULanguageExchangeObjectDetectionSubsystem::StartFrameCapture()
{
	UTexture* const CameraTexture = CameraSource->GetTexture();
	const FIntPoint CameraSizePixels = CameraSource->GetSizePixels();
	const int32 CameraWidth = CameraSizePixels.X;
	const int32 CameraHeight = CameraSizePixels.Y;
	if (!IsValid(CameraTexture) || CameraWidth <= 0 || CameraHeight <= 0)
	{
		// No camera frame yet
		return;
	}

	if (!bHasCapturedFrame)
	{
		bHasCapturedFrame = true;
		UE_LOG(LogLanguageExchange, Log, TEXT("First camera frame for detection: %d x %d"), CameraWidth,
			CameraHeight);
	}

	// Letterbox: scale the camera image to fit the square model input and centre it
	const int32 InputSizePixels = ModelInputTarget->SizeX;
	const double InputSize = InputSizePixels;
	const double Scale = InputSize / FMath::Max(CameraWidth, CameraHeight);
	const FVector2D ContentSizePixels(CameraWidth * Scale, CameraHeight * Scale);
	const FVector2D ContentOffsetPixels = (FVector2D(InputSize) - ContentSizePixels) * 0.5;

	UKismetRenderingLibrary::ClearRenderTarget2D(this, ModelInputTarget,
		FLinearColor(LanguageExchange::Detection::LetterboxColor));
	UCanvas* Canvas = nullptr;
	FVector2D CanvasSize;
	FDrawToRenderTargetContext DrawContext;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, ModelInputTarget, Canvas, CanvasSize, DrawContext);
	Canvas->K2_DrawTexture(CameraTexture, ContentOffsetPixels, ContentSizePixels, FVector2D::ZeroVector,
		FVector2D::UnitVector, FLinearColor::White, BLEND_Opaque);
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, DrawContext);

	const TSharedRef<FDetectionJob> Job = MakeShared<FDetectionJob>();
	Job->InputSizePixels = InputSizePixels;
	Job->ContentOffset = ContentOffsetPixels / InputSize;
	Job->ContentSize = ContentSizePixels / InputSize;
	Job->bIsRedFirst = ModelInputTarget->GetFormat() == PF_R8G8B8A8;
	Job->Readback = MakeShared<FRHIGPUTextureReadback>(TEXT("LanguageExchangeModelInput"));

	FTextureRenderTargetResource* const TargetResource = ModelInputTarget->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(LanguageExchangeCopyModelInput)(
		[Job, TargetResource](FRHICommandListImmediate& RHICommandList)
		{
			FRHITexture* const Texture = TargetResource->GetRenderTargetTexture();
			RHICommandList.Transition(FRHITransitionInfo(Texture, ERHIAccess::Unknown, ERHIAccess::CopySrc));
			Job->Readback->EnqueueCopy(RHICommandList, Texture);
			RHICommandList.Transition(FRHITransitionInfo(Texture, ERHIAccess::CopySrc, ERHIAccess::SRVMask));
		});

	CurrentJob = Job;
}

void ULanguageExchangeObjectDetectionSubsystem::QueueReadbackPoll()
{
	// One poll in flight at a time
	if (CurrentJob->bIsReadbackPollQueued.exchange(true))
	{
		return;
	}

	ENQUEUE_RENDER_COMMAND(LanguageExchangeReadModelInput)(
		[Job = CurrentJob.ToSharedRef()](FRHICommandListImmediate&)
		{
			LanguageExchange::Detection::ReadPixelsIfReady(*Job);
			Job->bIsReadbackPollQueued = false;
		});
}

void ULanguageExchangeObjectDetectionSubsystem::StartInference()
{
	if (CurrentJob->Pixels.IsEmpty())
	{
		// The readback failed (already logged); try again with the next frame
		CurrentJob.Reset();
		return;
	}

	const ULanguageExchangeDetectionSettings* const Settings = GetDefault<ULanguageExchangeDetectionSettings>();
	const float MaximumBoxOverlap = Settings->GetMaximumBoxOverlap();

	// A recorded frame keeps weaker detections too, so near misses show up in the recording
	CurrentJob->bIsRecorded = LanguageExchange::Detection::RecordVariable.GetValueOnGameThread();
	float MinimumConfidence = Settings->GetMinimumConfidence();
	if (CurrentJob->bIsRecorded)
	{
		MinimumConfidence = FMath::Min(MinimumConfidence, Settings->GetRecordingMinimumConfidence());
	}

	// The work owns shared references, so it stays valid even if the subsystem goes away first.
	// The thread pool instead of UE::Tasks::Launch: on Android, UE::Tasks' header code reads an engine thread_local
	// directly, and our module (NDK API 32, native TLS) can't link against the prebuilt engine (emulated TLS).
	InferenceFuture = Async(EAsyncExecution::ThreadPool,
		[Job = CurrentJob.ToSharedRef(), DetectionModel = Model.ToSharedRef(), MinimumConfidence, MaximumBoxOverlap]()
		{
			Job->Detections = DetectionModel->Detect(Job->Pixels, MinimumConfidence, MaximumBoxOverlap,
				Job->InferenceMilliseconds);
		});
	bIsInferenceRunning = true;
}

void ULanguageExchangeObjectDetectionSubsystem::FinishInference()
{
	bIsInferenceRunning = false;
	const TSharedPtr<FDetectionJob> Job = MoveTemp(CurrentJob);
	CurrentJob.Reset();

	InferenceMilliseconds = Job->InferenceMilliseconds;
	++InferenceCount;
	if (InferenceCount == 1)
	{
		FirstInferenceMilliseconds = InferenceMilliseconds;
	}
	else
	{
		InferenceMillisecondsTotal += InferenceMilliseconds;
	}

	if (Job->bIsRecorded)
	{
		RecordFrame(Job.ToSharedRef());
	}

	const float MinimumConfidence = GetDefault<ULanguageExchangeDetectionSettings>()->GetMinimumConfidence();
	Detections.Reset();
	for (FLanguageExchangeDetection Detection : Job->Detections)
	{
		if (Detection.Confidence < MinimumConfidence)
		{
			continue;
		}

		// From model input coordinates to camera image coordinates (undo the letterbox)
		FBox2D& Bounds = Detection.NormalizedBounds;
		Bounds.Min = ((Bounds.Min - Job->ContentOffset) / Job->ContentSize).ClampAxes(0.0, 1.0);
		Bounds.Max = ((Bounds.Max - Job->ContentOffset) / Job->ContentSize).ClampAxes(0.0, 1.0);
		Detections.Add(Detection);
	}

	LogDetectionsIfChanged();
}

void ULanguageExchangeObjectDetectionSubsystem::RecordFrame(const TSharedRef<FDetectionJob>& Job)
{
	if (RecordingDirectory.IsEmpty())
	{
		RecordingDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
			/ TEXT("Detection/Recordings") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		UE_LOG(LogLanguageExchange, Log, TEXT("Recording detections to %s"), *RecordingDirectory);
	}

	++RecordedFrameCount;
	const FString FrameFileName = FString::Printf(TEXT("Frame%05d.png"), RecordedFrameCount);

	// Encoding a PNG takes a while, so it happens on a worker thread (thread pool: see StartInference)
	Async(EAsyncExecution::ThreadPool, [Job, FilePath = RecordingDirectory / FrameFileName]()
	{
		LanguageExchange::Detection::SaveModelInput(*Job, FilePath);
	});

	FFileHelper::SaveStringToFile(LanguageExchange::Detection::DescribeFrameAsJson(FrameFileName, *Job),
		*(RecordingDirectory / TEXT("Detections.jsonl")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(), FILEWRITE_Append);
}

void ULanguageExchangeObjectDetectionSubsystem::LogDetectionsIfChanged()
{
	TArray<FString> ClassNames;
	for (const FLanguageExchangeDetection& Detection : Detections)
	{
		ClassNames.AddUnique(Detection.ClassName.ToString());
	}
	ClassNames.Sort();
	const FString JoinedClassNames = FString::Join(ClassNames, TEXT(", "));
	if (JoinedClassNames == LastLoggedClassNames)
	{
		return;
	}
	LastLoggedClassNames = JoinedClassNames;

	TArray<FString> Descriptions;
	for (const FLanguageExchangeDetection& Detection : Detections)
	{
		Descriptions.Add(FString::Printf(TEXT("%s %.2f"), *Detection.ClassName.ToString(), Detection.Confidence));
	}
	UE_LOG(LogLanguageExchange, Log, TEXT("Detected: %s (inference %.1f ms)"),
		Descriptions.IsEmpty() ? TEXT("nothing") : *FString::Join(Descriptions, TEXT(", ")), InferenceMilliseconds);
}

#undef LOCTEXT_NAMESPACE
