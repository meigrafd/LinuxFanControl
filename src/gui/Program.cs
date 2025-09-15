public static class Program
{
    public static void Main(string[] args)
    {
        var configDir = Path.Combine(AppContext.BaseDirectory, "config");
        var localeDir = Path.Combine(AppContext.BaseDirectory, "Locales");

        var langArg  = args.FirstOrDefault(a => a.StartsWith("--lang="));
        var themeArg = args.FirstOrDefault(a => a.StartsWith("--theme="));

        var langCode = langArg?.Split("=")[1] ?? "auto";
        var themeName = themeArg?.Split("=")[1] ?? "default";

        LocaleManager.Load(langCode);
        ThemeManager.Load(themeName);

        var rpc = new JsonRpcClient
        {
            TransportSendReceive = RpcTransport.SendReceive
        };

        Startup.Launch(rpc);
    }
}
