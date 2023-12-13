/******************************************************************************/
/* Animation Generator for UE5.03                                             */
/* -------------------------------------------------------------------------- */
/* License MIT                                                                */
/* Kindly sponsored by IMVU                                                   */
/* -------------------------------------------------------------------------- */
/* This is a header only library that simplify the process of creating a      */
/* `USkeletalMeshComponent`, with many surfaces, at runtime.                  */
/* You can just pass all the surfaces' data, this library will take care to   */
/* correctly populate the UE4 buffers, needed to have a fully working         */
/* `USkeletalMeshComponent`.                                                  */
/******************************************************************************/
#include "RuntimeAnimationGenerator.h"

#include "AnimSequenceRuntime.h"
#include "AnimationUtils.h"
#include "Animation/AnimSequenceBase.h"

void FRuntimeAnimationGeneratorModule::StartupModule()
{
}

void FRuntimeAnimationGeneratorModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FRuntimeAnimationGeneratorModule, RuntimeAnimationGenerator)

void FRuntimeAnimationGenerator::PrepareTracks(const USkeleton* Skeleton, FTracks& OutTracks)
{
	OutTracks.IsReady = false;

	// Delete the empty tracks and wrong BoneName.
	for (int32 I = OutTracks.Tracks.Num() - 1; I >= 0; I -= 1)
	{
		const FName& BoneName = OutTracks.Tracks[I].BoneName;
		if (OutTracks.Tracks[I].KeyFrames.Num() == 0 ||
		    Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName) == INDEX_NONE)
		{
			OutTracks.Tracks.RemoveAt(I);
		}
	}

	// Delete duplicate KeyFrames
	for (int32 I = OutTracks.Tracks.Num() - 1; I >= 0; I -= 1)
	{
		for (int32 Y = OutTracks.Tracks[I].KeyFrames.Num() - 1; Y > 0; Y -= 1)
		{
			if (OutTracks.Tracks[I].KeyFrames[Y].Time == OutTracks.Tracks[I].KeyFrames[Y - 1].Time)
			{
				OutTracks.Tracks[I].KeyFrames.RemoveAt(Y);
			}
		}
	}

	// Sort the tracks.
	for (FTrack& Track : OutTracks.Tracks)
	{
		Track.KeyFrames.Sort();
	}

	// Make sure we have the frame 0.
	for (FTrack& Track : OutTracks.Tracks)
	{
		checkf(Track.KeyFrames.Num() > 0, TEXT("No empty tracks at this point"));

		// NeedZeroFrame
		if (Track.KeyFrames[0].Time != 0.0)
		{
			if (FMath::IsNearlyEqual(Track.KeyFrames[0].Time, 0.0f))
			{
				Track.KeyFrames[0].Time = 0.0;
			}
			else
			{
				FKeyFrame ZeroFrame(Track.KeyFrames[0]);
				ZeroFrame.Time = 0.0;
				Track.KeyFrames.Insert(ZeroFrame, 0);
			}
		}
	}

	OutTracks.IsReady = true;
}

UAnimSequence* FRuntimeAnimationGenerator::Generate(USkeleton* Skeleton, const FTracks& TracksContainer, UObject* Outer)
{
	if (!ensureAlwaysMsgf(TracksContainer.IsReady, TEXT("Please call `PrepareTracks` before this function.")))
	{
		return nullptr;
	}

	const TArray<FTrack>& Tracks = TracksContainer.Tracks;

	if (Tracks.Num() == 0)
	{
		// Nothing to do!
		return nullptr;
	}

	TArray<int32> TrackIndices;
	TrackIndices.Reserve(Tracks.Num());

	UAnimSequence* Anim = NewObject<UAnimSequence>(Outer);

	// ~~ Initialize the Animation ~~
	Anim->BoneCompressionSettings = FAnimationUtils::GetDefaultAnimationRecorderBoneCompressionSettings();
	Anim->SetSkeleton(Skeleton);
	Anim->SetSequenceLength(0.f);
	Anim->SetRawNumberOfFrame(0);

	// ~~ Initialize the Animation tracks ~~
	TArray<FName> RuntimeAnimationTrackNames;
	for (const FTrack& Track : Tracks)
	{
		// Pass `nullptr` so we can initialize it later.
		// It's safe to cast to `AnimSequenceRuntime` since it doesn't add any member.
		TrackIndices.Push(static_cast<UAnimSequenceRuntime*>(Anim)->AddNewRawTrackRuntime(Track.BoneName, nullptr, RuntimeAnimationTrackNames));
	}
#if WITH_EDITOR
	// ~~ Init notifies ~~
	Anim->InitializeNotifyTrack();
#endif
	// ~~ First find the sequence duration and frame interval. ~~
	float FrameInterval = FLT_MAX;
	float SequenceDuration = 0.0;
	for (const FTrack& Track : Tracks)
	{
		float PreviousFrameTime = Track.KeyFrames[0].Time;
		for (int32 i = 1; i < Track.KeyFrames.Num(); i += 1)
		{
			const FKeyFrame& Frame = Track.KeyFrames[i];

			checkf(PreviousFrameTime < Frame.Time, TEXT("At this point this can't go backward."));
			const float Delta = Frame.Time - PreviousFrameTime;
			checkf(Delta != 0.0, TEXT("This can't never happen at this point."));
			FrameInterval = FMath::Min(FrameInterval, Delta);
			SequenceDuration = FMath::Max(Frame.Time, SequenceDuration);
			PreviousFrameTime = Frame.Time;
		}
	}

	// `+ 1` to add the frame 0.
	const uint32 NumFrames = (FrameInterval == 0.0 ? 0 : FMath::CeilToInt(SequenceDuration / FrameInterval)) + 1;
	// This is needed to avoid skip the last frame in case of precision loss.
	SequenceDuration = (NumFrames - 1) * FrameInterval;

	// ~~ Fill the animation tracks ~~

	for (int32 TrackId = 0; TrackId < Tracks.Num(); TrackId += 1)
	{
		const int32 TrackIndex = TrackIndices[TrackId];
		const FTrack& Track = Tracks[TrackId];

		FRawAnimSequenceTrack& AnimTrack = Anim->GetRawAnimationTrack(TrackIndex);
		AnimTrack.PosKeys.SetNum(NumFrames);
		AnimTrack.RotKeys.SetNum(NumFrames);
		AnimTrack.ScaleKeys.SetNum(NumFrames);

		uint32 FrameId = 0;
		uint32 NextFrameId = FrameId + 1 >= static_cast<uint32>(Track.KeyFrames.Num()) ? FrameId : (FrameId + 1);
		for (uint32 FrameIndex = 0; FrameIndex < NumFrames; FrameIndex += 1)
		{
			const float Time = FrameInterval * static_cast<float>(FrameIndex);

			if (NextFrameId < static_cast<uint32>(Track.KeyFrames.Num()))
			{
				if (Time >= Track.KeyFrames[NextFrameId].Time)
				{
					// Time to advance to the next frame.
					FrameId = NextFrameId;
					NextFrameId = FrameId + 1 >= static_cast<uint32>(Track.KeyFrames.Num()) ? FrameId : (FrameId + 1);
				}
			}

			if (FrameId == NextFrameId)
			{
				// This is the last frame, nothing to interpolate.
				const FKeyFrame& Frame = Track.KeyFrames[FrameId];
				AnimTrack.PosKeys[FrameIndex] = Frame.Position;
				AnimTrack.RotKeys[FrameIndex] = Frame.Rotation;
				AnimTrack.ScaleKeys[FrameIndex] = Frame.Scale;
			}
			else
			{
				const FKeyFrame& Frame1 = Track.KeyFrames[FrameId];
				const FKeyFrame& Frame2 = Track.KeyFrames[NextFrameId];

				checkf(Frame1.Time < Frame2.Time, TEXT("This is is impossible because the `Prepare` clears all the duplicate key frames."));
				const float Alpha = FMath::Clamp((Time - Frame1.Time) / (Frame2.Time - Frame1.Time), 0.0f, 1.0f);

				AnimTrack.PosKeys[FrameIndex] = FMath::Lerp(Frame1.Position, Frame2.Position, Alpha);
				AnimTrack.RotKeys[FrameIndex] = FQuat4f::Slerp(Frame1.Rotation, Frame2.Rotation, Alpha);
				AnimTrack.ScaleKeys[FrameIndex] = FMath::Lerp(Frame1.Scale, Frame2.Scale, Alpha);
			}
		}
	}

	// ~~ Finalize the animation ~~
	Anim->SetRawNumberOfFrame(NumFrames);
	Anim->SetSequenceLength(SequenceDuration);
#if WITH_EDITOR
	Anim->PostProcessSequence();
#endif
	Anim->MarkPackageDirty();

	return Anim;
}
