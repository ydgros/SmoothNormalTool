#include "SmoothNormalCommand.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "RawMesh.h"
#include "math.h"
#include "MeshUtilities.h"
#include "Rendering/SkeletalMeshModel.h"

namespace
{
	inline FVector ToFVector(const FVector3f& Value)
	{
		return FVector(Value.X, Value.Y, Value.Z);
	}

	inline FVector ToFVector(const FVector4f& Value)
	{
		return FVector(Value.X, Value.Y, Value.Z);
	}

	inline FVector3f ToFVector3f(const FVector& Value)
	{
		return FVector3f(Value.X, Value.Y, Value.Z);
	}
}

inline void SmoothNormalCommand::SmoothNormal(TArray<FAssetData> SelectedAssets)
{
	for (int i = 0; i < SelectedAssets.Num(); i++)
	{
		if (SelectedAssets[i].AssetClassPath == UStaticMesh::StaticClass()->GetClassPathName())
		{
			SmoothNormalStaticMeshTriangle(SelectedAssets[i]);
		}
		if (SelectedAssets[i].AssetClassPath == USkeletalMesh::StaticClass()->GetClassPathName())
		{
			SmoothNormalSkeletalMesh(SelectedAssets[i]);
		}
	}
}

void WeildVertex(TMap<FVector, FVector>& VertexNormalMap,TMap<FVector, FVector>& VertexWieldRemap)
{
	TArray<FVector> AllPositions;
	TArray<FVector> WieldPositions;
	VertexNormalMap.GetKeys(AllPositions);

	for (int i = 0; i < AllPositions.Num(); i++)
	{
		int foundIndex = INDEX_NONE;
		FVector Cur = AllPositions[i];
		for (int j = 0; j < WieldPositions.Num(); j++)
		{
			if (Cur.Equals(WieldPositions[j], 0.1f))
			{
				foundIndex = j;
				break;
			}
		}
		if (foundIndex == INDEX_NONE)
		{
			WieldPositions.Add(Cur);
			VertexWieldRemap.Add(Cur, Cur);
		}
		else
		{
			VertexWieldRemap.Add(Cur, WieldPositions[foundIndex]);
		}
	}
	for (int i = 0; i < AllPositions.Num(); i++)
	{
		FVector Cur = AllPositions[i];
		FVector Weild = VertexWieldRemap[Cur];
		if (Weild != Cur)
		{
			//VertexNormalMap[Weild] += VertexNormalMap[Cur];
		}
	}
}

void SmoothNormalCommand::SmoothNormalStaticMesh(FAssetData AssetData)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(AssetData.GetAsset());
	const FStaticMeshVertexBuffers& VertexBuffers = StaticMesh->GetRenderData()->LODResources[0].VertexBuffers;
	const FPositionVertexBuffer& PositionBuffer = VertexBuffers.PositionVertexBuffer;
	const FStaticMeshVertexBuffer& VertexBuffer = VertexBuffers.StaticMeshVertexBuffer;

	const TArray<int32>& WedgeMap = StaticMesh->GetRenderData()->LODResources[0].WedgeMap;

	TMap<FVector, FVector> VertexNormalMap;
	TMap<FVector, FVector> VertexWieldRemap;
	for(int Index = 0; Index < StaticMesh->GetNumSourceModels(); Index++)
	{
		FStaticMeshSourceModel& SourceModel = const_cast<FStaticMeshSourceModel&>(StaticMesh->GetSourceModels()[Index]);
		FRawMesh RawMesh;

		SourceModel.LoadRawMesh(RawMesh);

		for (int WedgeIndex = 0; WedgeIndex < RawMesh.WedgeIndices.Num(); WedgeIndex++)
		{
			int RawVertexIndex = RawMesh.WedgeIndices[WedgeIndex];
			FVector RawVertexPosition = ToFVector(RawMesh.VertexPositions[RawVertexIndex]);
		

			int VertexIndex = WedgeMap[WedgeIndex];
			FVector RenderVetexPosition = ToFVector(PositionBuffer.VertexPosition(VertexIndex));
			FVector RenderVertexNormal = ToFVector(VertexBuffer.VertexTangentZ(VertexIndex));
			if(!VertexNormalMap.Contains(RawVertexPosition))
			{
				VertexNormalMap.Add(RawVertexPosition, FVector::ZeroVector);
			}
			VertexNormalMap[RawVertexPosition] += RenderVertexNormal;
		}

		WeildVertex(VertexNormalMap, VertexWieldRemap);
		
		if(RawMesh.WedgeTexCoords[1].Num() == 0)
		{
			RawMesh.WedgeTexCoords[1].AddDefaulted(RawMesh.WedgeIndices.Num());
		}
		if(RawMesh.WedgeTexCoords[2].Num() == 0)
		{
			RawMesh.WedgeTexCoords[2].AddDefaulted(RawMesh.WedgeIndices.Num());
		}
		RawMesh.WedgeTexCoords[3].Empty();
		for (int WedgeIndex = 0; WedgeIndex < RawMesh.WedgeIndices.Num(); WedgeIndex++)
		{
			FVector RawVertexPosition = ToFVector(RawMesh.VertexPositions[RawMesh.WedgeIndices[WedgeIndex]]);

			int RenderVertexIndex = WedgeMap[WedgeIndex];
			FVector VertexTangentZ = ToFVector(VertexBuffer.VertexTangentZ(RenderVertexIndex));
			FVector VertexTangentX = ToFVector(VertexBuffer.VertexTangentX(RenderVertexIndex));
			FVector VertexTangentY = ToFVector(VertexBuffer.VertexTangentY(RenderVertexIndex));
			
			FVector WeidlRemapVertex = VertexWieldRemap[RawVertexPosition];
			FVector SmoothNormal = VertexNormalMap[WeidlRemapVertex].GetSafeNormal();
			FVector SmoothNormalAtTangent = FVector::ZeroVector;

			if (VertexTangentX != FVector::ZeroVector
				&&VertexTangentY != FVector::ZeroVector
				&&VertexTangentZ != FVector::ZeroVector)
			{
				FMatrix TangentToNormal(VertexTangentX, VertexTangentY, VertexTangentZ, FVector(0, 0, 0));
				//将平均法线转换到切线空间存储
				SmoothNormalAtTangent = TangentToNormal.InverseTransformVector(SmoothNormal).GetSafeNormal();
			}
			else
			{
				SmoothNormalAtTangent = FVector::ZeroVector;
			}
			RawMesh.WedgeTexCoords[3].Add(FVector2f(SmoothNormalAtTangent.X, SmoothNormalAtTangent.Y));
			RawMesh.WedgeTangentZ[WedgeIndex] = ToFVector3f(SmoothNormal);
		}
		SourceModel.SaveRawMesh(RawMesh);
	}
	StaticMesh->Build(false);
	StaticMesh->PostEditChange();

	StaticMesh->MarkPackageDirty();
}

void BuildSoftSkinVertexMap(TArray<FSoftSkinVertex>& Vertices, TMap<FVector, TArray<FSoftSkinVertex>>& VertexSkinMap)
{
	for (int i = 0; i < Vertices.Num(); i++)
	{
		const FVector PositionKey = ToFVector(Vertices[i].Position);
		if (VertexSkinMap.Contains(PositionKey))
		{
			VertexSkinMap[PositionKey].Add(Vertices[i]);
		}
		else
		{
			VertexSkinMap.Add(PositionKey, TArray<FSoftSkinVertex>());
			VertexSkinMap[PositionKey].Add(Vertices[i]);
		}
	}
}

FSoftSkinVertex* FindSoftSkinVertex(TMap<FVector, TArray<FSoftSkinVertex>>& VertexSkinMap,FVector Center, FVector Position,FVector Normal,FVector2f UV0)
{
	FSoftSkinVertex *Result = NULL;
	if (VertexSkinMap.Contains(Center))
	{
		TArray<FSoftSkinVertex>& Array = VertexSkinMap[Center];
		for (int i = 0; i < Array.Num(); i++)
		{
			if (ToFVector(Array[i].Position) == Position && Array[i].UVs[0] == UV0)
			{
				if (Result == NULL)
				{
					Result = &Array[i];
				}
				else
				{
					Result = &Array[i];
				}
			}

		}
	}
	return Result;
}

void SmoothNormalCommand::SmoothNormalSkeletalMesh(FAssetData AssetData)
{

	UObject* obj = AssetData.GetAsset();

	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(obj);

	SkeletalMesh->Build();

	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>(TEXT("MeshUtilities"));
	FMeshBuildSettings MeshBuildSettings;
	MeshBuildSettings.bRemoveDegenerates = true;
	MeshBuildSettings.bUseMikkTSpace = false;

	TMap<FVector, FVector> VertexNormalMap;

	FSkeletalMeshImportData RawMesh;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SkeletalMesh->LoadLODImportedData(0, RawMesh);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	TMap<FVector, TArray<FSoftSkinVertex>> VertexSkinMap;
	{
		FSkeletalMeshModel* SkelMeshModel = SkeletalMesh->GetImportedModel();

		FSkeletalMeshLODModel* ImportedModel = &SkelMeshModel->LODModels[0];
		TArray<FSoftSkinVertex> Vertices;
		ImportedModel->GetVertices(Vertices);

		int NumFaces = ImportedModel->IndexBuffer.Num() / 3;

		for (int i = 0; i < NumFaces; i++)
		{
			FVector Center=FVector::ZeroVector;

			bool flag=false;

			for (int j = 0; j < 3; j++)
			{
				int VertIndex = ImportedModel->IndexBuffer[i * 3 + j];

				Center += ToFVector(Vertices[VertIndex].Position);

				int ZeroCount = Vertices[VertIndex].TangentX.IsZero()+ Vertices[VertIndex].TangentY.IsZero()+ Vertices[VertIndex].TangentZ.IsNearlyZero3();
				if (ZeroCount>=2)
				{
					flag = true;
				}
			}
			if (flag)
			{
				continue;
			}
			Center /= 3;
			if (!VertexSkinMap.Contains(Center))
			{
				VertexSkinMap.Add(Center, TArray<FSoftSkinVertex>());
			}
			else
			{
				flag = true;
			}
			for (int j = 0; j < 3; j++)
			{
				int VertIndex = ImportedModel->IndexBuffer[i * 3 + j];

				VertexSkinMap[Center].Add(Vertices[VertIndex]);

			}
		}
	}



	for (int FaceIndex = 0; FaceIndex < RawMesh.Faces.Num(); FaceIndex++)
	{
		SkeletalMeshImportData::FTriangle Face = RawMesh.Faces[FaceIndex];

		for (int i = 0; i < 3; i++)
		{
			SkeletalMeshImportData::FVertex Wedge = RawMesh.Wedges[Face.WedgeIndex[i]];
			FVector VertexPosition = ToFVector(RawMesh.Points[Wedge.VertexIndex]);
			FVector VertexNormal = ToFVector(Face.TangentZ[i]);
			if (!VertexNormalMap.Contains(VertexPosition))
			{
				VertexNormalMap.Add(VertexPosition, FVector::ZeroVector);
			}
			VertexNormalMap[VertexPosition] += VertexNormal;
		}

	}



	for (int FaceIndex = 0; FaceIndex < RawMesh.Faces.Num(); FaceIndex++)
	{
		SkeletalMeshImportData::FTriangle Face = RawMesh.Faces[FaceIndex];

		FVector Center=FVector::ZeroVector;
		for (int i = 0; i < 3; i++)
		{
			SkeletalMeshImportData::FVertex Wedge = RawMesh.Wedges[Face.WedgeIndex[i]];
			FVector VertexPosition = ToFVector(RawMesh.Points[Wedge.VertexIndex]);
			Center += VertexPosition;
		}
		Center /= 3;

		for (int i = 0; i < 3; i++)
		{
			SkeletalMeshImportData::FVertex Wedge = RawMesh.Wedges[Face.WedgeIndex[i]];
			FVector VertexPosition = ToFVector(RawMesh.Points[Wedge.VertexIndex]);

			FVector SmoothNormal = VertexNormalMap[VertexPosition].GetSafeNormal();

			FSoftSkinVertex* SkinVertex = FindSoftSkinVertex(VertexSkinMap, Center, VertexPosition, ToFVector(Face.TangentZ[i]),Wedge.UVs[0]);

			FVector SmoothNormalAtTangent;
			if (SkinVertex!=NULL)
			{
				FVector TangentX = ToFVector(SkinVertex->TangentX);
				FVector TangentY = ToFVector(SkinVertex->TangentY);
				FVector TangentZ = ToFVector(SkinVertex->TangentZ);

				FMatrix TangentToNormal(TangentX, TangentY, TangentZ, FVector(0, 0, 0));

				SmoothNormalAtTangent = TangentToNormal.InverseTransformVector(SmoothNormal);
			}
			else
			{
				SmoothNormalAtTangent = FVector(0, 0, 1);
				SkinVertex = FindSoftSkinVertex(VertexSkinMap, Center, VertexPosition, ToFVector(Face.TangentZ[i]), Wedge.UVs[0]);
			}
			
			RawMesh.Wedges[Face.WedgeIndex[i]].UVs[1] = FVector2f::ZeroVector;
			RawMesh.Wedges[Face.WedgeIndex[i]].UVs[2] = FVector2f::ZeroVector;
			RawMesh.Wedges[Face.WedgeIndex[i]].UVs[3] =(FVector2f(SmoothNormalAtTangent.X, SmoothNormalAtTangent.Y));
			
		}

	}
	RawMesh.NumTexCoords = 4;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SkeletalMesh->SaveLODImportedData(0, RawMesh);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	SkeletalMesh->Build();
	SkeletalMesh->PostEditChange();

	SkeletalMesh->MarkPackageDirty();
}

void SmoothNormalCommand::SmoothNormalStaticMeshTriangle(FAssetData AssetData)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(AssetData.GetAsset());
	const FStaticMeshVertexBuffers& VertexBuffers = StaticMesh->GetRenderData()->LODResources[0].VertexBuffers;
	const FPositionVertexBuffer& PositionBuffer = VertexBuffers.PositionVertexBuffer;
	const FStaticMeshVertexBuffer& VertexBuffer = VertexBuffers.StaticMeshVertexBuffer;

	const TArray<int32>& WedgeMap = StaticMesh->GetRenderData()->LODResources[0].WedgeMap;

	TMap<FVector, FVector> VertexNormalMap;
	TMap<FVector, FVector> VertexWieldRemap;
	TMap<FVector, TArray<FVector>> WeightingNormalMap;
	for(int Index = 0; Index < StaticMesh->GetNumSourceModels(); Index++)
	{
		FStaticMeshSourceModel& SourceModel = const_cast<FStaticMeshSourceModel&>(StaticMesh->GetSourceModels()[Index]);
		FRawMesh RawMesh;

		SourceModel.LoadRawMesh(RawMesh);
		{
			check(RawMesh.WedgeIndices.Num() % 3 == 0);
		}
		for (int WedgeIndex = 0; WedgeIndex <= RawMesh.WedgeIndices.Num() -3; WedgeIndex = WedgeIndex+3)
		{
			int RawVertexIndex = RawMesh.WedgeIndices[WedgeIndex];
			int RawVertexIndex1 = RawMesh.WedgeIndices[WedgeIndex + 1];
			int RawVertexIndex2 = RawMesh.WedgeIndices[WedgeIndex + 2];
			FVector RawVertexPosition = ToFVector(RawMesh.VertexPositions[RawVertexIndex]);
			FVector RawVertexPosition1 = ToFVector(RawMesh.VertexPositions[RawVertexIndex1]);
			FVector RawVertexPosition2 = ToFVector(RawMesh.VertexPositions[RawVertexIndex2]);
			int VertexIndex = WedgeMap[WedgeIndex];
			int VertexIndex1 = WedgeMap[WedgeIndex + 1];
			int VertexIndex2 = WedgeMap[WedgeIndex + 2 ];
			FVector VertexNormal = ToFVector(VertexBuffer.VertexTangentZ(VertexIndex));

			FVector Side = RawVertexPosition1 - RawVertexPosition;
			FVector Side1 = RawVertexPosition2 - RawVertexPosition;
			float Angle = acos(FVector::DotProduct(Side.GetSafeNormal(), Side1.GetSafeNormal()));
			check(Angle>=0);
			VertexNormal *= Angle;
			if(!WeightingNormalMap.Contains(RawVertexPosition))
			{
				TArray<FVector> Normals;
				
				Normals.Add(VertexNormal);
				WeightingNormalMap.Add(RawVertexPosition, Normals);
			}
			else
			{

				auto& Array = WeightingNormalMap[RawVertexPosition];
				Array.Add(VertexNormal);
			}
			VertexNormal = ToFVector(VertexBuffer.VertexTangentZ(VertexIndex1));
			Side = RawVertexPosition - RawVertexPosition1;
			Side1 = RawVertexPosition2 - RawVertexPosition1;
			Angle = acos(FVector::DotProduct(Side.GetSafeNormal(), Side1.GetSafeNormal()));
			check(Angle>=0);
			VertexNormal *= Angle;
			if(!WeightingNormalMap.Contains(RawVertexPosition1))
			{
				TArray<FVector> Normals;
				
				Normals.Add(VertexNormal);
				WeightingNormalMap.Add(RawVertexPosition1, Normals);
			}
			else
			{

				auto& Array = WeightingNormalMap[RawVertexPosition1];
				Array.Add(VertexNormal);
			}
			VertexNormal = ToFVector(VertexBuffer.VertexTangentZ(VertexIndex2));
			Side = RawVertexPosition - RawVertexPosition2;
			Side1 = RawVertexPosition1 - RawVertexPosition2;
			Angle = acos(FVector::DotProduct(Side.GetSafeNormal(), Side1.GetSafeNormal()));
			check(Angle>=0);
			VertexNormal *= Angle;
			if(!WeightingNormalMap.Contains(RawVertexPosition2))
			{
				TArray<FVector> Normals;
				
				Normals.Add(VertexNormal);
				WeightingNormalMap.Add(RawVertexPosition2, Normals);
			}
			else
			{

				auto& Array = WeightingNormalMap[RawVertexPosition2];
				Array.Add(VertexNormal);
			}
		}
		if(RawMesh.WedgeTexCoords[1].Num() == 0)
		{
			RawMesh.WedgeTexCoords[1].AddDefaulted(RawMesh.WedgeIndices.Num());
		}
		if(RawMesh.WedgeTexCoords[2].Num() == 0)
		{
			RawMesh.WedgeTexCoords[2].AddDefaulted(RawMesh.WedgeIndices.Num());
		}
		RawMesh.WedgeTexCoords[3].Empty();
		
		for (int WedgeIndex = 0; WedgeIndex < RawMesh.WedgeIndices.Num(); WedgeIndex++)
		{
			FVector RawVertexPosition = ToFVector(RawMesh.VertexPositions[RawMesh.WedgeIndices[WedgeIndex]]);

			int RenderVertexIndex = WedgeMap[WedgeIndex];
			FVector VertexTangentZ = ToFVector(VertexBuffer.VertexTangentZ(RenderVertexIndex));
			FVector VertexTangentX = ToFVector(VertexBuffer.VertexTangentX(RenderVertexIndex));
			FVector VertexTangentY = ToFVector(VertexBuffer.VertexTangentY(RenderVertexIndex));
			FVector SmoothNormal = FVector::ZeroVector;
			if(!WeightingNormalMap.Contains(RawVertexPosition))
			{
				check(false);
			}
			else
			{
				const TArray<FVector>& WeightingNormals  = WeightingNormalMap[RawVertexPosition];
				for(const auto& Normal : WeightingNormals)
				{
					SmoothNormal += Normal;
				}
			}
			SmoothNormal = SmoothNormal.GetSafeNormal();
			FVector SmoothNormalAtTangent = FVector::ZeroVector;

			if (VertexTangentX != FVector::ZeroVector
				&&VertexTangentY != FVector::ZeroVector
				&&VertexTangentZ != FVector::ZeroVector)
			{
				FMatrix TangentToNormal(VertexTangentX, VertexTangentY, VertexTangentZ, FVector(0, 0, 0));
				SmoothNormalAtTangent = TangentToNormal.InverseTransformVector(SmoothNormal).GetSafeNormal();
			}
			else
			{
				SmoothNormalAtTangent = FVector::ZeroVector;
			}
			RawMesh.WedgeTexCoords[3].Add(FVector2f(SmoothNormalAtTangent.X, SmoothNormalAtTangent.Y));
			//RawMesh.WedgeTangentZ[WedgeIndex] = SmoothNormal;
		}
		SourceModel.SaveRawMesh(RawMesh);
	}
	StaticMesh->Build(false);
	StaticMesh->PostEditChange();

	StaticMesh->MarkPackageDirty();
}
