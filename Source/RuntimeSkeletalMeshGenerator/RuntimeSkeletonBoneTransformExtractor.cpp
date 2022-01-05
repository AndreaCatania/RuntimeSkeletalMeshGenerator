#include "RuntimeSkeletonBoneTransformExtractor.h"

#include "Animation/Skeleton.h"

FRuntimeSkeletonBoneTransformExtractor::FRuntimeSkeletonBoneTransformExtractor(const USkeleton* InSkeleton, const TMap<FName, FTransform>& PoseOffsets)
{
	Skeleton = InSkeleton;
	const TArray<FTransform>& BoneTransforms = Skeleton->GetReferenceSkeleton().GetRawRefBonePose();

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
			Skeleton);
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
	const int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);
	if (BoneIndex == INDEX_NONE)
	{
		return FMatrix::Identity;
	}
	else
	{
		return GetGlobalTransform(Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName));
	}
}

uint32 FRuntimeSkeletonBoneTransformExtractor::GetBoneNum() const
{
	return GlobalBoneTransforms.Num();
}

const USkeleton* FRuntimeSkeletonBoneTransformExtractor::GetSkeleton() const
{
	return Skeleton;
}

FMatrix FRuntimeSkeletonBoneTransformExtractor::ComputeGlobalTransform(
	const int32 BoneIndex,
	const TArray<FTransform>& LocalTransforms,
	const TMap<FName, FTransform>& PoseOffsets,
	TArray<FMatrix>& GlobalTransforms,
	TArray<bool>& HasGlobalTransform,
	const USkeleton* Skeleton)
{
	if (BoneIndex == INDEX_NONE)
	{
		return FMatrix::Identity;
	}

	if (HasGlobalTransform[BoneIndex])
	{
		return GlobalTransforms[BoneIndex];
	}

	const FName BoneName = Skeleton->GetReferenceSkeleton().GetBoneName(BoneIndex);
	const int32 ParentIndex = Skeleton->GetReferenceSkeleton().GetParentIndex(BoneIndex);

	const FMatrix ParentBoneTransform = ComputeGlobalTransform(
		ParentIndex,
		LocalTransforms,
		PoseOffsets,
		GlobalTransforms,
		HasGlobalTransform,
		Skeleton);

	GlobalTransforms[BoneIndex] = LocalTransforms[BoneIndex].ToMatrixWithScale() * ParentBoneTransform;

	if (const FTransform* PoseOffset = PoseOffsets.Find(BoneName))
	{
		GlobalTransforms[BoneIndex] = PoseOffset->ToMatrixWithScale() * GlobalTransforms[BoneIndex];
	}

	HasGlobalTransform[BoneIndex] = true;

	return GlobalTransforms[BoneIndex];
}
