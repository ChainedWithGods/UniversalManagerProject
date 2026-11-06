using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using Newtonsoft.Json;
using PakViewer.ViewModels;
using Windows.System;

namespace PakViewer.Settings;

public class AppSettings
{
    private static readonly string AppDataPath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
        "PakViewer");

    public static readonly string SettingsFilePath = Path.Combine(AppDataPath, "settings.json");

    public static AppSettings Instance { get; set; } = new();

    public ObservableCollection<GameConfig> Games { get; set; } = new();

    public List<GameConfiguration>? GameConfigurations { get; set; }

    public string? LastSelectedGame { get; set; }
    public string OutputDirectory { get; set; } = string.Empty;

    public ObservableCollection<string> ModFolders { get; set; } = new() { "mods", "~mods", "logicmods" };

    public string Theme { get; set; } = "System";

    public bool SystemSoundsEnabled { get; set; } = true;

    public bool AutoLoadLastProfile { get; set; } = false;

    public ObservableCollection<ShortcutConfig> Shortcuts { get; set; } = new();

    public bool CheckForUpdatesOnStartup { get; set; } = true;

    public string UpdateManifestUrl { get; set; } = string.Empty;

    public string? IgnoredUpdateVersion { get; set; }

    public AppSettings()
    {
        if (string.IsNullOrEmpty(OutputDirectory))
        {
            OutputDirectory = Path.Combine(AppDataPath, "Output");
        }

        if (Shortcuts.Count == 0)
        {
            Shortcuts.Add(new ShortcutConfig { Id = "LoadGame", Name = "Load Game", Key = VirtualKey.L, Modifiers = VirtualKeyModifiers.Control });
            Shortcuts.Add(new ShortcutConfig { Id = "Search", Name = "Search", Key = VirtualKey.F, Modifiers = VirtualKeyModifiers.Control });
            Shortcuts.Add(new ShortcutConfig { Id = "Settings", Name = "Open Settings", Key = VirtualKey.I, Modifiers = VirtualKeyModifiers.Control });
        }
    }

    public static void Load()
    {
        try
        {
            Directory.CreateDirectory(AppDataPath);

            if (File.Exists(SettingsFilePath))
            {
                var json = File.ReadAllText(SettingsFilePath);
                Instance = JsonConvert.DeserializeObject<AppSettings>(json) ?? new AppSettings();
                Instance.Games ??= new ObservableCollection<GameConfig>();
                Instance.Shortcuts ??= new ObservableCollection<ShortcutConfig>();

                var uniqueShortcuts = Instance.Shortcuts
                    .GroupBy(s => s.Id)
                    .Select(g => g.First())
                    .ToList();

                if (uniqueShortcuts.Count != Instance.Shortcuts.Count)
                {
                    Instance.Shortcuts.Clear();
                    foreach (var s in uniqueShortcuts)
                    {
                        Instance.Shortcuts.Add(s);
                    }
                }

                if (Instance.Shortcuts.Count == 0)
                {
                    Instance.Shortcuts.Add(new ShortcutConfig { Id = "LoadGame", Name = "Load Game", Key = VirtualKey.L, Modifiers = VirtualKeyModifiers.Control });
                    Instance.Shortcuts.Add(new ShortcutConfig { Id = "Search", Name = "Search", Key = VirtualKey.F, Modifiers = VirtualKeyModifiers.Control });
                    Instance.Shortcuts.Add(new ShortcutConfig { Id = "Settings", Name = "Open Settings", Key = VirtualKey.I, Modifiers = VirtualKeyModifiers.Control });
                }

                if (Instance.GameConfigurations != null && Instance.GameConfigurations.Count > 0)
                {
                    foreach (var oldConfig in Instance.GameConfigurations)
                    {
                        if (!Instance.Games.Any(g => g.Name == oldConfig.Name))
                        {
                            Instance.Games.Add(new GameConfig
                            {
                                Name = oldConfig.Name,
                                PakPath = oldConfig.PakFilesDirectory,
                                UsmapPath = oldConfig.UsmapFile,
                                AesKey = oldConfig.AesKey ?? "",
                                UeVersion = oldConfig.UeVersion
                            });
                        }
                    }
                    Instance.GameConfigurations = null;
                }
            }
            else
            {
                Instance = new AppSettings();
                Save();
            }
        }
        catch
        {
            Instance = new AppSettings();
        }
    }

    public static void Save()
    {
        try
        {
            Directory.CreateDirectory(AppDataPath);
            var json = JsonConvert.SerializeObject(Instance, Formatting.Indented);
            File.WriteAllText(SettingsFilePath, json);
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"Failed to save settings: {ex.Message}");
        }
    }
}

public class ShortcutConfig : ViewModelBase
{
    public string Id { get; set; } = string.Empty;
    public string Name { get; set; } = string.Empty;

    private VirtualKey _key;
    public VirtualKey Key
    {
        get => _key;
        set
        {
            if (SetProperty(ref _key, value))
            {
                OnPropertyChanged(nameof(DisplayString));
            }
        }
    }

    private VirtualKeyModifiers _modifiers;
    public VirtualKeyModifiers Modifiers
    {
        get => _modifiers;
        set
        {
            if (SetProperty(ref _modifiers, value))
            {
                OnPropertyChanged(nameof(DisplayString));
            }
        }
    }

    public string DisplayString
    {
        get
        {
            var parts = new List<string>();
            if (Modifiers.HasFlag(VirtualKeyModifiers.Control)) parts.Add("Ctrl");
            if (Modifiers.HasFlag(VirtualKeyModifiers.Shift)) parts.Add("Shift");
            if (Modifiers.HasFlag(VirtualKeyModifiers.Menu)) parts.Add("Alt");
            if (Modifiers.HasFlag(VirtualKeyModifiers.Windows)) parts.Add("Win");

            parts.Add(Key.ToString());
            return string.Join(" + ", parts);
        }
    }
}

public class GameConfig : ViewModelBase
{
    private string _name = string.Empty;
    public string Name { get => _name; set => SetProperty(ref _name, value); }

    private string _pakPath = string.Empty;
    public string PakPath { get => _pakPath; set => SetProperty(ref _pakPath, value); }

    private string? _usmapPath;
    public string? UsmapPath { get => _usmapPath; set => SetProperty(ref _usmapPath, value); }

    private string _aesKey = string.Empty;
    public string AesKey { get => _aesKey; set => SetProperty(ref _aesKey, value); }

    private string _ueVersion = "UE 5.3";
    public string UeVersion { get => _ueVersion; set => SetProperty(ref _ueVersion, value); }

    public ObservableCollection<string> ModFolders { get; set; } = new() { "mods", "~mods", "logicmods" };

    public override string ToString() => Name;
}

public class GameConfiguration
{
    public string Name { get; set; } = string.Empty;
    public string PakFilesDirectory { get; set; } = string.Empty;
    public string? UsmapFile { get; set; }
    public string? AesKey { get; set; }
    public string UeVersion { get; set; } = "UE 5.3";

    public override string ToString() => Name;
}
