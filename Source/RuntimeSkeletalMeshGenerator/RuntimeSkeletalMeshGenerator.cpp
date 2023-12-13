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
#include "RuntimeSkeletalMeshGenerator.h"

#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include <algorithm> 

void FRuntimeSkeletalMeshGeneratorModule::StartupModule()
{
}

void FRuntimeSkeletalMeshGeneratorModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FRuntimeSkeletalMeshGeneratorModule, RuntimeSkeletalMeshGenerator)

void FRuntimeSkeletalMeshGenerator::GenerateSkeletalMesh(
	USkeletalMesh* SkeletalMesh,
	const TArray<FMeshSurface>& Surfaces,
	const TArray<UMaterialInterface*>& SurfacesMaterial,
	const TMap<FName, FTransform>& BoneTransformsOverride)
{
	// Waits the rendering thread has done.
	FlushRenderingCommands();

	constexpr int32 LODIndex = 0;

#if WITH_EDITORONLY_DATA
	FSkeletalMeshImportData ImportedModelData;
#endif

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
	TArray<uint32> VertexSurfaceIndex;
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
		VertexSurfaceIndex.SetNum(VerticesCount);
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
				StaticVertices[VerticesOffset + VertexIndex].Position = FVector3f(Surface.Vertices[VertexIndex]);
				StaticVertices[VerticesOffset + VertexIndex].TangentX = FVector3f(Surface.Tangents[VertexIndex]);
				StaticVertices[VerticesOffset + VertexIndex].TangentY = FVector3f(FVector::CrossProduct(Surface.Normals[VertexIndex], Surface.Tangents[VertexIndex]) * (Surface.FlipBinormalSigns[VertexIndex] ? -1.0 : 1.0));
				StaticVertices[VerticesOffset + VertexIndex].TangentZ = FVector3f(Surface.Normals[VertexIndex]);
				FMemory::Memcpy(
					StaticVertices[VerticesOffset + VertexIndex].UVs,
					Surface.Uvs[VertexIndex].GetData(),
					sizeof(FVector2D) * UVCount);

				VertexSurfaceIndex[VerticesOffset + VertexIndex] = I;
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

#if WITH_EDITORONLY_DATA
	// Initialize the `ImportModel` this is used by the editor during reload time.
	ImportedModelData.Points.Empty();
	ImportedModelData.Points.Append(Vertices);

	// Existing points map 1:1
	ImportedModelData.PointToRawMap.AddUninitialized(ImportedModelData.Points.Num());
	for (int32 i = 0; i < ImportedModelData.Points.Num(); i++)
	{
		ImportedModelData.PointToRawMap[i] = i;
	}
	check(ImportedModelData.PointToRawMap.Num() == Vertices.Num());


	for (auto& s : Surfaces)
	{
		//�������ʵĸ�ֵ��һ����ȫ��ȷ����֪������SurfacesMaterialΪ�յ��������Ƿ�������
		if (s.MaterialIndex >= 0 && s.MaterialIndex < SurfacesMaterial.Num())
		{
			if (s.MaterialIndex >= ImportedModelData.Materials.Num())
			{
				SkeletalMeshImportData::FMaterial& Mat = ImportedModelData.Materials.AddDefaulted_GetRef();
				Mat.Material = SurfacesMaterial[s.MaterialIndex];
				Mat.MaterialImportName = SurfacesMaterial[s.MaterialIndex]->GetFullName();
			}
		}
	}


	ImportedModelData.Faces.SetNum(Indices.Num() / 3);
	for (int32 FaceIndex = 0; FaceIndex < ImportedModelData.Faces.Num(); FaceIndex += 1)
	{
		SkeletalMeshImportData::FTriangle& Triangle = ImportedModelData.Faces[FaceIndex];

		const int32 VertexIndex0 = Indices[FaceIndex * 3 + 0];
		const int32 VertexIndex1 = Indices[FaceIndex * 3 + 1];
		const int32 VertexIndex2 = Indices[FaceIndex * 3 + 2];
		Triangle.WedgeIndex[0] = FaceIndex * 3 + 0;
		Triangle.WedgeIndex[1] = FaceIndex * 3 + 1;
		Triangle.WedgeIndex[2] = FaceIndex * 3 + 2;

		Triangle.TangentX[0] = StaticVertices[VertexIndex0].TangentX;
		Triangle.TangentY[0] = StaticVertices[VertexIndex0].TangentY;
		Triangle.TangentZ[0] = StaticVertices[VertexIndex0].TangentZ;

		Triangle.TangentX[1] = StaticVertices[VertexIndex1].TangentX;
		Triangle.TangentY[1] = StaticVertices[VertexIndex1].TangentY;
		Triangle.TangentZ[1] = StaticVertices[VertexIndex1].TangentZ;

		Triangle.TangentX[2] = StaticVertices[VertexIndex2].TangentX;
		Triangle.TangentY[2] = StaticVertices[VertexIndex2].TangentY;
		Triangle.TangentZ[2] = StaticVertices[VertexIndex2].TangentZ;

		Triangle.MatIndex = VertexSurfaceIndex[VertexIndex0];
		Triangle.AuxMatIndex = 0;
		Triangle.SmoothingGroups = 1; // TODO Calculate the smoothing group correctly, otherwise everything will be smooth
	}

	ImportedModelData.Wedges.SetNum(ImportedModelData.Faces.Num() * 3);
	for (int32 FaceIndex = 0; FaceIndex < ImportedModelData.Faces.Num(); FaceIndex += 1)
	{
		for (int32 i = 0; i < 3; i += 1)
		{
			const int32 WedgeIndex = FaceIndex * 3 + i;
			const int32 VertexIndex = Indices[WedgeIndex];

			ImportedModelData.Wedges[WedgeIndex].VertexIndex = VertexIndex;
			for (int32 UVIndex = 0; UVIndex < FMath::Min(MAX_TEXCOORDS, MAX_STATIC_TEXCOORDS); UVIndex += 1)
			{
				ImportedModelData.Wedges[WedgeIndex].UVs[UVIndex] = StaticVertices[VertexIndex].UVs[UVIndex];
			}
			ImportedModelData.Wedges[WedgeIndex].MatIndex = VertexSurfaceIndex[VertexIndex];
			ImportedModelData.Wedges[WedgeIndex].Color = StaticVertices[VertexIndex].Color;
			ImportedModelData.Wedges[WedgeIndex].Reserved = 0;
		}
	}

	{
		const int32 BoneNum = SkeletalMesh->Skeleton->GetReferenceSkeleton().GetRawBoneNum();
		SkeletalMeshImportData::FBone DefaultBone;
		DefaultBone.Name = FString(TEXT(""));
		DefaultBone.Flags = 0;
		DefaultBone.NumChildren = 0;
		DefaultBone.ParentIndex = INDEX_NONE;
		DefaultBone.BonePos.Transform.SetIdentity();
		DefaultBone.BonePos.Length = 0.0;
		DefaultBone.BonePos.XSize = 1.0;
		DefaultBone.BonePos.YSize = 1.0;
		DefaultBone.BonePos.ZSize = 1.0;
		ImportedModelData.RefBonesBinary.Init(DefaultBone, BoneNum);
		for (int32 i = 0; i < BoneNum; i += 1)
		{
			ImportedModelData.RefBonesBinary[i].Name = SkeletalMesh->GetSkeleton()->GetReferenceSkeleton().GetBoneName(i).ToString();
			ImportedModelData.RefBonesBinary[i].ParentIndex = SkeletalMesh->GetSkeleton()->GetReferenceSkeleton().GetParentIndex(i);
			if (ImportedModelData.RefBonesBinary[i].ParentIndex != INDEX_NONE)
			{
				// Increase parent children count by 1
				ImportedModelData.RefBonesBinary[ImportedModelData.RefBonesBinary[i].ParentIndex].NumChildren += 1;
			}
		}

		// At this point it's sure all the bones are initialized, finish the process
		// by setting the local transform.
		for (int32 i = 0; i < BoneNum; i += 1)
		{
			// Relative to its parent.
			const FTransform* TransformOverride = BoneTransformsOverride.Find(SkeletalMesh->GetSkeleton()->GetReferenceSkeleton().GetBoneName(i));
			if (TransformOverride != nullptr)
			{
				
				ImportedModelData.RefBonesBinary[i].BonePos.Transform = FTransform3f(*TransformOverride);
			}
			else
			{
				ImportedModelData.RefBonesBinary[i].BonePos.Transform = FTransform3f(SkeletalMesh->GetSkeleton()->GetReferenceSkeleton().GetRawRefBonePose()[i]);
			}
			// Set the Bone Length.
			ImportedModelData.RefBonesBinary[i].BonePos.Length = ImportedModelData.RefBonesBinary[i].BonePos.Transform.GetLocation().Size();
		}
	}
#endif

	// Unreal doesn't support more than `MAX_TOTAL_INFLUENCES` BoneInfluences.
	check(MaxBoneInfluences <= MAX_TOTAL_INFLUENCES);

	// Unreal doesn't support more than `MAX_STATIC_TEXCOORDS`.
	check(UVCount <= MAX_STATIC_TEXCOORDS);

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

#if WITH_EDITORONLY_DATA
	FSkeletalMeshLODModel* SkeletalMeshLODModel = new FSkeletalMeshLODModel();
	SkeletalMesh->GetImportedModel()->LODModels.Add(SkeletalMeshLODModel);

	SkeletalMeshLODModel->NumVertices = Vertices.Num();
	SkeletalMeshLODModel->NumTexCoords = UVCount;

	SkeletalMeshLODModel->Sections.SetNum(Surfaces.Num());
	SkeletalMeshLODModel->MaxImportVertex = Vertices.Num() - 1;

	ImportedModelData.NumTexCoords = UVCount;
	ImportedModelData.MaxMaterialIndex = Surfaces.Num() - 1;
	ImportedModelData.bHasVertexColors = Surfaces[0].Colors.Num() > 0;;
	ImportedModelData.bHasNormals = true;
	ImportedModelData.bHasTangents = true;
	ImportedModelData.bUseT0AsRefPose = false;
	ImportedModelData.bDiffPose = false;
#endif

	LODMeshRenderData->RenderSections.SetNum(Surfaces.Num());

	for (int32 I = 0; I < Surfaces.Num(); I++)
	{
		const FMeshSurface& Surface = Surfaces[I];
		FSkelMeshRenderSection& RenderSection = LODMeshRenderData->RenderSections[I];

		RenderSection.bDisabled = false;
		RenderSection.BaseVertexIndex = SurfaceVertexOffsets[I];
		RenderSection.NumVertices = Surface.Vertices.Num();
		RenderSection.BaseIndex = SurfaceIndexOffsets[I];
		RenderSection.NumTriangles = Surface.Indices.Num() / 3;
		RenderSection.MaterialIndex = Surfaces[I].MaterialIndex;
		RenderSection.bCastShadow = true;
		RenderSection.bRecomputeTangent = false;
		RenderSection.MaxBoneInfluences = MaxBoneInfluences;

#if WITH_EDITOR
		FSkelMeshSection& MeshSection = SkeletalMeshLODModel->Sections[I];
		MeshSection.bDisabled = RenderSection.bDisabled;
		MeshSection.bRecomputeTangent = RenderSection.bRecomputeTangent;
		MeshSection.bCastShadow = RenderSection.bCastShadow;
		MeshSection.BaseVertexIndex = RenderSection.BaseVertexIndex;
		MeshSection.BaseIndex = RenderSection.BaseIndex;
		MeshSection.MaterialIndex = RenderSection.MaterialIndex;
		MeshSection.NumVertices = RenderSection.NumVertices;
		MeshSection.NumTriangles = RenderSection.NumTriangles;
		MeshSection.MaxBoneInfluences = RenderSection.MaxBoneInfluences;
		MeshSection.bUse16BitBoneIndex = bUse16BitBoneIndex;
		MeshSection.OriginalDataSectionIndex = I; // Section IDX for below lookup in user sections data

		MeshSection.SoftVertices.SetNum(Surface.Vertices.Num());
		for (int32 v = 0; v < Surface.Vertices.Num(); v += 1)
		{
			MeshSection.SoftVertices[v].Position = FVector3f(Surface.Vertices[v]);
			MeshSection.SoftVertices[v].TangentX = FVector3f(Surface.Tangents[v]);
			MeshSection.SoftVertices[v].TangentY = FVector3f(FVector::CrossProduct(Surface.Normals[v], Surface.Tangents[v]) * (Surface.FlipBinormalSigns[v] ? -1.0 : 1.0));
			MeshSection.SoftVertices[v].TangentZ = FVector3f(Surface.Normals[v]);
			FMemory::Memcpy(
				MeshSection.SoftVertices[v].UVs,
				Surface.Uvs[v].GetData(),
				sizeof(FVector2D) * UVCount);
			if (Surface.Colors.Num() > v)
			{
				MeshSection.SoftVertices[v].Color = Surface.Colors[v];
			}

			const TArray<FRawBoneInfluence>& VertInfluences = Surface.BoneInfluences[v];
			
			memset(MeshSection.SoftVertices[v].InfluenceWeights, 0, sizeof(MeshSection.SoftVertices[v].InfluenceWeights));
			memset(MeshSection.SoftVertices[v].InfluenceBones, 0, sizeof(MeshSection.SoftVertices[v].InfluenceBones));

			int nMax = std::min(VertInfluences.Num(), MAX_TOTAL_INFLUENCES);
			for (int InfluenceIndex = 0; InfluenceIndex < nMax; InfluenceIndex += 1)
			{
				const int32 VertexIndex = SurfaceVertexOffsets[I] + v;
				const FRawBoneInfluence& VertInfluence = VertInfluences[InfluenceIndex];
				// Make sure these are the same.
				check(v == VertInfluence.VertexIndex);

				// Convert 0.0 - 1.0 range to 0 - 255
				const uint8 EncodedWeight =
					FMath::Clamp(VertInfluence.Weight, 0.f, 1.f) * 255.f;

				MeshSection.SoftVertices[v].InfluenceWeights[InfluenceIndex] = EncodedWeight;
				MeshSection.SoftVertices[v].InfluenceBones[InfluenceIndex] = EncodedWeight == 0 ? 0 : VertInfluence.BoneIndex;
			}
		}

		{
			// In Editor we want to make sure the data is in sync between
			// `UserSectionsData` and RenderSections.
			FSkelMeshSourceSectionUserData& UserSectionData = SkeletalMeshLODModel->UserSectionsData.FindOrAdd(I);
			UserSectionData.bDisabled = MeshSection.bDisabled;
			UserSectionData.bCastShadow = MeshSection.bCastShadow;
			UserSectionData.bRecomputeTangent = MeshSection.bRecomputeTangent;
			UserSectionData.RecomputeTangentsVertexMaskChannel = MeshSection.RecomputeTangentsVertexMaskChannel;
			UserSectionData.GenerateUpToLodIndex = MeshSection.GenerateUpToLodIndex;

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
#if WITH_EDITOR
		SkeletalMeshLODModel->IndexBuffer = Indices;
#endif

		LODMeshRenderData->MultiSizeIndexContainer.RebuildIndexBuffer(
			// Dynamically chose the index buffer size.
			Indices.Num() < MAX_uint16 ? sizeof(uint16) : sizeof(uint32),
			Indices);

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
	LODMeshRenderData->StaticVertexBuffers.ColorVertexBuffer.Init(
		StaticVertices,
		NeedCPUAccess);
	LODMeshRenderData->StaticVertexBuffers.StaticMeshVertexBuffer.Init(
		StaticVertices,
		UVCount,
		NeedCPUAccess);

	LODMeshRenderData->SkinWeightVertexBuffer.SetMaxBoneInfluences(MaxBoneInfluences);
	LODMeshRenderData->SkinWeightVertexBuffer.SetUse16BitBoneIndex(bUse16BitBoneIndex);

	TArray<FSkinWeightInfo> Weights;
	Weights.Empty();
	Weights.SetNum(Vertices.Num());

	for (int WeightIdx = 0; WeightIdx < Weights.Num(); WeightIdx++)
	{
		for (int InfluenceIdx = 0; InfluenceIdx < MAX_TOTAL_INFLUENCES; ++InfluenceIdx)
		{
			Weights[WeightIdx].InfluenceBones[InfluenceIdx] = INDEX_NONE;
			Weights[WeightIdx].InfluenceWeights[InfluenceIdx] = 0;
		}
	}

	for (int32 I = 0; I < Surfaces.Num(); I++)
	{
		const FMeshSurface& Surface = Surfaces[I];

		for (int32 LocalVertexIndex = 0; LocalVertexIndex < Surface.BoneInfluences.Num(); LocalVertexIndex += 1)
		{
			const TArray<FRawBoneInfluence>& VertInfluences = Surface.BoneInfluences[LocalVertexIndex];
			const int32 VertexIndex = SurfaceVertexOffsets[I] + LocalVertexIndex;
			FSkinWeightInfo& Weight = Weights[VertexIndex];
			

			for (int InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; InfluenceIndex++)
			{
				if (InfluenceIndex >= VertInfluences.Num())
				{
					// The available influences for this surface, is smaller than the whole
					// Mesh. This happens when the user submits surfaces with different
					// bone weights. Pad it automatically, to simplify the user life.
					Weight.InfluenceWeights[InfluenceIndex] = 0;
					Weight.InfluenceBones[InfluenceIndex] = INDEX_NONE;
				}
				else
				{
					const FRawBoneInfluence& VertInfluence = VertInfluences[InfluenceIndex];
					// Make sure these are the same.
					check(LocalVertexIndex == VertInfluence.VertexIndex);

					if (!SkeletalMesh->GetSkeleton()->GetReferenceSkeleton().IsValidIndex(VertInfluence.BoneIndex))
					{
						// This bone appear to be invalid, continue.
						UE_LOG(LogTemp, Warning, TEXT("The bone %i isn't found in this skeleton"), VertInfluence.BoneIndex);
						continue;
					}

					// Convert 0.0 - 1.0 range to 0 - 255
					const uint8 EncodedWeight =
						FMath::Clamp(VertInfluence.Weight, 0.f, 1.f) * 255.f;
					Weight.InfluenceWeights[InfluenceIndex] = EncodedWeight;
					Weight.InfluenceBones[InfluenceIndex] = EncodedWeight == 0 ? INDEX_NONE : VertInfluence.BoneIndex;

#if WITH_EDITORONLY_DATA
					if (Weight.InfluenceBones[InfluenceIndex] != INDEX_NONE)
					{
						SkeletalMeshImportData::FRawBoneInfluence& Influence = ImportedModelData.Influences.AddDefaulted_GetRef();
						Influence.Weight = FMath::Clamp(static_cast<float>(Weight.InfluenceWeights[InfluenceIndex]) / 255.0, 0.0, 1.0);
						Influence.BoneIndex = Weight.InfluenceBones[InfluenceIndex];
						Influence.VertexIndex = VertexIndex;
					}
#endif
				}
				
			}
			
		}
	}

	// Enables all the Bones of this skeleton, to avoid break the mesh.
#if WITH_EDITOR
	SkeletalMeshLODModel->ActiveBoneIndices.Empty();
	SkeletalMeshLODModel->RequiredBones.Empty();
#endif
	LODMeshRenderData->RequiredBones.Empty();
	LODMeshRenderData->ActiveBoneIndices.Empty();

	for (int32 I = 0; I < Surfaces.Num(); I++)
	{
		FSkelMeshRenderSection& RenderSection = LODMeshRenderData->RenderSections[I];
		RenderSection.BoneMap.Empty();
#if WITH_EDITOR
		FSkelMeshSection& MeshSection = SkeletalMeshLODModel->Sections[I];
		MeshSection.BoneMap.Empty();
#endif
	}

	const int32 BoneNum = SkeletalMesh->Skeleton->GetReferenceSkeleton().GetNum();
	for (int32 BoneIndex = 0; BoneIndex < BoneNum; BoneIndex++)
	{
#if WITH_EDITOR
		SkeletalMeshLODModel->ActiveBoneIndices.AddUnique(BoneIndex);
		SkeletalMeshLODModel->RequiredBones.AddUnique(BoneIndex);
#endif
		LODMeshRenderData->RequiredBones.AddUnique(BoneIndex);
		LODMeshRenderData->ActiveBoneIndices.AddUnique(BoneIndex);

		for (int32 I = 0; I < Surfaces.Num(); I++)
		{
			FSkelMeshRenderSection& RenderSection = LODMeshRenderData->RenderSections[I];
			RenderSection.BoneMap.AddUnique(BoneIndex);
#if WITH_EDITOR
			FSkelMeshSection& MeshSection = SkeletalMeshLODModel->Sections[I];
			MeshSection.BoneMap.AddUnique(BoneIndex);
#endif
		}
	}

	// Set the skin weights.
	LODMeshRenderData->SkinWeightVertexBuffer = Weights;

	// Set the default Material.
	for (auto Material : SurfacesMaterial)
	{
		SkeletalMesh->GetMaterials().Add(Material);
	}

	// Rebuild inverse ref pose matrices.
	SkeletalMesh->SkelMirrorTable.Empty();
	SkeletalMesh->SkelMirrorAxis = EAxis::Type::None;
	SkeletalMesh->SkelMirrorFlipAxis = EAxis::Type::None;
	SkeletalMesh->GetRefBasesInvMatrix().Empty();
	SkeletalMesh->CalculateInvRefMatrices(); 
	MeshRenderData->bReadyForStreaming = false;
	

	// Suspected Finalization step
	if (!GIsEditor)
	{
		SkeletalMesh->NeverStream = false;
	}

#if WITH_EDITOR
	if (SkeletalMesh->GetLODSettings() != nullptr)
	{
		// update mapping information on the class
		SkeletalMesh->GetLODSettings()->SetLODSettingsFromMesh(SkeletalMesh);

		checkf(SkeletalMesh->GetLODSettings() != nullptr, TEXT("At this point the LODSetings are supposed to be set."));

		const int32 NumSettings = FMath::Min(SkeletalMesh->GetLODSettings()->GetNumberOfSettings(), SkeletalMesh->GetLODNum());
		checkf(LODIndex < NumSettings, TEXT("Make sure the LODSettings are set for the LODIndex 0."));

		const FSkeletalMeshLODGroupSettings* SkeletalMeshLODGroupSettings = &SkeletalMesh->GetLODSettings()->GetSettingsForLODLevel(LODIndex);
		MeshLodInfo.BuildGUID = MeshLodInfo.ComputeDeriveDataCacheKey(SkeletalMeshLODGroupSettings);
	}

	const FString BuildStringID = SkeletalMesh->GetImportedModel()->LODModels[0].GetLODModelDeriveDataKey();
	SkeletalMesh->GetImportedModel()->LODModels[0].BuildStringID = BuildStringID;

	SkeletalMesh->SetLODImportedDataVersions(0, ESkeletalMeshGeoImportVersions::LatestVersion, ESkeletalMeshSkinningImportVersions::LatestVersion);
	SkeletalMesh->SaveLODImportedData(0, ImportedModelData);
	SkeletalMesh->InvalidateDeriveDataCacheGUID();
#endif

	// Calls InitResources.
	SkeletalMesh->PostLoad();

#if WITH_EDITOR
	// Signals to editor we are done with our changes
	// This is to prevent the editor variable changes overwriting the import mesh,
	// if you don't set this random crashes occur.
	SkeletalMesh->StackPostEditChange();
#endif
}

USkeletalMeshComponent* FRuntimeSkeletalMeshGenerator::GenerateSkeletalMeshComponent(
	AActor* Actor,
	USkeleton* BaseSkeleton,
	const TArray<FMeshSurface>& Surfaces,
	const TArray<UMaterialInterface*>& SurfacesMaterial,
	const TMap<FName, FTransform>& BoneTransformsOverride)
{
	// Note: we do not pass anything so the skeletal mesh is transient and
	// destroyed when the play session end.
	USkeletalMesh* SkeletalMesh = NewObject<USkeletalMesh>();
	SkeletalMesh->SetRefSkeleton(BaseSkeleton->GetReferenceSkeleton());
	SkeletalMesh->SetSkeleton(BaseSkeleton);


	GenerateSkeletalMesh(
		SkeletalMesh,
		Surfaces,
		SurfacesMaterial,
		BoneTransformsOverride);

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

void FRuntimeSkeletalMeshGenerator::UpdateSkeletalMeshComponent(
	USkeletalMeshComponent* SkeletalMeshComponent,
	USkeleton* BaseSkeleton,
	const TArray<FMeshSurface>& Surfaces,
	const TArray<UMaterialInterface*>& SurfacesMaterial,
	const TMap<FName, FTransform>& BoneTransformOverrides)
{
	// Note: we do not pass anything so the skeletal mesh is transient and
	// destroyed when the play session end.
	USkeletalMesh* SkeletalMesh = NewObject<USkeletalMesh>();
	SkeletalMesh->SetRefSkeleton(BaseSkeleton->GetReferenceSkeleton());
	SkeletalMesh->SetSkeleton(BaseSkeleton);

	GenerateSkeletalMesh(
		SkeletalMesh,
		Surfaces,
		SurfacesMaterial,
		BoneTransformOverrides);

	// We register the skeleton resource (which is not meant to be transient to
	// the engine).
	SkeletalMeshComponent->SetSkeletalMesh(SkeletalMesh);

	check(SkeletalMeshComponent->RequiredBones.Num() != 0);
	check(SkeletalMeshComponent->FillComponentSpaceTransformsRequiredBones.Num() != 0);
}

bool FRuntimeSkeletalMeshGenerator::DecomposeSkeletalMesh(
	/// The `SkeletalMesh` to decompose
	const USkeletalMesh* SkeletalMesh,
	/// Out Surfaces.
	TArray<FMeshSurface>& OutSurfaces,
	/// The vertex offsets for each surface, relative to the passed `SkeletalMesh`
	TArray<int32>& OutSurfacesVertexOffsets,
	/// The index offsets for each surface, relative to the passed `SkeletalMesh`
	TArray<int32>& OutSurfacesIndexOffsets,
	/// Out Materials used.
	TArray<UMaterialInterface*>& OutSurfacesMaterial)
{
	OutSurfaces.Empty();
	OutSurfacesVertexOffsets.Empty();
	OutSurfacesIndexOffsets.Empty();
	OutSurfacesMaterial.Empty();

	// Assume only the LOD 0 exist.
	constexpr int32 LODIndex = 0;

	// Extract the Surfaces from the `SourceMesh`
	const int32 RenderSectionsNum = SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex].RenderSections.Num();;

	OutSurfaces.SetNum(RenderSectionsNum);
	OutSurfacesVertexOffsets.SetNum(RenderSectionsNum);
	OutSurfacesIndexOffsets.SetNum(RenderSectionsNum);

	const FSkeletalMeshLODRenderData& RenderData = SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex];

	TArray<uint32> IndexBuffer;
	RenderData.MultiSizeIndexContainer.GetIndexBuffer(IndexBuffer);

	for (int32 SectionIndex = 0; SectionIndex < RenderSectionsNum; SectionIndex += 1)
	{
		FMeshSurface& Surface = OutSurfaces[SectionIndex];

		const FSkelMeshRenderSection& RenderSection = RenderData.RenderSections[SectionIndex];

		const uint32 VertexIndexOffset = RenderSection.BaseVertexIndex;
		const uint32 VertexNum = RenderSection.NumVertices;

		OutSurfacesVertexOffsets[SectionIndex] = VertexIndexOffset;

		Surface.Vertices.SetNum(VertexNum);
		Surface.Normals.SetNum(VertexNum);
		Surface.Tangents.SetNum(VertexNum);
		Surface.FlipBinormalSigns.SetNum(VertexNum);
		Surface.Uvs.SetNum(VertexNum);
		Surface.Colors.SetNum(VertexNum);
		Surface.BoneInfluences.SetNum(VertexNum);

		for (uint32 i = 0; i < VertexNum; i += 1)
		{
			const uint32 VertexIndex = VertexIndexOffset + i;

			Surface.Vertices[i] = FVector(RenderData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(VertexIndex));

			Surface.Normals[i] = FVector(RenderData.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(VertexIndex));
			Surface.Tangents[i] = FVector(RenderData.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentX(VertexIndex));
			// Check if the Binormal Sign is flipped.
			const FVector& ActualBinormal = FVector(RenderData.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentY(VertexIndex));
			const FVector CalculatedBinormal = FVector::CrossProduct(Surface.Normals[i], Surface.Tangents[i]);
			// If the Binormal points toward different location, the `FlipBinormalSign`
			// must be `false`. Check how the `VertexTangentY` is computed above.
			Surface.FlipBinormalSigns[i] = FVector::DotProduct(ActualBinormal, CalculatedBinormal) < 0.99;

			Surface.Uvs[i].SetNum(RenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords());
			for (uint32 UVIndex = 0; UVIndex < RenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords(); UVIndex += 1)
			{
				Surface.Uvs[i][UVIndex] = FVector2d(RenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV(VertexIndex, UVIndex));
			}

			if (VertexIndex < RenderData.StaticVertexBuffers.ColorVertexBuffer.GetNumVertices())
			{
				// Make sure the color is set. Not all meshes have vertex colors.
				Surface.Colors[i] = RenderData.StaticVertexBuffers.ColorVertexBuffer.VertexColor(VertexIndex);
			}

			checkf(int32(RenderData.SkinWeightVertexBuffer.GetMaxBoneInfluences()) >= RenderSection.MaxBoneInfluences, TEXT("These two MUST be the same."));

			Surface.BoneInfluences[i].SetNum(RenderSection.MaxBoneInfluences);
			for (
				int32 BoneInfluenceIndex = 0;
				BoneInfluenceIndex < RenderSection.MaxBoneInfluences;
				BoneInfluenceIndex += 1)
			{
				Surface.BoneInfluences[i][BoneInfluenceIndex].VertexIndex = i;
				Surface.BoneInfluences[i][BoneInfluenceIndex].BoneIndex =
					RenderSection.BoneMap[RenderData.SkinWeightVertexBuffer.GetBoneIndex(VertexIndex, BoneInfluenceIndex)];
				Surface.BoneInfluences[i][BoneInfluenceIndex].Weight =
					FMath::Clamp(static_cast<float>(RenderData.SkinWeightVertexBuffer.GetBoneWeight(VertexIndex, BoneInfluenceIndex)) / 255.0, 0.0, 1.0);
			}
		}

		const uint32 IndexIndexOffset = RenderSection.BaseIndex;
		const uint32 IndexCount = RenderSection.NumTriangles * 3;

		OutSurfacesIndexOffsets[SectionIndex] = IndexIndexOffset;
		Surface.Indices.SetNum(IndexCount);

		for (uint32 i = 0; i < IndexCount; i += 1)
		{
			const uint32 Index = i + IndexIndexOffset;
			// Note: Subtracting the `VertexIndexOffset` to obtain the index relative
			// on the surface.
			Surface.Indices[i] = IndexBuffer[Index] - VertexIndexOffset;
		}
	}

	OutSurfacesMaterial.Reserve(SkeletalMesh->GetMaterials().Num());
	for (const FSkeletalMaterial& Material : SkeletalMesh->GetMaterials())
	{
		OutSurfacesMaterial.Add(Material.MaterialInterface);
	}

	return true;
}
