using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using PakViewer.ViewModels;
using Windows.Storage.Pickers;
using System;
using System.Linq;
using WinRT.Interop;
using PakViewer.Settings;
using PakViewer.Services;
using Windows.System;

namespace PakViewer.Views;

public sealed partial class SettingsPanel : UserControl
{
    public SettingsViewModel ViewModel { get; }

    private readonly bool _isMinimalMode;

    public event EventHandler? CloseRequested;

    public SettingsPanel() : this(false)
    {
    }

    public SettingsPanel(bool isMinimalMode)
    {
        this.InitializeComponent();
        _isMinimalMode = isMinimalMode;
        ViewModel = new SettingsViewModel();
        DataContext = ViewModel;

        ElementSoundPlayer.State = AppSettings.Instance.SystemSoundsEnabled ? ElementSoundPlayerState.On : ElementSoundPlayerState.Off;
        UpdateThemePreviewSelection(ViewModel.SelectedTheme);

        if (_isMinimalMode)
        {

            ProfilesTabButton.Visibility = Visibility.Collapsed;
            ShortcutsTabButton.Visibility = Visibility.Collapsed;
            StartupBehaviorGrid.Visibility = Visibility.Collapsed;
        }

        UpdateTabButtonStyles();
    }

    public void FocusInitialElement()
    {
        GeneralTabButton.Focus(FocusState.Programmatic);
    }

    private void GeneralTab_Click(object sender, RoutedEventArgs e)
    {
        ViewModel.SelectedSectionIndex = 0;
        UpdateTabButtonStyles();
    }

    private void ProfilesTab_Click(object sender, RoutedEventArgs e)
    {
        ViewModel.SelectedSectionIndex = 1;
        UpdateTabButtonStyles();
    }

    private void ShortcutsTab_Click(object sender, RoutedEventArgs e)
    {
        ViewModel.SelectedSectionIndex = 2;
        UpdateTabButtonStyles();
    }

    private void UpdateTabButtonStyles()
    {

        GeneralTabButton.Style = ViewModel.SelectedSectionIndex == 0
            ? (Style)Application.Current.Resources["AccentButtonStyle"]
            : null;

        ProfilesTabButton.Style = ViewModel.SelectedSectionIndex == 1
            ? (Style)Application.Current.Resources["AccentButtonStyle"]
            : null;

        ShortcutsTabButton.Style = ViewModel.SelectedSectionIndex == 2
            ? (Style)Application.Current.Resources["AccentButtonStyle"]
            : null;
    }

    private async void BrowsePakFolder_Click(object sender, RoutedEventArgs e)
    {
        var folderPicker = new FolderPicker();
        folderPicker.SuggestedStartLocation = PickerLocationId.ComputerFolder;
        folderPicker.FileTypeFilter.Add("*");

        var hwnd = GetWindowHandleForCurrentXamlRoot();
        if (hwnd != IntPtr.Zero)
        {
            InitializeWithWindow.Initialize(folderPicker, hwnd);
        }

        var folder = await folderPicker.PickSingleFolderAsync();
        if (folder != null && ViewModel.SelectedGame != null)
        {
            ViewModel.SelectedGame.PakPath = folder.Path;
        }
    }

    private async void BrowseUsmapFile_Click(object sender, RoutedEventArgs e)
    {
        var filePicker = new FileOpenPicker();
        filePicker.SuggestedStartLocation = PickerLocationId.ComputerFolder;
        filePicker.FileTypeFilter.Add(".usmap");
        filePicker.FileTypeFilter.Add("*");

        var hwnd = GetWindowHandleForCurrentXamlRoot();
        if (hwnd != IntPtr.Zero)
        {
            InitializeWithWindow.Initialize(filePicker, hwnd);
        }

        var file = await filePicker.PickSingleFileAsync();
        if (file != null && ViewModel.SelectedGame != null)
        {
            ViewModel.SelectedGame.UsmapPath = file.Path;
        }
    }

    private void SaveButton_Click(object sender, RoutedEventArgs e)
    {

        SyncUeVersionFromCombo();
        ViewModel.SaveSettings();

        if (App.Current is App app && app.MainWindowInstance is MainWindow mainWindow)
        {
            mainWindow.ViewModel.SyncSelectedGameFromSettings();
        }

        CloseRequested?.Invoke(this, EventArgs.Empty);
    }

    private void UeVersionComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        SyncUeVersionFromCombo();
    }

    private void UeVersionComboBox_LostFocus(object sender, RoutedEventArgs e)
    {
        SyncUeVersionFromCombo();
    }

    private void UeVersionComboBox_TextSubmitted(ComboBox sender, ComboBoxTextSubmittedEventArgs args)
    {
        SyncUeVersionFromCombo();
    }

    private void UeVersionComboBox_Loaded(object sender, RoutedEventArgs e)
    {

        ApplySavedUeVersionToCombo();

        DispatcherQueue.TryEnqueue(Microsoft.UI.Dispatching.DispatcherQueuePriority.Low, () =>
        {
            ApplySavedUeVersionToCombo();
        });
    }

    private void GameSelectorComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {

        ApplySavedUeVersionToCombo();

        DispatcherQueue.TryEnqueue(Microsoft.UI.Dispatching.DispatcherQueuePriority.Low, () =>
        {
            ApplySavedUeVersionToCombo();
        });
    }

    private void ApplySavedUeVersionToCombo()
    {
        if (ViewModel.SelectedGame == null) return;
        var saved = ViewModel.SelectedGame.UeVersion;
        if (string.IsNullOrEmpty(saved)) return;

        var matchedItem = ViewModel.UeVersions.FirstOrDefault(v =>
            string.Equals(v, saved, StringComparison.OrdinalIgnoreCase));

        if (matchedItem == null && saved.StartsWith("UE", StringComparison.OrdinalIgnoreCase) && saved.Length > 2)
        {
            var next = saved[2];
            if (next == 'G' || next == '_' || char.IsDigit(next) || next == '.' || next == ' ')
            {
                var withoutUE = saved.Substring(2).Trim();
                matchedItem = ViewModel.UeVersions.FirstOrDefault(v =>
                    string.Equals(v, withoutUE, StringComparison.OrdinalIgnoreCase));
            }
        }

        if (matchedItem == null)
        {
            matchedItem = ViewModel.UeVersions.FirstOrDefault(v =>
                string.Equals(v.Replace(" ", ""), saved.Replace(" ", ""), StringComparison.OrdinalIgnoreCase));
        }

        if (matchedItem != null)
        {

            UeVersionComboBox.SelectedItem = matchedItem;
            UeVersionComboBox.Text = matchedItem;
        }
        else
        {

            UeVersionComboBox.SelectedItem = null;
            UeVersionComboBox.Text = saved;
        }
    }

    private void SyncUeVersionFromCombo()
    {
        if (ViewModel.SelectedGame == null) return;

        var value = UeVersionComboBox.SelectedItem as string
                    ?? UeVersionComboBox.Text;
        if (!string.IsNullOrWhiteSpace(value))
        {
            var trimmed = value.Trim();
            var matchedItem = ViewModel.UeVersions.FirstOrDefault(v =>
                string.Equals(v.Replace(" ", ""), trimmed.Replace(" ", ""), StringComparison.OrdinalIgnoreCase));
            ViewModel.SelectedGame.UeVersion = matchedItem ?? trimmed;
        }
    }

    private void CloseButton_Click(object sender, RoutedEventArgs e)
    {
        CloseRequested?.Invoke(this, EventArgs.Empty);
    }

    private void ThemeComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (sender is ComboBox comboBox && comboBox.SelectedItem is string theme)
        {
            ApplyTheme(theme);
            UpdateThemePreviewSelection(theme);
        }
    }

    private void ThemeRadio_Checked(object sender, RoutedEventArgs e)
    {
        if (sender is RadioButton radio && radio.Content is string theme)
        {
            ApplyTheme(theme);
            if (ThemeComboBox.SelectedItem as string != theme)
            {
                ThemeComboBox.SelectedItem = theme;
            }
        }
    }

    private void ThemePreview_Tapped(object sender, TappedRoutedEventArgs e)
    {
        if (sender is Border border && border.Tag is string theme)
        {
            switch (theme)
            {
                case "Light":
                    LightThemeRadio.IsChecked = true;
                    break;
                case "Dark":
                    DarkThemeRadio.IsChecked = true;
                    break;
                default:
                    SystemThemeRadio.IsChecked = true;
                    break;
            }
        }
    }

    private void ApplyTheme(string theme)
    {
        ViewModel.SelectedTheme = theme;
        AppSettings.Instance.Theme = theme;

        if (App.Current is App app && app.MainWindowInstance != null)
        {
            App.ApplyThemeToWindow(app.MainWindowInstance, theme);
        }
    }

    private void UpdateThemePreviewSelection(string theme)
    {
        switch (theme)
        {
            case "Light":
                LightThemeRadio.IsChecked = true;
                break;
            case "Dark":
                DarkThemeRadio.IsChecked = true;
                break;
            default:
                SystemThemeRadio.IsChecked = true;
                break;
        }
    }

    private void SystemSoundsToggle_Toggled(object sender, RoutedEventArgs e)
    {
        AppSettings.Instance.SystemSoundsEnabled = ViewModel.SystemSoundsEnabled;
        ElementSoundPlayer.State = ViewModel.SystemSoundsEnabled ? ElementSoundPlayerState.On : ElementSoundPlayerState.Off;
    }

    private async void AddProfile_Click(object sender, RoutedEventArgs e)
    {
        var nameBox = new TextBox
        {
            PlaceholderText = "Profile name",
            MinWidth = 260
        };

        var dialog = new ContentDialog
        {
            Title = "Add profile",
            Content = nameBox,
            PrimaryButtonText = "Add",
            CloseButtonText = "Cancel",
            DefaultButton = ContentDialogButton.Primary,
            XamlRoot = XamlRoot
        };

        var result = await dialog.ShowAsync();
        if (result == ContentDialogResult.Primary)
        {
            var name = nameBox.Text?.Trim();
            if (!string.IsNullOrWhiteSpace(name))
            {
                ViewModel.AddGame(name);
            }
        }
    }

    private async void RemoveProfile_Click(object sender, RoutedEventArgs e)
    {
        if (ViewModel.SelectedGame == null)
        {
            return;
        }

        var dialog = new ContentDialog
        {
            Title = "Remove profile",
            Content = $"Remove profile \"{ViewModel.SelectedGame.Name}\"?",
            PrimaryButtonText = "Remove",
            CloseButtonText = "Cancel",
            DefaultButton = ContentDialogButton.Close,
            XamlRoot = XamlRoot
        };

        var result = await dialog.ShowAsync();
        if (result == ContentDialogResult.Primary)
        {
            ViewModel.RemoveSelectedGame();
        }
    }

    private async void EditShortcut_Click(object sender, RoutedEventArgs e)
    {
        if (sender is Button btn && btn.Tag is ShortcutConfig shortcut)
        {
            var keyDisplay = new TextBlock
            {
                Text = "Press any key combination...",
                HorizontalAlignment = HorizontalAlignment.Center,
                FontSize = 16,
                FontWeight = Microsoft.UI.Text.FontWeights.SemiBold
            };

            var dialog = new ContentDialog
            {
                Title = $"Edit shortcut for \"{shortcut.Name}\"",
                Content = keyDisplay,
                PrimaryButtonText = string.Empty,
                SecondaryButtonText = "Reset to Default",
                CloseButtonText = "Cancel",
                DefaultButton = ContentDialogButton.Close,
                XamlRoot = XamlRoot
            };

            VirtualKey capturedKey = VirtualKey.None;
            VirtualKeyModifiers capturedModifiers = VirtualKeyModifiers.None;
            bool keyCaptured = false;

            dialog.PreviewKeyDown += (s, args) =>
            {
                if (IsModifierKey(args.Key)) return;

                capturedKey = args.Key;
                capturedModifiers = Microsoft.UI.Input.InputKeyboardSource.GetKeyStateForCurrentThread(VirtualKey.Control).HasFlag(Windows.UI.Core.CoreVirtualKeyStates.Down) ? VirtualKeyModifiers.Control : VirtualKeyModifiers.None;

                if (Microsoft.UI.Input.InputKeyboardSource.GetKeyStateForCurrentThread(VirtualKey.Shift).HasFlag(Windows.UI.Core.CoreVirtualKeyStates.Down))
                    capturedModifiers |= VirtualKeyModifiers.Shift;

                if (Microsoft.UI.Input.InputKeyboardSource.GetKeyStateForCurrentThread(VirtualKey.Menu).HasFlag(Windows.UI.Core.CoreVirtualKeyStates.Down))
                    capturedModifiers |= VirtualKeyModifiers.Menu;

                if (Microsoft.UI.Input.InputKeyboardSource.GetKeyStateForCurrentThread(VirtualKey.LeftWindows).HasFlag(Windows.UI.Core.CoreVirtualKeyStates.Down) ||
                    Microsoft.UI.Input.InputKeyboardSource.GetKeyStateForCurrentThread(VirtualKey.RightWindows).HasFlag(Windows.UI.Core.CoreVirtualKeyStates.Down))
                    capturedModifiers |= VirtualKeyModifiers.Windows;

                var parts = new System.Collections.Generic.List<string>();
                if (capturedModifiers.HasFlag(VirtualKeyModifiers.Control)) parts.Add("Ctrl");
                if (capturedModifiers.HasFlag(VirtualKeyModifiers.Shift)) parts.Add("Shift");
                if (capturedModifiers.HasFlag(VirtualKeyModifiers.Menu)) parts.Add("Alt");
                if (capturedModifiers.HasFlag(VirtualKeyModifiers.Windows)) parts.Add("Win");
                parts.Add(capturedKey.ToString());

                keyDisplay.Text = string.Join(" + ", parts);
                keyCaptured = true;
                args.Handled = true;
                dialog.PrimaryButtonText = "Save";
            };

            var result = await dialog.ShowAsync();
            if (result == ContentDialogResult.Primary && keyCaptured)
            {
                shortcut.Key = capturedKey;
                shortcut.Modifiers = capturedModifiers;
                AppSettings.Save();
            }
            else if (result == ContentDialogResult.Secondary)
            {
                switch (shortcut.Id)
                {
                    case "LoadGame":
                        shortcut.Key = VirtualKey.L;
                        shortcut.Modifiers = VirtualKeyModifiers.Control;
                        break;
                    case "Search":
                        shortcut.Key = VirtualKey.F;
                        shortcut.Modifiers = VirtualKeyModifiers.Control;
                        break;
                    case "Settings":
                        shortcut.Key = VirtualKey.I;
                        shortcut.Modifiers = VirtualKeyModifiers.Control;
                        break;
                }
                AppSettings.Save();
            }
        }
    }

    private bool IsModifierKey(VirtualKey key)
    {
        return key == VirtualKey.Control || key == VirtualKey.Shift || key == VirtualKey.Menu ||
               key == VirtualKey.LeftWindows || key == VirtualKey.RightWindows;
    }

    private IntPtr GetWindowHandleForCurrentXamlRoot()
    {

        if (App.Current is App app && app.MainWindowInstance != null)
        {
            return WindowNative.GetWindowHandle(app.MainWindowInstance);
        }

        return IntPtr.Zero;
    }

    private async void CheckForUpdatesButton_Click(object sender, RoutedEventArgs e)
    {
        var checkBtn = sender as Button;
        if (checkBtn != null)
            checkBtn.IsEnabled = false;

        try
        {
            var updateService = new UpdateService();
            var result = await updateService.CheckForUpdatesAsync();

            if (result.HasUpdate && result.Manifest != null)
            {
                await UpdatePrompt.ShowAsync(this.XamlRoot, result);
            }
            else if (!string.IsNullOrEmpty(result.ErrorMessage))
            {
                var errorDialog = new ContentDialog
                {
                    Title = "Update Check Failed",
                    Content = $"Could not check for updates:\n{result.ErrorMessage}",
                    CloseButtonText = "OK",
                    XamlRoot = this.XamlRoot
                };
                await errorDialog.ShowAsync();
            }
            else
            {
                var upToDateDialog = new ContentDialog
                {
                    Title = "Up to Date",
                    Content = $"You are using the latest version of PakViewer (v{result.CurrentVersion}).",
                    CloseButtonText = "OK",
                    XamlRoot = this.XamlRoot
                };
                await upToDateDialog.ShowAsync();
            }
        }
        catch (Exception ex)
        {
            var errDialog = new ContentDialog
            {
                Title = "Error",
                Content = $"Failed to check for updates:\n{ex.Message}",
                CloseButtonText = "OK",
                XamlRoot = this.XamlRoot
            };
            await errDialog.ShowAsync();
        }
        finally
        {
            if (checkBtn != null)
                checkBtn.IsEnabled = true;
        }
    }
}
