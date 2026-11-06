using System.Collections.ObjectModel;
using System.Windows.Input;
using CUE4Parse.UE4.Versions;
using PakViewer.Settings;

namespace PakViewer.ViewModels;

public class SettingsViewModel : ViewModelBase
{
    private GameConfig? _selectedGame;
    private int _selectedSectionIndex;
    private string _selectedTheme;
    private bool _systemSoundsEnabled;
    private bool _autoLoadLastProfile;
    private bool _checkForUpdatesOnStartup;
    private string _updateManifestUrl = string.Empty;

    public SettingsViewModel()
    {

        Games = AppSettings.Instance.Games;
        Shortcuts = AppSettings.Instance.Shortcuts;

        if (Games.Count > 0)
        {

            if (!string.IsNullOrEmpty(AppSettings.Instance.LastSelectedGame))
            {
                var lastGame = Games.FirstOrDefault(g =>
                    string.Equals(g.Name, AppSettings.Instance.LastSelectedGame, StringComparison.OrdinalIgnoreCase));
                SelectedGame = lastGame ?? Games[0];
            }
            else
            {
                SelectedGame = Games[0];
            }
        }
        else
        {

            var defaultGame = new GameConfig
            {
                Name = "New Game",
                PakPath = "",
                UsmapPath = "",
                AesKey = "",
                UeVersion = "UE5.3"
            };
            Games.Add(defaultGame);
            SelectedGame = defaultGame;
        }

        AddGameCommand = new RelayCommand(AddGame);
        RemoveGameCommand = new RelayCommand(RemoveGame, () => SelectedGame != null);

        _selectedSectionIndex = 0;
        _selectedTheme = AppSettings.Instance.Theme;
        _systemSoundsEnabled = AppSettings.Instance.SystemSoundsEnabled;
        _autoLoadLastProfile = AppSettings.Instance.AutoLoadLastProfile;
        _checkForUpdatesOnStartup = AppSettings.Instance.CheckForUpdatesOnStartup;
        _updateManifestUrl = AppSettings.Instance.UpdateManifestUrl;
    }

    public ObservableCollection<GameConfig> Games { get; }

    public ObservableCollection<ShortcutConfig> Shortcuts { get; }

    public List<string> UeVersions { get; } = BuildUeVersionList();

    private static List<string> BuildUeVersionList()
    {

        var common = new List<string>
        {
            "UE 4.0", "UE 4.1", "UE 4.2", "UE 4.3", "UE 4.4",
            "UE 4.5", "UE 4.6", "UE 4.7", "UE 4.8", "UE 4.9",
            "UE 4.10", "UE 4.11", "UE 4.12", "UE 4.13", "UE 4.14",
            "UE 4.15", "UE 4.16", "UE 4.17", "UE 4.18", "UE 4.19",
            "UE 4.20", "UE 4.21", "UE 4.22", "UE 4.23", "UE 4.24",
            "UE 4.25", "UE 4.26", "UE 4.27",
            "UE 5.0", "UE 5.1", "UE 5.2", "UE 5.3", "UE 5.4", "UE 5.5", "UE 5.6", "UE 5.7", "UE 5.8", "UE 5.9", "UE 6.0"
        };

        try
        {
            var gameEnums = Enum.GetValues<EGame>()
                .Where(v =>
                {

                    if (v == EGame.GAME_UE4_LATEST || v == EGame.GAME_UE5_LATEST)
                        return false;

                    var value = (uint)v;
                    var gameOffset = value & 0xFF;

                    if (gameOffset > 0)
                        return true;

                    var matchString = $"UE{(value >> 16) & 0xFF}.{(value >> 8) & 0xFF}";
                    return !common.Any(c => c.Replace(" ", "").Equals(matchString, StringComparison.OrdinalIgnoreCase));
                })
                .Select(v => v.ToString())
                .OrderBy(s => s, StringComparer.Ordinal);
            common.AddRange(gameEnums);
        }
        catch
        {

        }

        return common;
    }

    public GameConfig? SelectedGame
    {
        get => _selectedGame;
        set
        {
            if (SetProperty(ref _selectedGame, value))
            {

                AppSettings.Instance.LastSelectedGame = value?.Name;

            }
        }
    }

    public int SelectedSectionIndex
    {
        get => _selectedSectionIndex;
        set
        {
            if (SetProperty(ref _selectedSectionIndex, value))
            {
                OnPropertyChanged(nameof(IsGeneralSelected));
                OnPropertyChanged(nameof(IsProfilesSelected));
                OnPropertyChanged(nameof(IsShortcutsSelected));
            }
        }
    }

    public bool IsGeneralSelected => SelectedSectionIndex == 0;

    public bool IsProfilesSelected => SelectedSectionIndex == 1;

    public bool IsShortcutsSelected => SelectedSectionIndex == 2;

    public List<string> ThemeOptions { get; } = new()
    {
        "System",
        "Light",
        "Dark"
    };

    public string SelectedTheme
    {
        get => _selectedTheme;
        set => SetProperty(ref _selectedTheme, value);
    }

    public bool SystemSoundsEnabled
    {
        get => _systemSoundsEnabled;
        set => SetProperty(ref _systemSoundsEnabled, value);
    }

    public bool AutoLoadLastProfile
    {
        get => _autoLoadLastProfile;
        set => SetProperty(ref _autoLoadLastProfile, value);
    }

    public bool CheckForUpdatesOnStartup
    {
        get => _checkForUpdatesOnStartup;
        set => SetProperty(ref _checkForUpdatesOnStartup, value);
    }

    public string UpdateManifestUrl
    {
        get => _updateManifestUrl;
        set => SetProperty(ref _updateManifestUrl, value);
    }

    public ICommand AddGameCommand { get; }
    public ICommand RemoveGameCommand { get; }

    private void AddGame()
    {
        AddGame("New Game");
    }

    public void AddGame(string name)
    {
        var newGame = new GameConfig
        {
            Name = GetUniqueName(name),
            PakPath = "",
            UsmapPath = "",
            AesKey = "",
            UeVersion = "UE 5.3"
        };
        Games.Add(newGame);
        SelectedGame = newGame;
    }

    private void RemoveGame()
    {
        RemoveSelectedGame();
    }

    public void RemoveSelectedGame()
    {
        if (SelectedGame != null)
        {
            Games.Remove(SelectedGame);
            SelectedGame = Games.FirstOrDefault();
        }
    }

    private string GetUniqueName(string name)
    {
        var trimmedName = string.IsNullOrWhiteSpace(name) ? "New Game" : name.Trim();
        if (!Games.Any(g => string.Equals(g.Name, trimmedName, StringComparison.OrdinalIgnoreCase)))
        {
            return trimmedName;
        }

        var index = 2;
        string candidate;
        do
        {
            candidate = $"{trimmedName} ({index})";
            index++;
        } while (Games.Any(g => string.Equals(g.Name, candidate, StringComparison.OrdinalIgnoreCase)));

        return candidate;
    }

    public void SaveSettings()
    {
        AppSettings.Instance.Theme = SelectedTheme;
        AppSettings.Instance.SystemSoundsEnabled = SystemSoundsEnabled;
        AppSettings.Instance.AutoLoadLastProfile = AutoLoadLastProfile;
        AppSettings.Instance.CheckForUpdatesOnStartup = CheckForUpdatesOnStartup;
        AppSettings.Instance.UpdateManifestUrl = UpdateManifestUrl;
        AppSettings.Save();
    }
}
