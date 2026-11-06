using System.ComponentModel;

namespace PakViewer.ViewModels;

public enum LoadingMode
{
    [Description("Multiple")]
    Multiple,
    [Description("All")]
    All,

}

public class LoadingModeViewModel : ViewModelBase
{
    private LoadingMode _selectedMode = LoadingMode.Multiple;

    public LoadingMode[] Modes { get; } = (LoadingMode[])Enum.GetValues(typeof(LoadingMode));

    public LoadingMode SelectedMode
    {
        get => _selectedMode;
        set => SetProperty(ref _selectedMode, value);
    }
}
