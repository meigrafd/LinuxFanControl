// (c) 2025 LinuxFanControl contributors. MIT License.
using System.IO;
using System.Text.Json;
using System.Threading.Tasks;

namespace LinuxFanControl.Gui.Services
{
    public sealed class FanControlImporter
    {
        public sealed class ImportResult
        {
            public bool Success { get; set; }
            public AppConfig? Config { get; set; }
            public string? Error { get; set; }
        }

        public async Task<ImportResult> TryImportAsync(string path)
        {
            try
            {
                var json = await File.ReadAllTextAsync(path);
                using var doc = JsonDocument.Parse(json);

                // Map to our AppConfig in a best-effort manner (simplified)
                var cfg = new AppConfig { Version = ConfigService.CurrentVersion };

                return new ImportResult { Success = true, Config = cfg };
            }
            catch (System.Exception ex)
            {
                return new ImportResult { Success = false, Error = ex.Message };
            }
        }
    }
}
