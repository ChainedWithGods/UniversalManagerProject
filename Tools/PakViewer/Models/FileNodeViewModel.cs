using System.Collections.ObjectModel;
using System.IO;
using Microsoft.UI.Xaml.Media;
using PakViewer.ViewModels;

namespace PakViewer.Models;

public class FileNodeViewModel : ViewModelBase
{
         private bool _isExpanded;
         private bool _isSelected;
         private bool? _isSkeletalMesh;
         private bool? _isSkeleton;
         private bool? _isAnimation;
         private bool? _isPhysicsAsset;
         private bool? _isTexture;
         private bool? _isMaterial;
         private bool? _isAudioFile;
         private bool _isResolved;
         private ImageSource? _thumbnail;

         public bool IsResolved
         {
             get => _isResolved;
             set => SetProperty(ref _isResolved, value);
         }

         public FileNodeViewModel(string name, bool isDirectory, string fullPath, FileNodeViewModel? parent = null)
         {
                  Name = name;
                  IsDirectory = isDirectory;
                  FullPath = fullPath;
                  Parent = parent;
                  Children = new ObservableCollection<FileNodeViewModel>();
         }

         public string Name { get; }
         public bool IsDirectory { get; }
         public string FullPath { get; }
         public FileNodeViewModel? Parent { get; set; }
         public ObservableCollection<FileNodeViewModel> Children { get; }

         public bool IsIconVisible => !IsDirectory && _thumbnail == null && !HasCustomIcon;

         public string DisplayName => IsDirectory ? Name : Path.GetFileNameWithoutExtension(Name);

         public string DisplayCount => IsDirectory && FileCount > 0 ? FileCount.ToString() : "";

         public ImageSource? Thumbnail
         {
                  get => _thumbnail;
                  set
                  {
                      if (SetProperty(ref _thumbnail, value))
                      {
                          OnPropertyChanged(nameof(IsIconVisible));
                          OnPropertyChanged(nameof(HasCustomIcon));
                          OnPropertyChanged(nameof(IsIconVisibleForPreview));
                          OnPropertyChanged(nameof(HasCustomIconForPreview));
                      }
                  }
         }

         private long _size;
         public long Size
         {
                  get => _size;
                  set => SetProperty(ref _size, value);
         }

         public string Extension
         {
                  get
                  {
                           var ext = Path.GetExtension(FullPath);
                           if (string.IsNullOrEmpty(ext))
                           {
                                    ext = Path.GetExtension(Name);
                           }
                           return ext.TrimStart('.').ToLowerInvariant();
                  }
         }

         public string FormattedSize
         {
                  get
                  {
                           double len = Size;
                           string[] sizes = { "B", "KB", "MB", "GB", "TB" };
                           int order = 0;
                           while (len >= 1024 && order < sizes.Length - 1)
                           {
                                    order++;
                                    len = len / 1024;
                           }
                           return $"{len:0.##} {sizes[order]}";
                  }
         }

         private int _fileCount;
         public int FileCount
         {
                  get => _fileCount;
                  set
                  {
                           if (SetProperty(ref _fileCount, value))
                           {
                                    OnPropertyChanged(nameof(DisplayCount));
                           }
                  }
         }

         private string _sourceArchive = "";
         public string SourceArchive
         {
                  get => _sourceArchive;
                  set => SetProperty(ref _sourceArchive, value);
         }

         public bool HasSourceArchive => !string.IsNullOrEmpty(_sourceArchive);

         private bool _isModFolder;
         public bool IsModFolder
         {
                  get => _isModFolder;
                  set => SetProperty(ref _isModFolder, value);
         }

         private long _offset;
         public long Offset
         {
                  get => _offset;
                  set => SetProperty(ref _offset, value);
         }

         public string FormattedOffset => $"0x{Offset:X}";

         private string _compressionMethod = "None";
         public string CompressionMethod
         {
                  get => _compressionMethod;
                  set => SetProperty(ref _compressionMethod, value);
         }

         private bool _isEncrypted;
         public bool IsEncrypted
         {
                  get => _isEncrypted;
                  set => SetProperty(ref _isEncrypted, value);
         }

         private string _includedInArchive = "";
         public string IncludedInArchive
         {
                  get => _includedInArchive;
                  set => SetProperty(ref _includedInArchive, value);
         }

        public bool IsSkeleton
        {
                 get
                 {
                          if (IsDirectory) return false;

                          if (_isSkeleton.HasValue)
                                   return _isSkeleton.Value;

                          var fileName = Name;
                          if (fileName.Contains("Skeleton", StringComparison.OrdinalIgnoreCase) ||
                              fileName.Contains("_Skeleton", StringComparison.OrdinalIgnoreCase))
                              return true;

                          return false;
                 }
                 set
                 {
                          _isSkeleton = value;
                          OnPropertyChanged(nameof(IsSkeleton));
                          OnPropertyChanged(nameof(IconPath));
                          OnPropertyChanged(nameof(CustomIconSource));
                          OnPropertyChanged(nameof(CustomIconUri));
                          OnPropertyChanged(nameof(CustomIconSize));
                          OnPropertyChanged(nameof(HasCustomIcon));
                         OnPropertyChanged(nameof(CustomIconSourceForPreview));
                         OnPropertyChanged(nameof(CustomIconUriForPreview));
                         OnPropertyChanged(nameof(CustomIconSizeForPreview));
                         OnPropertyChanged(nameof(HasCustomIconForPreview));
                         OnPropertyChanged(nameof(IsIconVisibleForPreview));
                          OnPropertyChanged(nameof(FileIcon));
                          OnPropertyChanged(nameof(IsIconVisible));
                 }
        }

        public bool IsAnimation
        {
                 get
                 {
                          if (IsDirectory) return false;

                          if (_isAnimation.HasValue)
                                   return _isAnimation.Value;

                          var fileName = Name;
                          if (fileName.Contains("Anim", StringComparison.OrdinalIgnoreCase) ||
                              fileName.Contains("Animation", StringComparison.OrdinalIgnoreCase) ||
                              fileName.StartsWith("A_", StringComparison.OrdinalIgnoreCase) ||
                              fileName.StartsWith("Anim_", StringComparison.OrdinalIgnoreCase))
                              return true;

                          return false;
                 }
                 set
                 {
                          _isAnimation = value;
                          OnPropertyChanged(nameof(IsAnimation));
                          OnPropertyChanged(nameof(IconPath));
                          OnPropertyChanged(nameof(CustomIconSource));
                          OnPropertyChanged(nameof(CustomIconUri));
                          OnPropertyChanged(nameof(CustomIconSize));
                          OnPropertyChanged(nameof(HasCustomIcon));
                         OnPropertyChanged(nameof(CustomIconSourceForPreview));
                         OnPropertyChanged(nameof(CustomIconUriForPreview));
                         OnPropertyChanged(nameof(CustomIconSizeForPreview));
                         OnPropertyChanged(nameof(HasCustomIconForPreview));
                         OnPropertyChanged(nameof(IsIconVisibleForPreview));
                          OnPropertyChanged(nameof(FileIcon));
                          OnPropertyChanged(nameof(IsIconVisible));
                 }
        }

        public bool IsSkeletalMesh
        {
                 get
                 {
                          if (IsDirectory) return false;

                          if (_isTexture == true)
                                   return false;

                          if (!string.IsNullOrEmpty(FullPath) &&
                              (FullPath.IndexOf("\\Textures\\", StringComparison.OrdinalIgnoreCase) >= 0 ||
                               FullPath.IndexOf("/Textures/", StringComparison.OrdinalIgnoreCase) >= 0))
                                   return false;

                          if (_isSkeletalMesh.HasValue)
                                   return _isSkeletalMesh.Value;

                          var fileName = Name;
                          if (fileName.StartsWith("SK_", StringComparison.OrdinalIgnoreCase) ||
                              fileName.StartsWith("SKM_", StringComparison.OrdinalIgnoreCase) ||
                              fileName.StartsWith("SM_", StringComparison.OrdinalIgnoreCase) ||
                              fileName.Contains("SkeletalMesh", StringComparison.OrdinalIgnoreCase) ||
                              fileName.Contains("StaticMesh", StringComparison.OrdinalIgnoreCase))
                              return true;

                          return false;
                 }
                 set
                 {
                          _isSkeletalMesh = value;
                          OnPropertyChanged(nameof(IsSkeletalMesh));
                          OnPropertyChanged(nameof(IconPath));
                          OnPropertyChanged(nameof(CustomIconSource));
                          OnPropertyChanged(nameof(CustomIconUri));
                          OnPropertyChanged(nameof(CustomIconSize));
                          OnPropertyChanged(nameof(HasCustomIcon));
                         OnPropertyChanged(nameof(CustomIconSourceForPreview));
                         OnPropertyChanged(nameof(CustomIconUriForPreview));
                         OnPropertyChanged(nameof(CustomIconSizeForPreview));
                         OnPropertyChanged(nameof(HasCustomIconForPreview));
                         OnPropertyChanged(nameof(IsIconVisibleForPreview));
                          OnPropertyChanged(nameof(FileIcon));
                          OnPropertyChanged(nameof(IsIconVisible));
                 }
        }

         public bool IsPhysicsAsset
         {
                  get
                  {
                           if (IsDirectory) return false;
                           if (_isPhysicsAsset.HasValue)
                           {
                                    return _isPhysicsAsset.Value;
                           }

                           var fileName = Name;
                           if (fileName.StartsWith("PHYS_", StringComparison.OrdinalIgnoreCase) ||
                               fileName.Contains("PhysicsAsset", StringComparison.OrdinalIgnoreCase) ||
                               fileName.Contains("_Physics", StringComparison.OrdinalIgnoreCase))
                               return true;

                           return false;
                  }
                  set
                  {
                           _isPhysicsAsset = value;
                           OnPropertyChanged(nameof(IsPhysicsAsset));
                           OnPropertyChanged(nameof(IconPath));
                           OnPropertyChanged(nameof(CustomIconSource));
                           OnPropertyChanged(nameof(CustomIconUri));
                           OnPropertyChanged(nameof(CustomIconSize));
                           OnPropertyChanged(nameof(HasCustomIcon));
                          OnPropertyChanged(nameof(CustomIconSourceForPreview));
                          OnPropertyChanged(nameof(CustomIconUriForPreview));
                          OnPropertyChanged(nameof(CustomIconSizeForPreview));
                          OnPropertyChanged(nameof(HasCustomIconForPreview));
                          OnPropertyChanged(nameof(IsIconVisibleForPreview));
                           OnPropertyChanged(nameof(FileIcon));
                           OnPropertyChanged(nameof(IsIconVisible));
                  }
         }

        private static readonly HashSet<string> AudioExtensions = new(StringComparer.OrdinalIgnoreCase)
        {
            "wav", "ogg", "flac", "opus", "mp3", "mid", "wem", "wems",
            "xvag", "at9", "mp4", "bnk", "pck", "bank", "acb", "awb", "hca", "adx"
        };

        public bool IsAudioFile
        {
            get => !IsDirectory && (_isAudioFile == true || AudioExtensions.Contains(Extension));
            set
            {
                if (SetProperty(ref _isAudioFile, value))
                {
                    OnPropertyChanged(nameof(IconPath));
                    OnPropertyChanged(nameof(CustomIconSource));
                    OnPropertyChanged(nameof(CustomIconUri));
                    OnPropertyChanged(nameof(CustomIconSize));
                    OnPropertyChanged(nameof(HasCustomIcon));
                    OnPropertyChanged(nameof(CustomIconSourceForPreview));
                    OnPropertyChanged(nameof(CustomIconUriForPreview));
                    OnPropertyChanged(nameof(CustomIconSizeForPreview));
                    OnPropertyChanged(nameof(HasCustomIconForPreview));
                    OnPropertyChanged(nameof(IsIconVisibleForPreview));
                    OnPropertyChanged(nameof(IsIconVisible));
                    OnPropertyChanged(nameof(FileIcon));
                    OnPropertyChanged(nameof(IsTexture));
                }
            }
        }

        public double TreeIconSize => IsDirectory ? 32 : 20;

         public bool IsTexture
         {
                  get
                  {
                           if (IsDirectory) return false;
                           if (Extension != "uasset" && Extension != "ubulk")
                                    return false;

                           if (_isAudioFile == true)
                                    return false;

                           if (_isAnimation == true)
                                    return false;

                           if (_isTexture.HasValue)
                                    return _isTexture.Value;

                           var fileName = Name;
                           if (Path.GetExtension(fileName).Length == 0 && !string.IsNullOrEmpty(FullPath))
                           {
                                    fileName = Path.GetFileName(FullPath);
                           }

                           if (fileName.StartsWith("SK_", StringComparison.OrdinalIgnoreCase) ||
                               fileName.StartsWith("SM_", StringComparison.OrdinalIgnoreCase) ||
                               fileName.StartsWith("PHYS_", StringComparison.OrdinalIgnoreCase) ||
                               fileName.StartsWith("MI_", StringComparison.OrdinalIgnoreCase) ||
                               fileName.StartsWith("M_", StringComparison.OrdinalIgnoreCase) ||
                               fileName.StartsWith("A_", StringComparison.OrdinalIgnoreCase) ||
                               fileName.StartsWith("Anim_", StringComparison.OrdinalIgnoreCase) ||
                               fileName.Contains("Skeleton", StringComparison.OrdinalIgnoreCase) ||
                               fileName.Contains("PhysicsAsset", StringComparison.OrdinalIgnoreCase) ||
                               fileName.Contains("Anim", StringComparison.OrdinalIgnoreCase))
                           {
                               return false;
                           }

                           return fileName.StartsWith("T_", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.Contains("Texture", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.Contains("BaseColor", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.Contains("Metallic", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.Contains("Roughness", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.Contains("Normal", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.Contains("Emissive", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.Contains("AO", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.Contains("Occlusion", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.Contains("Albedo", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.Contains("Diffuse", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.Contains("Specular", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.Contains("Opacity", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.Contains("Height", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.Contains("Displacement", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.EndsWith("_albm.uasset", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.EndsWith("_nrml.uasset", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.EndsWith("_spec.uasset", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.EndsWith("_emis.uasset", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.EndsWith("_rough.uasset", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.EndsWith("_ao.uasset", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.EndsWith("_mask.uasset", StringComparison.OrdinalIgnoreCase) ||

                                  fileName.EndsWith("_A.uasset", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.EndsWith("_N.uasset", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.EndsWith("_M.uasset", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.EndsWith("_R.uasset", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.EndsWith("_ORM.uasset", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.EndsWith("_D.uasset", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.EndsWith("_S.uasset", StringComparison.OrdinalIgnoreCase) ||
                                  fileName.EndsWith("_E.uasset", StringComparison.OrdinalIgnoreCase);
            }
            set
                  {
                      _isTexture = value;
                      OnPropertyChanged(nameof(IsTexture));
                      OnPropertyChanged(nameof(IconPath));
                      OnPropertyChanged(nameof(CustomIconSource));
                      OnPropertyChanged(nameof(CustomIconUri));
                      OnPropertyChanged(nameof(CustomIconSize));
                      OnPropertyChanged(nameof(HasCustomIcon));
                      OnPropertyChanged(nameof(CustomIconSourceForPreview));
                      OnPropertyChanged(nameof(CustomIconUriForPreview));
                      OnPropertyChanged(nameof(CustomIconSizeForPreview));
                      OnPropertyChanged(nameof(HasCustomIconForPreview));
                      OnPropertyChanged(nameof(IsIconVisibleForPreview));
                      OnPropertyChanged(nameof(FileIcon));
                      OnPropertyChanged(nameof(IsIconVisible));
                  }
         }

         public bool IsMaterial
         {
                  get
                  {
                           if (IsDirectory) return false;
                           if (Extension != "uasset")
                                    return false;

                           if (_isMaterial.HasValue)
                                    return _isMaterial.Value;

                           if (IsTexture) return false;

                           var fileName = Name;
                           if (Path.GetExtension(fileName).Length == 0 && !string.IsNullOrEmpty(FullPath))
                           {
                                    fileName = Path.GetFileName(FullPath);
                           }

                           if (fileName.StartsWith("MI_", StringComparison.OrdinalIgnoreCase) ||
                               fileName.StartsWith("M_", StringComparison.OrdinalIgnoreCase) ||
                               fileName.Contains("Material", StringComparison.OrdinalIgnoreCase) ||
                               fileName.EndsWith("_Inst.uasset", StringComparison.OrdinalIgnoreCase))
                                    return true;

                           return false;
                  }
                  set
                  {
                      _isMaterial = value;
                      OnPropertyChanged(nameof(IsMaterial));
                      OnPropertyChanged(nameof(IconPath));
                      OnPropertyChanged(nameof(CustomIconSource));
                      OnPropertyChanged(nameof(CustomIconUri));
                      OnPropertyChanged(nameof(CustomIconSize));
                      OnPropertyChanged(nameof(HasCustomIcon));
                      OnPropertyChanged(nameof(CustomIconSourceForPreview));
                      OnPropertyChanged(nameof(CustomIconUriForPreview));
                      OnPropertyChanged(nameof(CustomIconSizeForPreview));
                      OnPropertyChanged(nameof(HasCustomIconForPreview));
                      OnPropertyChanged(nameof(IsIconVisibleForPreview));
                      OnPropertyChanged(nameof(FileIcon));
                      OnPropertyChanged(nameof(IsIconVisible));
                  }
         }

         public string? IconPath
         {
                  get
                  {
                           if (IsDirectory) return null;

                           if (IsAnimation)
                                    return "pack://application:,,,/Resources/Icons/Animations.ico";

                           if (IsPhysicsAsset)
                                    return "pack://application:,,,/Resources/Icons/person.ico";

                           if (IsSkeletalMesh)
                    return "pack://application:,,,/Resources/Icons/human-bones.ico";

                           if (IsTexture)
                                    return "pack://application:,,,/Resources/Icons/2D Textures.ico";

                           if (IsMaterial)
                                    return "pack://application:,,,/Resources/Icons/materials.ico";

                          return null;
                 }
        }

        public string? CustomIconSource
        {
            get
            {
                if (!IsDirectory)
                {
                    if (IsPhysicsAsset)
                    {
                        return "ms-appx:///Icons/Physics.ico";
                    }

                    if (IsSkeleton)
                    {
                        return "ms-appx:///Icons/StaticSK.ico";
                    }

                    if (IsAnimation)
                    {
                        return "ms-appx:///Icons/Animations.ico";
                    }

                    if (IsSkeletalMesh)
                    {
                        return "ms-appx:///Icons/Skeleton.ico";
                    }

                    if (IsAudioFile || Extension is "wem" or "wems" or "bnk" or "pck" or "bank" or "acb" or "awb" or
                        "wav" or "ogg" or "flac" or "xvag" or "at9" or "mp4" or "opus" or "mp3" or "hca" or "adx" or "mid")
                    {
                        return "ms-appx:///Icons/MSounds.ico";
                    }

                    if (IsTexture)
                    {
                        return "ms-appx:///Icons/STextures.ico";
                    }

                    if (IsMaterial)
                    {
                        return "ms-appx:///Icons/Materials.ico";
                    }

                    return null;
                }

                if (IsDirectory)
                {
                    if (string.Equals(Name, "Character", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Characters", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "characters", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "character", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Character.ico";
                    }

                    if (string.Equals(Name, "cloth", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "cloths", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "clothes", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "mesh", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "meshes", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Cloth.ico";
                    }

                    if (string.Equals(Name, "NPC", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "NPCS", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Npcs", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Npc", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Npc.ico";
                    }

                    if (string.Equals(Name, "Sounds", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Sound", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "sounds", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "sound", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Music", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Musics", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "music", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "musics", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Audio", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Audios", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "audio", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "audios", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/music.ico";
                    }

                    if (string.Equals(Name, "Localization", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Localizations", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Localization.ico";
                    }

                    if (string.Equals(Name, "Guns", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Gun", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "guns", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "gun", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Weapons", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Weapon", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Gun.ico";
                    }

                    if (string.Equals(Name, "Content", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Contents", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Content.ico";
                    }

                    if (string.Equals(Name, "Config", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Configs", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Configuration", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "config", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "configs", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "configuration", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Config.ico";
                    }

                    if (string.Equals(Name, "Engine", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Engines", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "engine", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "engines", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Engine.ico";
                    }

                    if (string.Equals(Name, "Skin", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Skins", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "skin", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "skins", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Skin.ico";
                    }

                    if (string.Equals(Name, "Project", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Projects", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "project", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "projects", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Project.ico";
                    }

                    if (string.Equals(Name, "Plugins", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Plugin", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "plugins", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "plugin", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Plugins.ico";
                    }

                    if (string.Equals(Name, "Textures", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Texture", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "texture", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "textures", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Tex", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Texs", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "tex", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "texs", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Textures.ico";
                    }

                    if (string.Equals(Name, "Material", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Materials", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "material", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "materials", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Mat", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Mats", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "mat", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "mats", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Material.ico";
                    }

                    if (string.Equals(Name, "Makeup", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Makeups", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Makeup.ico";
                    }

                    if (string.Equals(Name, "Platform", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Platforms", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Platform.ico";
                    }

                    if (string.Equals(Name, "Blueprint", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Blueprints", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Blueprint.ico";
                    }

                    if (string.Equals(Name, "Model", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Models", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "model", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "models", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Model.ico";
                    }

                    if (string.Equals(Name, "Items", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Item", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "item", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "items", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Item.ico";
                    }

                    if (string.Equals(Name, "unisex", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/unisex.ico";
                    }

                    if (string.Equals(Name, "Male", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Males", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "male", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "males", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Male.ico";
                    }

                    if (string.Equals(Name, "Female", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Females", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "female", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "females", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Female.ico";
                    }

                    if (string.Equals(Name, "Accessories", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Accessory", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "accessories", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "accessory", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "accessorie", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Accessorie", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/Accessories.ico";
                    }

                    if (string.Equals(Name, "UI", StringComparison.OrdinalIgnoreCase))
                    {
                        return "ms-appx:///Icons/UI.ico";
                    }

                    return "ms-appx:///Icons/Folder.ico";
                }

                return null;
            }
        }

        public System.Uri? CustomIconUri => !string.IsNullOrEmpty(CustomIconSource) ? new System.Uri(CustomIconSource) : null;

        public double CustomIconSize
        {
            get
            {
                var source = CustomIconSource;
                if (string.IsNullOrEmpty(source)) return 0;

                if (IsDirectory)
                {
                    return 22;
                }

                return 20;
            }
        }

        public bool HasCustomIcon => !string.IsNullOrEmpty(CustomIconSource);

        public bool IsIconVisibleForPreview => !IsDirectory && _thumbnail == null && !HasCustomIconForPreview;

        public string? CustomIconSourceForPreview
        {
            get
            {
                if (IsAudioFile)
                {
                    return CustomIconSource;
                }

                return IsTexture ? null : CustomIconSource;
            }
        }

        public System.Uri? CustomIconUriForPreview => !string.IsNullOrEmpty(CustomIconSourceForPreview) ? new System.Uri(CustomIconSourceForPreview) : null;

        public double CustomIconSizeForPreview
        {
            get
            {
                var source = CustomIconSourceForPreview;
                if (string.IsNullOrEmpty(source)) return 0;

                if (IsDirectory)
                {
                    return 22;
                }

                return 20;
            }
        }

        public bool HasCustomIconForPreview => !string.IsNullOrEmpty(CustomIconSourceForPreview) && _thumbnail == null;

        public bool IsExpanded
         {
                  get => _isExpanded;
                  set => SetProperty(ref _isExpanded, value);
         }

         public bool IsSelected
         {
                  get => _isSelected;
                  set => SetProperty(ref _isSelected, value);
         }

         public string FileIcon
        {
            get
            {
                if (IsDirectory)
                {
                    if (string.Equals(Name, "Textures", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Tex", StringComparison.OrdinalIgnoreCase))
                    {
                        return "\uE8B9";
                    }

                    if (string.Equals(Name, "Materials", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Material", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Mats", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "Mat", StringComparison.OrdinalIgnoreCase))
                    {
                        return "\uE790";
                    }

                    if (string.Equals(Name, "cloth", StringComparison.OrdinalIgnoreCase) ||
                       string.Equals(Name, "cloths", StringComparison.OrdinalIgnoreCase) ||
                       string.Equals(Name, "clothes", StringComparison.OrdinalIgnoreCase) ||
                       string.Equals(Name, "mesh", StringComparison.OrdinalIgnoreCase) ||
                       string.Equals(Name, "meshes", StringComparison.OrdinalIgnoreCase))
                   {
                       return "\uF158";
                   }

                    return "\uE8B7";
                }

                if (IsAnimation) return "\uE8B2";
                if (IsAudioFile) return "\uEA8A";
                if (IsTexture) return "\uE91B";
                if (IsMaterial) return "\uE790";
                if (IsSkeletalMesh) return "\uF158";
                if (IsPhysicsAsset) return "\uE706";

                return GetFileIconGlyph(Extension);
            }
        }

         public string FolderColor
         {
             get
             {
                 if (IsDirectory)
                 {
                     if (string.Equals(Name, "cloth", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "cloths", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "clothes", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "mesh", StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(Name, "meshes", StringComparison.OrdinalIgnoreCase))
                    {
                        return "#E91E63";
                    }
                 }
                 return "#1E88E5";
             }
         }

         private static string GetFileIconGlyph(string extension)
         {
                  return extension.ToLower() switch
                  {

                           "uasset" => "\uEA86",
                           "umap" => "\uE909",
                           "uexp" => "\uE7B8",
                           "ubulk" => "\uE7B8",
                           "utoc" => "\uE7B8",
                           "pak" => "\uE7B8",

                           "ini" => "\uE70F",
                           "cfg" => "\uE70F",
                           "json" => "\uE943",
                           "json5" => "\uE943",
                           "xml" => "\uE943",
                           "yml" => "\uE943",
                           "csv" => "\uE943",
                           "tsv" => "\uE943",
                           "txt" => "\uE8A5",
                           "log" => "\uE8A5",
                           "md" => "\uE8A5",
                           "dat" => "\uE8A5",
                           "po" => "\uE8A5",
                           "bat" => "\uE943",
                           "vdf" => "\uE943",
                           "scss" => "\uE943",
                           "glslfx" => "\uE943",
                           "verse" => "\uE943",
                           "pem" => "\uE72E",
                           "gitignore" => "\uE72E",

                           "lua" => "\uE943",
                           "luac" => "\uE943",
                           "cpp" => "\uE943",
                           "h" => "\uE943",
                           "js" => "\uE943",
                           "css" => "\uE943",
                           "html" => "\uE943",

                           "uproject" => "\uE8A5",
                           "uefnproject" => "\uE8A5",
                           "uplugin" => "\uE8A5",
                           "upluginmanifest" => "\uE8A5",
                           "uparam" => "\uE70F",
                           "code-workspace" => "\uE8A5",
                           "projectstore" => "\uE8A5",
                           "manifest" => "\uE8A5",
                           "archive" => "\uE7B8",

                           "locres" => "\uE7C1",
                           "locmeta" => "\uE7C1",

                           "wav" => "\uEA8A",
                           "ogg" => "\uEA8A",
                           "flac" => "\uEA8A",
                           "wem" => "\uEA8A",
                           "xvag" => "\uEA8A",
                           "at9" => "\uEA8A",
                           "mp4" => "\uEA8A",
                           "bnk" => "\uEA8A",
                           "pck" => "\uEA8A",
                           "bank" => "\uEA8A",
                           "acb" => "\uEA8A",
                           "awb" => "\uEA8A",
                           "opus" => "\uEA8A",
                           "mp3" => "\uEA8A",
                           "hca" => "\uEA8A",
                           "adx" => "\uEA8A",
                           "mid" => "\uEA8A",

                           "png" => "\uE91B",
                           "jpg" => "\uE91B",
                           "jpeg" => "\uE91B",
                           "bmp" => "\uE91B",
                           "svg" => "\uE91B",
                           "dds" => "\uE91B",
                           "tga" => "\uE91B",

                           "ufont" => "\uEDBA",
                           "otf" => "\uEDBA",
                           "ttf" => "\uEDBA",

                           "ushaderbytecode" => "\uE943",
                           "ushadercode" => "\uE943",
                           "upipelinecache" => "\uE943",
                           "stinfo" => "\uE943",

                           "bin" => "\uE7B8",
                           "udic" => "\uE7B8",
                           "res" => "\uE7B8",

                           "cube" => "\uE91B",
                           "usda" => "\uE8A5",
                           "ocio" => "\uE70F",
                           "cptake" => "\uE8A5",
                           "vmodule" => "\uE8A5",
                           "spi1d" => "\uE8A5",
                           "apx" => "\uE8A5",
                           "udn" => "\uE8A5",
                           "doc" => "\uE8A5",
                           "tpl" => "\uE8A5",
                           "dbc" => "\uE7B8",

                           _ => "\uE8B7"
                  };
         }
}
