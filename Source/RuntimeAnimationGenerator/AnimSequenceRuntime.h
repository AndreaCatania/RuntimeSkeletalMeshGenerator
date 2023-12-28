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
#pragma once

#include "Animation/AnimSequence.h"

/// This class add some functionalities to build the Animation at runtime.
/// Note: This class is not an `UClass` on purpose:
/// The `UAnimSequence` can't be overriden from an external module, so the only
/// way to add some functionality to it, is to cast the `UAnimSequence` to this
/// class, to expose and add functionalities to `UAnimSequence`.
/// Please, make sure to never add any member to this class or dragons will summon.
class UAnimSequenceRuntime final : public UAnimSequence
{
public:
	/// This is the runtime version of `AddNewRawTrack`.
	/// If called on editor, it creates enough data to store this Animation.
	/// If called at runtime it still creates the animation.
	int32 AddNewRawTrackRuntime(FName TrackName, FRawAnimSequenceTrack* TrackData, TArray<FName>& RuntimeAnimationTrackNames);
};
