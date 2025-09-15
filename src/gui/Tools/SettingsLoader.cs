public static class SettingsLoader
{
    public static List<string> GetAvailableThemes()
    {
        var themeDir = Path.Combine(AppContext.BaseDirectory, "Themes");
        if (!Directory.Exists(themeDir))
            return new List<string>();

        return Directory.GetFiles(themeDir, "*.json")
        .Select(Path.GetFileNameWithoutExtension)
        .OrderBy(name => name)
        .ToList();
    }

    public static List<string> GetAvailableLanguages()
    {
        var localeDir = Path.Combine(AppContext.BaseDirectory, "Locales");
        if (!Directory.Exists(localeDir))
            return new List<string>();

        return Directory.GetFiles(localeDir, "*.json")
        .Select(Path.GetFileNameWithoutExtension)
        .OrderBy(name => name)
        .ToList();
    }
}
