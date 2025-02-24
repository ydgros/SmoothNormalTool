#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

struct NormalInfor
{
	FVector3f normal;
	float angle;
};
class SMOOTHNORMALTOOL_API SmoothNormalCommand
{
private:
	static void SmoothNormalSkeletalMesh(FAssetData AssetData);
	static void SmoothNormalStaticMesh(FAssetData AssetData);
	static void SmoothNormalStaticMeshTriangle(FAssetData AssetData);
public:
	static void SmoothNormal(TArray<FAssetData> SelectedAssets);
};


