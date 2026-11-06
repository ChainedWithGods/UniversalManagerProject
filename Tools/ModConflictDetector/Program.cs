using System.CommandLine;
using ModConflictDetector;

var rootCommand = new RootCommand("Unreal Engine Mod Conflict Detector - Detects file conflicts between pak/utoc mods");

var modsPathOption = new Option<string>(
    aliases: ["--mods-path", "-m"],
    description: "Path to the mods directory containing pak/utoc files")
{ IsRequired = true };

var gamePathOption = new Option<string?>(
    aliases: ["--game-path", "-g"],
    description: "Path to the game's pak directory (for IoStore global data)");

var usmapOption = new Option<string?>(
    aliases: ["--usmap", "-u"],
    description: "Path to the .usmap mappings file");

var aesKeyOption = new Option<string?>(
    aliases: ["--aes-key", "-k"],
    description: "AES encryption key (hex format, e.g., 0x...)");

var ueVersionOption = new Option<string>(
    aliases: ["--ue-version", "-v"],
    description: "Unreal Engine version (e.g., '5.3', '4.27')")
{ IsRequired = false };
ueVersionOption.SetDefaultValue("5.3");

var outputOption = new Option<string?>(
    aliases: ["--output", "-o"],
    description: "Output file path for JSON results (prints to console if not specified)");

var verboseOption = new Option<bool>(
    aliases: ["--verbose"],
    description: "Enable verbose output");

var includeDisabledOption = new Option<bool>(
    aliases: ["--include-disabled"],
    description: "Include disabled mods (files ending with -x)");

rootCommand.AddOption(modsPathOption);
rootCommand.AddOption(gamePathOption);
rootCommand.AddOption(usmapOption);
rootCommand.AddOption(aesKeyOption);
rootCommand.AddOption(ueVersionOption);
rootCommand.AddOption(outputOption);
rootCommand.AddOption(verboseOption);
rootCommand.AddOption(includeDisabledOption);

rootCommand.SetHandler(async (modsPath, gamePath, usmap, aesKey, ueVersion, output, verbose, includeDisabled) =>
{
    var detector = new ConflictDetector(verbose);
    var result = await detector.DetectConflictsAsync(new ConflictDetectorOptions
    {
        ModsPath = modsPath,
        GamePath = gamePath,
        UsmapPath = usmap,
        AesKey = aesKey,
        UeVersion = ueVersion,
        IncludeDisabledMods = includeDisabled
    });

    var json = Newtonsoft.Json.JsonConvert.SerializeObject(result, Newtonsoft.Json.Formatting.Indented);

    if (!string.IsNullOrEmpty(output))
    {
        await File.WriteAllTextAsync(output, json);
        Console.WriteLine($"Results written to: {output}");
    }
    else
    {
        Console.WriteLine(json);
    }

    Environment.ExitCode = result.Conflicts.Count > 0 ? 1 : 0;

}, modsPathOption, gamePathOption, usmapOption, aesKeyOption, ueVersionOption, outputOption, verboseOption, includeDisabledOption);

return await rootCommand.InvokeAsync(args);
