using System;
using System.Threading.Tasks;
using CUE4Parse.FileProvider;
using CUE4Parse.GameTypes.SMG.UE4.Assets.Exports.Wwise;
using CUE4Parse.UE4.Assets;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.UE4.Assets.Exports.Animation;
using CUE4Parse.UE4.Assets.Exports.AudioSynesthesia;
using CUE4Parse.UE4.Assets.Exports.BuildData;
using CUE4Parse.UE4.Assets.Exports.Component;
using CUE4Parse.UE4.Assets.Exports.Criware;
using CUE4Parse.UE4.Assets.Exports.CustomizableObject;
using CUE4Parse.UE4.Assets.Exports.Engine;
using CUE4Parse.UE4.Assets.Exports.Engine.Font;
using CUE4Parse.UE4.Assets.Exports.Fmod;
using CUE4Parse.UE4.Assets.Exports.Foliage;
using CUE4Parse.UE4.Assets.Exports.Internationalization;
using CUE4Parse.UE4.Assets.Exports.LevelSequence;
using CUE4Parse.UE4.Assets.Exports.Material;
using CUE4Parse.UE4.Assets.Exports.Material.Editor;
using CUE4Parse.UE4.Assets.Exports.Nanite;
using CUE4Parse.UE4.Assets.Exports.Niagara;
using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse.UE4.Assets.Exports.Sound;
using CUE4Parse.UE4.Assets.Exports.StaticMesh;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse.UE4.Assets.Exports.Wwise;
using CUE4Parse.UE4.Assets.Objects;
using CUE4Parse.UE4.Objects.Engine;
using CUE4Parse.UE4.Objects.Engine.Animation;
using CUE4Parse.UE4.Objects.Engine.Curves;
using CUE4Parse.UE4.Objects.MediaAssets;
using CUE4Parse.UE4.Objects.Niagara;
using CUE4Parse.UE4.Objects.PhysicsEngine;
using CUE4Parse.UE4.Objects.RigVM;
using CUE4Parse.UE4.Objects.UObject;
using CUE4Parse.UE4.Objects.UObject.Editor;
using PakViewer.Models;
using Serilog;

namespace PakViewer.Services;

public static class AssetTypeDetector
{
    public static (EAssetCategory Category, EBulkType Actions) Detect(IPackage pkg, string nameWithoutExtension)
    {
        try
        {
            var mainIndex = pkg.GetExportIndex(nameWithoutExtension, StringComparison.OrdinalIgnoreCase);
            if (mainIndex < 0) mainIndex = pkg.GetExportIndex($"{nameWithoutExtension}_C", StringComparison.OrdinalIgnoreCase);
            if (mainIndex < 0) mainIndex = 0;

            var pointer = new FPackageIndex(pkg, mainIndex + 1).ResolvedObject;
            if (pointer?.Object is null)
                return (EAssetCategory.All, EBulkType.None);

            var dummy = ((AbstractUePackage) pkg).ConstructObject(pointer.Class, pkg);

            return dummy switch
            {
                URigVMBlueprintGeneratedClass => (EAssetCategory.RigVMBlueprintGeneratedClass, EBulkType.Code),
                UAnimBlueprintGeneratedClass => (EAssetCategory.AnimBlueprintGeneratedClass, EBulkType.Code),
                UWidgetBlueprintGeneratedClass => (EAssetCategory.WidgetBlueprintGeneratedClass, EBulkType.Code),
                UBlueprintGeneratedClass or UFunction => (EAssetCategory.BlueprintGeneratedClass, EBulkType.Code),
                UUserDefinedEnum => (EAssetCategory.UserDefinedEnum, EBulkType.None),
                UUserDefinedStruct => (EAssetCategory.UserDefinedStruct, EBulkType.Code),
                UBlueprintCore => (EAssetCategory.Blueprint, EBulkType.Code),
                UClassCookedMetaData or UStructCookedMetaData or UEnumCookedMetaData => (EAssetCategory.CookedMetaData, EBulkType.None),

                UStaticMesh => (EAssetCategory.StaticMesh, EBulkType.Meshes),
                USkeletalMesh => (EAssetCategory.SkeletalMesh, EBulkType.Meshes),
                UCustomizableObject => (EAssetCategory.CustomizableObject, EBulkType.None),
                UNaniteDisplacedMesh => (EAssetCategory.NaniteDisplacedMesh, EBulkType.None),

                UTexture => (EAssetCategory.Texture, EBulkType.Textures),

                UMaterialInterface => (EAssetCategory.Material, EBulkType.None),
                UMaterialInterfaceEditorOnlyData => (EAssetCategory.MaterialEditorData, EBulkType.None),
                UMaterialFunction => (EAssetCategory.MaterialFunction, EBulkType.None),
                UMaterialFunctionEditorOnlyData => (EAssetCategory.MaterialFunctionEditorData, EBulkType.None),
                UMaterialParameterCollection => (EAssetCategory.MaterialParameterCollection, EBulkType.None),

                UAnimationAsset => (EAssetCategory.Animation, EBulkType.Animations),
                USkeleton => (EAssetCategory.Skeleton, EBulkType.Meshes),
                URig => (EAssetCategory.Rig, EBulkType.None),

                UWorld => (EAssetCategory.World, EBulkType.Meshes | EBulkType.Textures | EBulkType.Audio | EBulkType.Code),
                UMapBuildDataRegistry => (EAssetCategory.BuildData, EBulkType.Textures),
                ULevelSequence => (EAssetCategory.LevelSequence, EBulkType.Code),
                UFoliageType => (EAssetCategory.Foliage, EBulkType.None),

                UDataAsset or UDataTable or UCurveTable or UStringTable => (EAssetCategory.Data, EBulkType.None),
                UCurveBase => (EAssetCategory.CurveBase, EBulkType.None),
                UPhysicsAsset => (EAssetCategory.PhysicsAsset, EBulkType.None),
                UObjectRedirector => (EAssetCategory.ObjectRedirector, EBulkType.None),
                UPhysicalMaterial => (EAssetCategory.PhysicalMaterial, EBulkType.None),
                ULoudnessNRT or UConstantQNRT => (EAssetCategory.Data, EBulkType.None),

                USoundAtomCue or UAkAudioEvent or USoundCue or UFMODEvent
                    or UAkAssetData or UAkAssetPlatformData or UAtomCueSheet or USoundAtomCueSheet or UAtomWaveBank => (EAssetCategory.Audio, EBulkType.Audio),

                UFMODBank or UAkAudioBank or UAkInitBank => (EAssetCategory.SoundBank, EBulkType.Audio),

                UWwiseAssetLibrary or USoundBase or UAkMediaAssetData or UAtomCueSheet
                    or USoundAtomCueSheet or UAkAudioType or UExternalSource or UExternalSourceBank
                    or UAkMediaAsset => (EAssetCategory.Audio, EBulkType.Audio),

                UFileMediaSource => (EAssetCategory.Video, EBulkType.None),
                UFont or UFontFace => (EAssetCategory.Font, EBulkType.None),

                UNiagaraSystem or UNiagaraScriptBase or UParticleSystem => (EAssetCategory.Particle, EBulkType.None),

                _ => (EAssetCategory.All, EBulkType.None),
            };
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to detect asset type for {Name}", nameWithoutExtension);
            return (EAssetCategory.All, EBulkType.None);
        }
    }
}
