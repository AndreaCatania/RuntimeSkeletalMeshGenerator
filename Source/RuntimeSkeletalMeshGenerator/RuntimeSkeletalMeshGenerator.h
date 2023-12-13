/******************************************************************************/
/* SkeletalMeshComponent Generator for UE5.03                                 */
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

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshRenderData.h"

/**
 * Struct for BoneInfluences.
 */
struct RUNTIMESKELETALMESHGENERATOR_API FRawBoneInfluence
{
	float Weight;
	int32 VertexIndex;
	int32 BoneIndex;
};

/**
 * This structure contains all the mesh surface info.
 */
struct RUNTIMESKELETALMESHGENERATOR_API FMeshSurface
{
	TArray<FVector3f> Vertices;
	TArray<uint32> Indices;
	TArray<FVector3f> Tangents;
	TArray<FVector3f> Normals;
	TArray<bool> FlipBinormalSigns;
	TArray<FColor> Colors;
	TArray<TArray<FVector2f>> Uvs;
	TArray<TArray<FRawBoneInfluence>> BoneInfluences;
};

class FRuntimeSkeletalMeshGeneratorModule : public IModuleInterface
{
public: // ------------------------------------- IModuleInterface implementation
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

class RUNTIMESKELETALMESHGENERATOR_API FRuntimeSkeletalMeshGenerator
{
	static void FillBufferWithMeshSurface(
		const TArray<FMeshSurface>& Surfaces,
		const int32 UVCount,
		bool& bUse16BitBoneIndex,
		int32& MaxBoneInfluences,
		TArray<uint32>& SurfaceVertexOffsets,
		TArray<uint32>& SurfaceIndexOffsets,
		TArray<FStaticMeshBuildVertex>& StaticVertices,
		TArray<uint32>& VertexSurfaceIndex,
		TArray<FVector>& Vertices,
		TArray<uint32>& Indices);
public: // ----------------------------------------------------------------- API
	/**
	 * Generate the `SkeletalMesh` for the given surfaces.
	 */
	static bool GenerateSkeletalMesh(
		USkeletalMesh* SkeletalMesh,
		const TArray<FMeshSurface>& Surfaces,
		const TArray<UMaterialInterface*>& SurfacesMaterial,
		const TMap<FName, FTransform>& BoneTransformsOverride = TMap<FName, FTransform>());

	/**
	 * Generate the `SkeletalMeshComponent` for the given surfaces, and add the
	 * component to the `Actor`.
	 */
	static USkeletalMeshComponent* GenerateSkeletalMeshComponent(
		AActor* Actor,
		USkeleton* BaseSkeleton,
		const TArray<FMeshSurface>& Surfaces,
		const TArray<UMaterialInterface*>& SurfacesMaterial,
		const TMap<FName, FTransform>& BoneTransformsOverride = TMap<FName, FTransform>());

	/**
	 * Update an existing the `SkeletalMeshComponent` for the given surfaces
	 * optionally supply the transform override
	 */
	static void UpdateSkeletalMeshComponent(
		USkeletalMeshComponent* SkeletalMeshComponent,
		USkeleton* BaseSkeleton,
		const TArray<FMeshSurface>& Surfaces,
		const TArray<UMaterialInterface*>& SurfacesMaterial,
		const TMap<FName, FTransform>& BoneTransformOverrides = TMap<FName, FTransform>());

	/**
	 * Decompose the `USkeletalMesh` in `Surfaces`.
	 */
	static bool DecomposeSkeletalMesh(
		/// The `SkeletalMesh` to decompose
		const USkeletalMesh* SkeletalMesh,
		/// Out Surfaces.
		TArray<FMeshSurface>& OutSurfaces,
		/// The vertex offsets for each surface, relative to the passed `SkeletalMesh`
		TArray<int32>& OutSurfacesVertexOffsets,
		/// The index offsets for each surface, relative to the passed `SkeletalMesh`
		TArray<int32>& OutSurfacesIndexOffsets,
		/// Out Materials used.
		TArray<UMaterialInterface*>& OutSurfacesMaterial);
};
