using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace PakViewer.Models;

public class AssetCategory : INotifyPropertyChanged
{
    public event PropertyChangedEventHandler? PropertyChanged;

    private void OnPropertyChanged([CallerMemberName] string? propertyName = null)
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }

    private string _name = string.Empty;
    private string _icon = string.Empty;
    private int _count;

    public string Name
    {
        get => _name;
        set
        {
            if (_name != value)
            {
                _name = value;
                OnPropertyChanged();
            }
        }
    }

    public string Icon
    {
        get => _icon;
        set
        {
            if (_icon != value)
            {
                _icon = value;
                OnPropertyChanged();
                OnPropertyChanged(nameof(IconUri));
            }
        }
    }

    public System.Uri? IconUri => !string.IsNullOrEmpty(Icon) ? new System.Uri(Icon) : null;

    public int Count
    {
        get => _count;
        set
        {
            if (_count != value)
            {
                _count = value;
                OnPropertyChanged();
                OnPropertyChanged(nameof(DisplayCount));
            }
        }
    }

    public string DisplayCount => $"{Count} {(Count == 1 ? "item" : "items")}";

    public ObservableCollection<FileNodeViewModel> Assets { get; } = new();

    public AssetCategory(string name, string icon)
    {
        Name = name;
        Icon = icon;
    }
}
