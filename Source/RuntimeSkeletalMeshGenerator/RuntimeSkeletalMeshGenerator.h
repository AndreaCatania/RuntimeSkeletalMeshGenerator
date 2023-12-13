/******************************************************************************/
/* SkeletalMeshComponent Generator for UE4.27                                 */
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
#include "Rendering/SkeletalMeshLODImporterData.h"
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
	TArray<FVector> Vertices;
	TArray<FVector> Tangents;
	TArray<bool> FlipBinormalSigns;
	TArray<FVector> Normals;
	TArray<FColor> Colors;
	TArray<TArray<FVector2D>> Uvs;
	TArray<TArray<FRawBoneInfluence>> BoneInfluences;
	TArray<uint32> Indices;

	int MaterialIndex;
};

class FRuntimeSkeletalMeshGeneratorModule : public IModuleInterface
{
public: // ------------------------------------- IModuleInterface implementation
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

class RUNTIMESKELETALMESHGENERATOR_API FRuntimeSkeletalMeshGenerator
{
public: // ----------------------------------------------------------------- API
	/**
	 * Generate the `SkeletalMesh` for the given surfaces.
	 */
	static void GenerateSkeletalMesh(
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
