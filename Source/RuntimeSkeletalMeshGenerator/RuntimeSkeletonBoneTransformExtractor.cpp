#include "RuntimeSkeletonBoneTransformExtractor.h"

#include "Animation/Skeleton.h"

FRuntimeSkeletonBoneTransformExtractor::FRuntimeSkeletonBoneTransformExtractor(
	const FReferenceSkeleton& InRefSkeleton,
	const TMap<FName, FTransform>& PoseOffsets)
	: RefSkeleton(InRefSkeleton)
{
	const TArray<FTransform>& BoneTransforms = InRefSkeleton.GetRawRefBonePose();

	GlobalBoneTransforms.Init(FMatrix::Identity, BoneTransforms.Num());

	TArray<bool> HasGlobalTransform;
	HasGlobalTransform.Init(false, BoneTransforms.Num());

	for (int32 i = 0; i < BoneTransforms.Num(); i += 1)
	{
		ComputeGlobalTransform(
			i,
			BoneTransforms,
			PoseOffsets,
			GlobalBoneTransforms,
			HasGlobalTransform,
			InRefSkeleton);
	}
}

/// Returns the Bone Transform, fetching it by BoneIndex.
const FMatrix& FRuntimeSkeletonBoneTransformExtractor::GetGlobalTransform(const int32 BoneIndex) const
{
	return GlobalBoneTransforms[BoneIndex];
}

/// Slower version of `GetGlobalTransform()` that fetches bone by Name.
FMatrix FRuntimeSkeletonBoneTransformExtractor::GetGlobalTransform(const FName BoneName) const
{
	const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
	if (BoneIndex == INDEX_NONE)
	{
		return FMatrix::Identity;
	}
	else
	{
		return GetGlobalTransform(RefSkeleton.FindBoneIndex(BoneName));
	}
}

uint32 FRuntimeSkeletonBoneTransformExtractor::GetBoneNum() const
{
	return GlobalBoneTransforms.Num();
}

FMatrix FRuntimeSkeletonBoneTransformExtractor::ComputeGlobalTransform(
	const int32 BoneIndex,
	const TArray<FTransform>& LocalTransforms,
	const TMap<FName, FTransform>& PoseOffsets,
	TArray<FMatrix>& GlobalTransforms,
	TArray<bool>& HasGlobalTransform,
	const FReferenceSkeleton& InRefSkeleton)
{
	if (BoneIndex == INDEX_NONE)
	{
		return FMatrix::Identity;
	}

	if (HasGlobalTransform[BoneIndex])
	{
		return GlobalTransforms[BoneIndex];
	}

	const FName BoneName = InRefSkeleton.GetBoneName(BoneIndex);
	const int32 ParentIndex = InRefSkeleton.GetParentIndex(BoneIndex);

	const FMatrix ParentBoneTransform = ComputeGlobalTransform(
		ParentIndex,
		LocalTransforms,
		PoseOffsets,
		GlobalTransforms,
		HasGlobalTransform,
		InRefSkeleton);

	GlobalTransforms[BoneIndex] = LocalTransforms[BoneIndex].ToMatrixWithScale() * ParentBoneTransform;

	if (const FTransform* PoseOffset = PoseOffsets.Find(BoneName))
	{
		GlobalTransforms[BoneIndex] = PoseOffset->ToMatrixWithScale() * GlobalTransforms[BoneIndex];
	}

	HasGlobalTransform[BoneIndex] = true;

	return GlobalTransforms[BoneIndex];
}
