/******************************************************************************/
/* Animation Generator for UE4.27                                             */
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
#include "AnimSequenceRuntime.h"

int32 AnimSequenceRuntime::AddNewRawTrackRuntime(FName TrackName, FRawAnimSequenceTrack* TrackData, TArray<FName>& RuntimeAnimationTrackNames)
{
#if WITH_EDITORONLY_DATA
	return AddNewRawTrack(TrackName, TrackData);
#else
	// During compression, we store the track indices on 16 bits
	const int32 MAX_NUM_TRACKS = 65535;
	if (RawAnimationData.Num() >= MAX_NUM_TRACKS)
	{
		return INDEX_NONE;
	}

	const int32 SkeletonIndex = GetSkeleton() ? GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(TrackName) : INDEX_NONE;

	if (SkeletonIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	int32 TrackIndex = RuntimeAnimationTrackNames.IndexOfByKey(TrackName);
	if (TrackIndex != INDEX_NONE)
	{
		if (TrackData)
		{
			RawAnimationData[TrackIndex] = *TrackData;
		}
		return TrackIndex;
	}

	check(RuntimeAnimationTrackNames.Num() == RawAnimationData.Num());
	TrackIndex = RuntimeAnimationTrackNames.Add(TrackName);
	TrackToSkeletonMapTable.Add(FTrackToSkeletonMap(SkeletonIndex));
	if (TrackData)
	{
		RawAnimationData.Add(*TrackData);
	}
	else
	{
		RawAnimationData.Add(FRawAnimSequenceTrack());
	}
	return TrackIndex;
#endif
}
