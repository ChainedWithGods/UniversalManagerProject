using Microsoft.UI.Xaml;
using PakViewer.Views;
using PakViewer.Settings;
using Serilog;
using System;
using System.Collections.Concurrent;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Linq;
using System.Collections.Generic;
using System.Text;

namespace PakViewer;

public partial class App : Application
{
    private Window? m_window;

    public Window? MainWindowInstance => m_window;

    public static string NormalizePath(string? path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return string.Empty;
        }

        try
        {
            var full = Path.GetFullPath(path.Trim());
            return full.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        }
        catch
        {
            return path.Trim().TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
        }
    }

    private static string NormalizeUeVersion(string? version)
    {
        if (string.IsNullOrWhiteSpace(version))
        {
            return string.Empty;
        }

        var v = version.Trim();
        if (v.StartsWith("UE ", StringComparison.OrdinalIgnoreCase))
        {
            v = "UE" + v[3..].Trim();
        }

        if (!v.StartsWith("UE", StringComparison.OrdinalIgnoreCase))
        {
            v = "UE" + v;
        }

        if (v.Length > 2)
        {
            var remainder = v[2..].TrimStart(' ', '_');
            if (!string.IsNullOrEmpty(remainder) && char.IsDigit(remainder[0]))
            {
                v = "UE " + remainder;
            }
        }

        return v;
    }

    public App()
    {
        this.InitializeComponent();

        var cmdArgs = ParseCommandLineArgs(Environment.GetCommandLineArgs().Skip(1).ToArray());

        if (!cmdArgs.HideConsole)
        {
            AllocConsole();
        }

        InitializeLogging();

        Log.Information($"Command line argument count: {Environment.GetCommandLineArgs().Length - 1}");
        for (int i = 1; i < Environment.GetCommandLineArgs().Length; i++)
        {
            Log.Information($"Arg[{i-1}]: {Environment.GetCommandLineArgs()[i]}");
        }

        this.UnhandledException += (_, e) =>
        {
            Log.Error(e.Exception, "Unhandled exception");
            e.Handled = true;
        };

        TaskScheduler.UnobservedTaskException += (_, e) =>
        {
            Log.Error(e.Exception, "Unobserved task exception");
            e.SetObserved();
        };

        AppDomain.CurrentDomain.UnhandledException += (_, e) =>
        {
            if (e.ExceptionObject is Exception ex)
            {
                Log.Error(ex, "AppDomain unhandled exception");
            }
            else
            {
                Log.Error("AppDomain unhandled exception");
            }
        };

        Log.Information("Loading application settings...");
        AppSettings.Load();
        Log.Information($"Loaded {AppSettings.Instance.Games.Count} game configurations");

        var seenPakPaths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var removedDuplicates = 0;
        for (var idx = AppSettings.Instance.Games.Count - 1; idx >= 0; idx--)
        {
            var g = AppSettings.Instance.Games[idx];
            var normalizedPakPath = NormalizePath(g.PakPath);
            if (!string.Equals(g.PakPath, normalizedPakPath, StringComparison.Ordinal))
            {
                g.PakPath = normalizedPakPath;
            }

            if (string.IsNullOrEmpty(normalizedPakPath))
            {
                continue;
            }

            if (!seenPakPaths.Add(normalizedPakPath))
            {
                AppSettings.Instance.Games.RemoveAt(idx);
                removedDuplicates++;
            }
        }

        if (removedDuplicates > 0)
        {
            Log.Information($"Removed {removedDuplicates} duplicate game profile(s) based on PakPath");
            AppSettings.Save();
        }

        var normalizedUeCount = 0;
        foreach (var g in AppSettings.Instance.Games)
        {
            var normalized = NormalizeUeVersion(g.UeVersion);
            if (!string.IsNullOrEmpty(normalized) && !string.Equals(g.UeVersion, normalized, StringComparison.Ordinal))
            {
                g.UeVersion = normalized;
                normalizedUeCount++;
            }
        }

        if (normalizedUeCount > 0)
        {
            AppSettings.Save();
        }

        if (!string.IsNullOrEmpty(cmdArgs.PaksPath))
        {
            Log.Information($"Using paks path from command line: {cmdArgs.PaksPath}");

            var normalizedCmdPakPath = NormalizePath(cmdArgs.PaksPath);

            var configuredGameName = string.IsNullOrWhiteSpace(cmdArgs.GameName)
                ? "Game (Auto-configured)"
                : cmdArgs.GameName;

            var existing = AppSettings.Instance.Games
                .FirstOrDefault(g => string.Equals(NormalizePath(g.PakPath), normalizedCmdPakPath, StringComparison.OrdinalIgnoreCase));

            if (existing == null)
            {
                var normalizedCmdUeVersion = NormalizeUeVersion(cmdArgs.UeVersion);
                var gameConfig = new GameConfig
                {
                    Name = configuredGameName,
                    PakPath = normalizedCmdPakPath,
                    UsmapPath = cmdArgs.UsmapFile,
                    AesKey = cmdArgs.AesKey ?? "",
                    UeVersion = !string.IsNullOrEmpty(normalizedCmdUeVersion) ? normalizedCmdUeVersion : "UE 5.3"
                };

                AppSettings.Instance.Games.Add(gameConfig);
                AppSettings.Instance.LastSelectedGame = gameConfig.Name;
                AppSettings.Save();
            }
            else
            {

                bool changed = false;

                if (!string.Equals(NormalizePath(existing.PakPath), normalizedCmdPakPath, StringComparison.OrdinalIgnoreCase))
                {
                    existing.PakPath = normalizedCmdPakPath;
                    changed = true;
                }

                if (!string.IsNullOrWhiteSpace(cmdArgs.GameName) && !string.Equals(existing.Name, cmdArgs.GameName, StringComparison.Ordinal))
                {
                    existing.Name = cmdArgs.GameName;
                    changed = true;
                }

                if (!string.IsNullOrEmpty(cmdArgs.UsmapFile) && cmdArgs.UsmapFile != existing.UsmapPath)
                {
                    existing.UsmapPath = cmdArgs.UsmapFile;
                    changed = true;
                }

                if (!string.IsNullOrEmpty(cmdArgs.AesKey) && cmdArgs.AesKey != existing.AesKey)
                {
                    existing.AesKey = cmdArgs.AesKey;
                    changed = true;
                }

                if (!string.IsNullOrEmpty(cmdArgs.UeVersion) && cmdArgs.UeVersion != existing.UeVersion)
                {
                    var normalizedCmdUeVersion = NormalizeUeVersion(cmdArgs.UeVersion);
                    if (!string.IsNullOrEmpty(normalizedCmdUeVersion) && !string.Equals(normalizedCmdUeVersion, existing.UeVersion, StringComparison.Ordinal))
                    {
                        existing.UeVersion = normalizedCmdUeVersion;
                        changed = true;
                    }
                }

                if (changed)
                {
                    AppSettings.Save();
                }
                AppSettings.Instance.LastSelectedGame = existing.Name;
            }
        }

        if (cmdArgs.Headless && !string.IsNullOrEmpty(cmdArgs.ExportStructurePath))
        {
            Log.Information("Running in headless export mode");

            Task.Run(async () =>
            {
                try
                {
                    await RunHeadlessExportAsync(cmdArgs);
                    Log.Information("Export completed, shutting down");
                    Environment.Exit(0);
                }
                catch (Exception ex)
                {
                    Log.Error(ex, "Export failed");
                    Console.Error.WriteLine($"ERROR: {ex.Message}");
                    if (ex.InnerException != null)
                        Console.Error.WriteLine($"Inner: {ex.InnerException.Message}");
                    Environment.Exit(1);
                }
            });
            return;
        }
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {

        try
        {
            Resources["BoolToVis"] = new Converters.BoolToVisibilityConverter();
            Resources["InverseBoolToVis"] = new Converters.InverseBoolToVisibilityConverter();
            Resources["NullToVis"] = new Converters.NullToVisibilityConverter();
            Resources["InverseNullToVis"] = new Converters.InverseNullToVisibilityConverter();
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to initialize converters");
        }

        var cmdArgs = ParseCommandLineArgs(Environment.GetCommandLineArgs().Skip(1).ToArray());

        if (cmdArgs.Headless)
        {
            return;
        }

        m_window = new MainWindow(cmdArgs);
        ApplyThemeToWindow(m_window, AppSettings.Instance.Theme);
        m_window.Activate();
    }

    public static void ApplyThemeToWindow(Window? window, string? theme)
    {
        if (window?.Content is FrameworkElement root)
        {
            root.RequestedTheme = ResolveTheme(theme);
        }
    }

    public static ElementTheme ResolveTheme(string? theme)
    {
        return theme?.ToLowerInvariant() switch
        {
            "light" => ElementTheme.Light,
            "dark" => ElementTheme.Dark,
            _ => ElementTheme.Default
        };
    }

    private void InitializeLogging()
    {
        var logPath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "PakViewer",
            "Logs",
            "app.log");

        Directory.CreateDirectory(Path.GetDirectoryName(logPath)!);

        var pathSanitizingFormatter = new Logging.PathSanitizingFormatter(
            "{Timestamp:yyyy-MM-dd HH:mm:ss.fff zzz} [{Level:u3}] {Message:lj}{NewLine}{Exception}");

        var seenWarnings = new ConcurrentDictionary<string, byte>(StringComparer.Ordinal);

        Log.Logger = new LoggerConfiguration()
            .MinimumLevel.Debug()

            .Filter.ByExcluding(logEvent =>
                logEvent.Level == Serilog.Events.LogEventLevel.Warning &&
                !seenWarnings.TryAdd(logEvent.RenderMessage(), 0))
            .WriteTo.File(pathSanitizingFormatter, logPath, rollingInterval: RollingInterval.Day)
            .WriteTo.Console(formatter: pathSanitizingFormatter)
            .CreateLogger();

        Log.Information("Application started");
        Log.Information($"Settings path: {AppSettings.SettingsFilePath}");
        Log.Information($"Log path: {logPath}");
    }

    private CommandLineArgs ParseCommandLineArgs(string[] args)
    {
        var result = new CommandLineArgs();

        for (int i = 0; i < args.Length; i++)
        {
            var arg = args[i];

            if (arg == "--paks-path" && i + 1 < args.Length)
            {
                result.PaksPath = args[++i];
            }
            else if (arg == "--usmap-file" && i + 1 < args.Length)
            {
                result.UsmapFile = args[++i];
            }
            else if (arg == "--aes-key" && i + 1 < args.Length)
            {
                result.AesKey = args[++i];
            }
            else if (arg == "--ue-version" && i + 1 < args.Length)
            {
                result.UeVersion = args[++i];
            }
            else if (arg == "--game-name" && i + 1 < args.Length)
            {
                result.GameName = args[++i];
            }
            else if (arg == "--open-file" && i + 1 < args.Length)
            {
                result.FileToOpen = args[++i];
            }
            else if (arg == "--export-structure" && i + 1 < args.Length)
            {
                result.ExportStructurePath = args[++i];
            }
            else if (arg == "--headless")
            {
                result.Headless = true;
            }
            else if (arg == "--hide-console")
            {
                result.HideConsole = true;
            }
            else if (arg == "--minimal-ui")
            {
                result.MinimalUI = true;
            }
            else if (arg == "--conflict-mode")
            {
                result.ConflictMode = true;
            }
            else if (arg == "--conflict-mod" && i + 1 < args.Length)
            {
                result.ConflictMod = args[++i];
                result.ConflictMode = true;
            }
            else if (arg == "--conflict-json" && i + 1 < args.Length)
            {
                result.ConflictJsonPath = args[++i];
                result.ConflictMode = true;
            }
            else if (File.Exists(arg))
            {

                result.FileToOpen = arg;
            }
        }

        Log.Information($"Command line args parsed: PaksPath={result.PaksPath}, UsmapFile={result.UsmapFile}, MinimalUI={result.MinimalUI}, ConflictMode={result.ConflictMode}");
        return result;
    }

    private async Task RunHeadlessExportAsync(CommandLineArgs cmdArgs)
    {
        Log.Information($"Exporting structure to: {cmdArgs.ExportStructurePath}");
        Log.Information($"File to open: {cmdArgs.FileToOpen}");
        Log.Information($"Paks path: {cmdArgs.PaksPath}");
        Log.Information($"Usmap file: {cmdArgs.UsmapFile}");

        if (string.IsNullOrWhiteSpace(cmdArgs.ExportStructurePath))
        {
            throw new ArgumentException("Export path is missing. Use --export-structure <path>.");
        }

        if (string.IsNullOrEmpty(cmdArgs.FileToOpen))
        {
            throw new ArgumentException($"Mod file path is empty. Use --open-file <path>.");
        }

        if (!File.Exists(cmdArgs.FileToOpen))
        {
            throw new FileNotFoundException($"Mod file not found: {cmdArgs.FileToOpen}");
        }

        var pakLoader = new PakViewer.Services.PakLoaderService();

        var gameConfig = new PakViewer.Settings.GameConfig
        {
            Name = "Export Session",
            PakPath = cmdArgs.PaksPath ?? "",
            UsmapPath = cmdArgs.UsmapFile ?? "",
            AesKey = cmdArgs.AesKey ?? "",
            UeVersion = cmdArgs.UeVersion ?? "UE 5.3"
        };

        Log.Information("Loading mod for structure export...");
        pakLoader.LoadModOnly(cmdArgs.FileToOpen, gameConfig);

        var fileTree = pakLoader.GetModFileTree(cmdArgs.FileToOpen);

        string gameName = !string.IsNullOrEmpty(cmdArgs.GameName) ? cmdArgs.GameName : "Unreal Engine Game";
        string ueVersion = gameConfig.UeVersion ?? "Unknown";

        var sb = new StringBuilder();
        sb.AppendLine("===============================================");
        sb.AppendLine($"       {gameName.ToUpper()} MOD STRUCTURE REPORT");
        sb.AppendLine("===============================================");
        sb.AppendLine();
        sb.AppendLine($"Generated: {DateTime.Now:yyyy-MM-dd HH:mm:ss}");
        sb.AppendLine($"Game: {gameName}");
        sb.AppendLine($"Unreal Engine Version: {ueVersion}");
        sb.AppendLine($"Mod File: {Path.GetFileName(cmdArgs.FileToOpen)}");
        sb.AppendLine($"Full Path: {cmdArgs.FileToOpen}");
        sb.AppendLine();
        sb.AppendLine("-----------------------------------------------");
        sb.AppendLine("FILE STRUCTURE");
        sb.AppendLine("-----------------------------------------------");
        sb.AppendLine();

        foreach (var node in fileTree)
        {
            PrintNode(sb, node, "");
        }

        sb.AppendLine();
        sb.AppendLine("-----------------------------------------------");
        sb.AppendLine("SUMMARY");
        sb.AppendLine("-----------------------------------------------");

        int fileCount = 0;
        int dirCount = 0;
        CountNodes(fileTree, ref fileCount, ref dirCount);

        sb.AppendLine($"Total Directories: {dirCount}");
        sb.AppendLine($"Total Files: {fileCount}");
        sb.AppendLine();
        sb.AppendLine("===============================================");
        sb.AppendLine("This report was generated by Universal Manager");
        sb.AppendLine("Generated with effort by Universal Manager");
        sb.AppendLine("===============================================");

        await File.WriteAllTextAsync(cmdArgs.ExportStructurePath, sb.ToString());

        Log.Information($"Export completed successfully: {fileCount} files, {dirCount} directories");
    }

    private void PrintNode(StringBuilder sb, Models.FileNodeViewModel node, string indent)
    {
        string icon = node.IsDirectory ? "[DIR]" : "[FILE]";
        sb.AppendLine($"{indent}{icon} {node.Name}");

        if (node.IsDirectory && node.Children.Count > 0)
        {
            foreach (var child in node.Children)
            {
                PrintNode(sb, child, indent + "  ");
            }
        }
    }

    private void CountNodes(List<Models.FileNodeViewModel> nodes, ref int fileCount, ref int dirCount)
    {
        foreach (var node in nodes)
        {
            if (node.IsDirectory)
            {
                dirCount++;
                CountNodes(node.Children.ToList(), ref fileCount, ref dirCount);
            }
            else
            {
                fileCount++;
            }
        }
    }

    public class CommandLineArgs
    {
        public string? PaksPath { get; set; }
        public string? UsmapFile { get; set; }
        public string? AesKey { get; set; }
        public string? UeVersion { get; set; }
        public string? GameName { get; set; }
        public string? FileToOpen { get; set; }
        public string? ExportStructurePath { get; set; }
        public bool Headless { get; set; }
        public bool HideConsole { get; set; }
        public bool MinimalUI { get; set; }
        public bool ConflictMode { get; set; }
        public string? ConflictMod { get; set; }
        public string? ConflictJsonPath { get; set; }
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool AllocConsole();
}
