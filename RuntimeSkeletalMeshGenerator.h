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

#include "MeshUtilities.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshModel.h"

/**
 * This structure contains all the mesh surface info.
 */
struct FMeshSurface
{
	TArray<FVector> Vertices;
	TArray<FVector> Tangents;
	TArray<bool> FlipBinormalSigns;
	TArray<FVector> Normals;
	TArray<uint32> Indices;
	TArray<FColor> Colors;
	TArray<TArray<FVector2D>> Uvs;
	TArray<TArray<SkeletalMeshImportData::FRawBoneInfluence>> BoneInfluences;
};

/**
 * Generate the `SkeletalMeshComponent` for the given surfaces, and add the
 * component to the `Actor`.
 */
inline USkeletalMeshComponent* GenerateSkeletalMeshComponent(
	AActor* Actor,
	USkeleton* BaseSkeleton,
	const TArray<FMeshSurface>& Surfaces,
	const TArray<UMaterialInterface*>& SurfacesMaterial)
{
	// Waits the rendering thread has done.
	FlushRenderingCommands();

	TArray<uint32> SurfaceVertexOffsets;
	TArray<uint32> SurfaceIndexOffsets;
	SurfaceVertexOffsets.SetNum(Surfaces.Num());
	SurfaceIndexOffsets.SetNum(Surfaces.Num());

	bool bUse16BitBoneIndex = false;
	int32 MaxBoneInfluences = 0;
	const int32 UVCount = Surfaces.Num() > 0 ? (Surfaces[0].Uvs.Num() > 0 ? Surfaces[0].Uvs[0].Num() : 0) : 0;

	// Collect all the vertices and index for each surface.
	TArray<FStaticMeshBuildVertex> StaticVertices;
	TArray<FVector> Vertices;
	TArray<uint32> Indices;
	{
		int MaxBoneIndex = 0;
		// First count all the vertices.
		uint32 VerticesCount = 0;
		uint32 IndicesCount = 0;
		for (const FMeshSurface& Surface : Surfaces)
		{
			VerticesCount += Surface.Vertices.Num();
			IndicesCount += Surface.Indices.Num();

			for (const auto& Influences : Surface.BoneInfluences)
			{
				MaxBoneInfluences = FMath::Max(Influences.Num(), MaxBoneInfluences);
				for (const auto& Influence : Influences)
				{
					MaxBoneIndex = FMath::Max(Influence.BoneIndex, MaxBoneIndex);
				}
			}

#if WITH_EDITOR
			// Make sure all the surfaces and all the vertices have the same amount of
			// UVs.
			for (const TArray<FVector2D> Uvs : Surface.Uvs)
			{
				check(UVCount == Uvs.Num());
			}
#endif
		}

		bUse16BitBoneIndex = MaxBoneIndex <= MAX_uint16;

		StaticVertices.SetNum(VerticesCount);
		Vertices.SetNum(VerticesCount);
		Indices.SetNum(IndicesCount);

		uint32 VerticesOffset = 0;
		uint32 IndicesOffset = 0;
		for (int32 I = 0; I < Surfaces.Num(); I++)
		{
			const FMeshSurface& Surface = Surfaces[I];

			for (int VertexIndex = 0; VertexIndex < Surface.Vertices.Num(); VertexIndex += 1)
			{
				if (Surface.Colors.Num() > 0)
				{
					StaticVertices[VerticesOffset + VertexIndex].Color = Surface.Colors[VertexIndex];
				}
				StaticVertices[VerticesOffset + VertexIndex].Position = Surface.Vertices[VertexIndex];
				StaticVertices[VerticesOffset + VertexIndex].TangentX = Surface.Tangents[VertexIndex];
				StaticVertices[VerticesOffset + VertexIndex].TangentY = FVector::CrossProduct(Surface.Normals[VertexIndex], Surface.Tangents[VertexIndex]) * (Surface.FlipBinormalSigns[VertexIndex] ? -1.0 : 1.0);
				StaticVertices[VerticesOffset + VertexIndex].TangentZ = Surface.Normals[VertexIndex];
				FMemory::Memcpy(
					StaticVertices[VerticesOffset + VertexIndex].UVs,
					Surface.Uvs[VertexIndex].GetData(),
					sizeof(FVector2D) * UVCount);
			}

			FMemory::Memcpy(
				Vertices.GetData() + VerticesOffset,
				Surface.Vertices.GetData(),
				sizeof(FVector) * Surface.Vertices.Num());

			// Convert the Indices to Global.
			for (int32 x = 0; x < Surface.Indices.Num(); x++)
			{
				Indices[IndicesOffset + x] = Surface.Indices[x] + VerticesOffset;
			}

			SurfaceVertexOffsets[I] = VerticesOffset;
			VerticesOffset += Surface.Vertices.Num();

			SurfaceIndexOffsets[I] = IndicesOffset;
			IndicesOffset += Surface.Indices.Num();
		}
	}

	// Unreal doesn't support more than `MAX_TOTAL_INFLUENCES` BoneInfluences.
	check(MaxBoneInfluences <= MAX_TOTAL_INFLUENCES);

	// Unreal doesn't support more than `MAX_STATIC_TEXCOORDS`.
	check(UVCount <= MAX_STATIC_TEXCOORDS);

	// Note: we use nullptr outer to make this transient.
	USkeletalMesh* SkeletalMesh = NewObject<USkeletalMesh>();
	SkeletalMesh->RefSkeleton = BaseSkeleton->GetReferenceSkeleton();
	SkeletalMesh->Skeleton = BaseSkeleton;

	// Populate Arrays step.
	SkeletalMesh->AllocateResourceForRendering();
	FSkeletalMeshRenderData* MeshRenderData = SkeletalMesh->GetResourceForRendering();

	FSkeletalMeshLODRenderData* LODMeshRenderData = new FSkeletalMeshLODRenderData;
	MeshRenderData->LODRenderData.Add(LODMeshRenderData);

	SkeletalMesh->ResetLODInfo();
	FSkeletalMeshLODInfo& MeshLodInfo = SkeletalMesh->AddLODInfo();
	// These are correct unreal defaults.
	MeshLodInfo.LODHysteresis = 0.02;
	MeshLodInfo.ScreenSize = 1.0;

	// Set Bounding boxes
	const FBox BoundingBox(Vertices.GetData(), Vertices.Num());
	SkeletalMesh->SetImportedBounds(FBoxSphereBounds(BoundingBox));

	// NOTE: not the same as the original code - fishy remove if not required.
	// Seems required to edit in editor.
	FSkeletalMeshLODModel* SkeletalMeshLODModel = new FSkeletalMeshLODModel();
	SkeletalMesh->GetImportedModel()->LODModels.Add(SkeletalMeshLODModel);

	SkeletalMeshLODModel->NumVertices = Vertices.Num();
	SkeletalMeshLODModel->NumTexCoords = UVCount;

	SkeletalMeshLODModel->Sections.SetNum(Surfaces.Num());
	LODMeshRenderData->RenderSections.SetNum(Surfaces.Num());

	for (int32 I = 0; I < Surfaces.Num(); I++)
	{
		const FMeshSurface& Surface = Surfaces[I];
		FSkelMeshSection& MeshSection = SkeletalMeshLODModel->Sections[I];
		FSkelMeshRenderSection& RenderSection = LODMeshRenderData->RenderSections[I];

		MeshSection.bDisabled = false;
		MeshSection.BaseVertexIndex = SurfaceVertexOffsets[I];
		MeshSection.NumVertices = Surface.Vertices.Num();
		MeshSection.BaseIndex = SurfaceIndexOffsets[I];
		MeshSection.NumTriangles = Surface.Indices.Num() / 3;
		MeshSection.MaterialIndex = I;
		MeshSection.bCastShadow = true;
		MeshSection.bRecomputeTangent = true;
		MeshSection.bUse16BitBoneIndex = bUse16BitBoneIndex;
		MeshSection.OriginalDataSectionIndex = I; // Section IDX for below lookup in user sections data
		MeshSection.SoftVertices.Empty();
		MeshSection.MaxBoneInfluences = MaxBoneInfluences;

		RenderSection.bDisabled = MeshSection.bDisabled;
		RenderSection.bRecomputeTangent = MeshSection.bRecomputeTangent;
		RenderSection.bCastShadow = MeshSection.bCastShadow;
		RenderSection.BaseVertexIndex = MeshSection.BaseVertexIndex;
		RenderSection.BaseIndex = MeshSection.BaseIndex;
		RenderSection.MaterialIndex = MeshSection.MaterialIndex;
		RenderSection.NumVertices = MeshSection.NumVertices;
		RenderSection.NumTriangles = MeshSection.NumTriangles;
		RenderSection.MaxBoneInfluences = MeshSection.MaxBoneInfluences;

		{
			FSkelMeshSourceSectionUserData& UserSectionData = SkeletalMeshLODModel->UserSectionsData.FindOrAdd(I);
			UserSectionData.bDisabled = MeshSection.bDisabled;
			UserSectionData.bCastShadow = MeshSection.bCastShadow;
			UserSectionData.bRecomputeTangent = MeshSection.bRecomputeTangent;
			UserSectionData.RecomputeTangentsVertexMaskChannel = MeshSection.RecomputeTangentsVertexMaskChannel;
			UserSectionData.GenerateUpToLodIndex = MeshSection.GenerateUpToLodIndex;
		}

#if WITH_EDITOR
		{
			// In Editor we want to make sure the data is in sync between
			// `UserSectionsData` and RenderSections.
			const FSkelMeshSourceSectionUserData& UserSectionData = SkeletalMeshLODModel->UserSectionsData.FindOrAdd(I);
			const bool IsRenderDataInSync =
				UserSectionData.bDisabled == RenderSection.bDisabled &&
				UserSectionData.bCastShadow == RenderSection.bCastShadow &&
				UserSectionData.bRecomputeTangent == RenderSection.bRecomputeTangent &&
				UserSectionData.RecomputeTangentsVertexMaskChannel == RenderSection.RecomputeTangentsVertexMaskChannel &&
				UserSectionData.CorrespondClothAssetIndex == RenderSection.CorrespondClothAssetIndex &&
				UserSectionData.ClothingData.AssetGuid == RenderSection.ClothingData.AssetGuid &&
				UserSectionData.ClothingData.AssetLodIndex == RenderSection.ClothingData.AssetLodIndex;

			check(IsRenderDataInSync); // this must always be true or unreal will choke.
		}
#endif

		// This is used when you have no overlapping Vertices.
		{
			RenderSection.DuplicatedVerticesBuffer.DupVertData.ResizeBuffer(1);
			uint8* VertData = RenderSection.DuplicatedVerticesBuffer.DupVertData.GetDataPointer();
			FMemory::Memzero(VertData, sizeof(uint32) * RenderSection.DuplicatedVerticesBuffer.DupVertData.Num());

			RenderSection.DuplicatedVerticesBuffer.DupVertIndexData.ResizeBuffer(RenderSection.NumVertices);
			uint8* IndexData = RenderSection.DuplicatedVerticesBuffer.DupVertIndexData.GetDataPointer();
			FMemory::Memzero(IndexData, RenderSection.NumVertices * sizeof(FIndexLengthPair));
		}
	}

	// Set the Indices.
	{
		SkeletalMeshLODModel->IndexBuffer = Indices;

		LODMeshRenderData->MultiSizeIndexContainer.RebuildIndexBuffer(
			// Dynamically chose the index buffer size.
			SkeletalMeshLODModel->IndexBuffer.Num() < MAX_uint16 ? sizeof(uint16) : sizeof(uint32),
			SkeletalMeshLODModel->IndexBuffer);

		// Test our data is good and the renderer can access it.
		TArray<uint32> ActualIndexBuffer;
		LODMeshRenderData->MultiSizeIndexContainer.GetIndexBuffer(ActualIndexBuffer);
		check(ActualIndexBuffer.Num() == Indices.Num());
	}

	// Set the Vertex now.
	constexpr bool NeedCPUAccess = false;

	LODMeshRenderData->StaticVertexBuffers.PositionVertexBuffer.Init(
		StaticVertices,
		NeedCPUAccess);
	LODMeshRenderData->StaticVertexBuffers.StaticMeshVertexBuffer.Init(
		StaticVertices,
		UVCount,
		NeedCPUAccess);

	LODMeshRenderData->SkinWeightVertexBuffer.SetMaxBoneInfluences(MaxBoneInfluences);
	LODMeshRenderData->SkinWeightVertexBuffer.SetUse16BitBoneIndex(bUse16BitBoneIndex);

	TArray<FSkinWeightInfo> Weights;
	Weights.SetNum(Vertices.Num());

	for (int32 I = 0; I < Surfaces.Num(); I++)
	{
		const FMeshSurface& Surface = Surfaces[I];

		for (int32 VertexIndex = 0; VertexIndex < Surface.BoneInfluences.Num(); VertexIndex++)
		{
			const TArray<SkeletalMeshImportData::FRawBoneInfluence>& VertInfluences = Surface.BoneInfluences[VertexIndex];
			FSkinWeightInfo& Weight = Weights[SurfaceVertexOffsets[I] + VertexIndex];

			for (int InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; InfluenceIndex++)
			{
				if (InfluenceIndex >= VertInfluences.Num())
				{
					// The available influences for this surface, is smaller than the whole
					// Mesh. This happens when the user submits surfaces with different
					// bone weights. Pad it automatically, to simplify the user life.
					Weight.InfluenceWeights[InfluenceIndex] = 0;
					Weight.InfluenceBones[InfluenceIndex] = 0;
				}
				else
				{
					const SkeletalMeshImportData::FRawBoneInfluence& VertInfluence = VertInfluences[InfluenceIndex];

					// Make sure these are the same.
					check(VertexIndex == VertInfluence.VertexIndex);

					if (!BaseSkeleton->GetReferenceSkeleton().IsValidIndex(VertInfluence.BoneIndex))
					{
						// This bone appear to be invalid, continue.
						UE_LOG(LogTemp, Warning, TEXT("The bone %i isn't found in this skeleton"), VertInfluence.BoneIndex);
						continue;
					}

					// Convert 0.0 - 1.0 range to 0 - 255
					const uint8 EncodedWeight =
						FMath::Clamp(VertInfluence.Weight, 0.f, 1.f) * 255.f;
					Weight.InfluenceWeights[InfluenceIndex] = EncodedWeight;
					Weight.InfluenceBones[InfluenceIndex] = EncodedWeight == 0 ? 0 : VertInfluence.BoneIndex;
				}
			}
		}
	}

	// Enables all the Bones of this skeleton, to avoid break the mesh.
	SkeletalMeshLODModel->ActiveBoneIndices.Empty();
	SkeletalMeshLODModel->RequiredBones.Empty();
	LODMeshRenderData->RequiredBones.Empty();
	LODMeshRenderData->ActiveBoneIndices.Empty();

	for (int32 I = 0; I < Surfaces.Num(); I++)
	{
		FSkelMeshSection& MeshSection = SkeletalMeshLODModel->Sections[I];
		FSkelMeshRenderSection& RenderSection = LODMeshRenderData->RenderSections[I];
		MeshSection.BoneMap.Empty();
		RenderSection.BoneMap.Empty();
	}

	for (int32 BoneIndex = 0; BoneIndex < BaseSkeleton->GetBoneTree().Num(); BoneIndex++)
	{
		SkeletalMeshLODModel->ActiveBoneIndices.Add(BoneIndex);
		SkeletalMeshLODModel->RequiredBones.Add(BoneIndex);
		LODMeshRenderData->RequiredBones.Add(BoneIndex);
		LODMeshRenderData->ActiveBoneIndices.Add(BoneIndex);

		for (int32 I = 0; I < Surfaces.Num(); I++)
		{
			FSkelMeshSection& MeshSection = SkeletalMeshLODModel->Sections[I];
			FSkelMeshRenderSection& RenderSection = LODMeshRenderData->RenderSections[I];
			MeshSection.BoneMap.Add(BoneIndex);
			RenderSection.BoneMap.Add(BoneIndex);
		}
	}

	// Set the skin weights.
	LODMeshRenderData->SkinWeightVertexBuffer = Weights;

	// Set the default Material.
	for (auto Material : SurfacesMaterial)
	{
		SkeletalMesh->Materials.Add(Material);
	}

	// Rebuild inverse ref pose matrices.
	SkeletalMesh->SkelMirrorTable.Empty();
	SkeletalMesh->SkelMirrorAxis = EAxis::Type::None;
	SkeletalMesh->SkelMirrorFlipAxis = EAxis::Type::None;
	SkeletalMesh->RefBasesInvMatrix.Empty();
	SkeletalMesh->CalculateInvRefMatrices();
	MeshRenderData->bReadyForStreaming = false;

	// Suspected Finalization step
	if (!GIsEditor)
	{
		SkeletalMesh->NeverStream = false;
	}

	// Calls InitResources.
	SkeletalMesh->PostLoad();

	// Signals to editor we are done with our changes
	// This is to prevent the editor variable changes overwriting the import mesh,
	// if you don't set this random crashes occur.
	SkeletalMesh->StackPostEditChange();

	USkeletalMeshComponent* SkeletalMeshComponent =
		NewObject<USkeletalMeshComponent>(
			Actor,
			USkeletalMeshComponent::StaticClass());

	// We register the skeleton resource (which is not meant to be transient to
	// the engine).
	SkeletalMeshComponent->SetSkeletalMesh(SkeletalMesh);

	// We register the components and attach them to the editor
	// We are adding at runtime so its necessary to manually register to the engine.
	SkeletalMeshComponent->AttachToComponent(
		Actor->GetRootComponent(),
		FAttachmentTransformRules::KeepRelativeTransform);

	// Manually called because the component won't appear in the list without this
	// call.
	Actor->AddInstanceComponent(SkeletalMeshComponent);

	// Registration is required at runtime since we don't add during construction
	SkeletalMeshComponent->RegisterComponent();

	check(SkeletalMeshComponent->RequiredBones.Num() != 0);
	check(SkeletalMeshComponent->FillComponentSpaceTransformsRequiredBones.Num() != 0);

	return SkeletalMeshComponent;
}
