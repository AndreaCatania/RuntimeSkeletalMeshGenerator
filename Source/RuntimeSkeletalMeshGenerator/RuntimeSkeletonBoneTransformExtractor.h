#pragma once

#include "Math/Transform.h"

class USkeleton;

/// Utility to fetch the bone world transform.
/// Eventually you can pose the skeleton before extracting the final Bone transform.
class RUNTIMESKELETALMESHGENERATOR_API FRuntimeSkeletonBoneTransformExtractor
{
	const USkeleton* Skeleton = nullptr;
	TArray<FMatrix> GlobalBoneTransforms;

public:
	FRuntimeSkeletonBoneTransformExtractor(const USkeleton* InSkeleton, const TMap<FName, FTransform>& PoseOffsets);

	/// Returns the Bone Transform, fetching it by BoneIndex.
	const FMatrix& GetGlobalTransform(const int32 BoneIndex) const;

	/// Slower version of `GetGlobalTransform()` that fetches bone by Name.
	FMatrix GetGlobalTransform(const FName BoneName) const;

	uint32 GetBoneNum() const;

	const USkeleton* GetSkeleton() const;

private:
	static FMatrix ComputeGlobalTransform(
		const int32 BoneIndex,
		const TArray<FTransform>& LocalTransforms,
		const TMap<FName, FTransform>& PoseOffsets,
		TArray<FMatrix>& GlobalTransforms,
		TArray<bool>& HasGlobalTransform,
		const USkeleton* Skeleton);
};
