using System.Text.Json.Nodes;

public static class ProfileLoader
{
    private static readonly string ProfileDir = Path.Combine(AppContext.BaseDirectory, "Profiles");

    public static List<ProfileModel> LoadAll()
    {
        if (!Directory.Exists(ProfileDir))
            return new List<ProfileModel>();

        var profiles = new List<ProfileModel>();

        foreach (var file in Directory.GetFiles(ProfileDir, "*.json"))
        {
            try
            {
                var json = File.ReadAllText(file);
                var node = JsonNode.Parse(json);
                var model = Parse(node, Path.GetFileNameWithoutExtension(file));
                profiles.Add(model);
            }
            catch
            {
                // skip invalid profile
            }
        }

        return profiles;
    }

    public static ProfileModel? Load(string name)
    {
        var path = Path.Combine(ProfileDir, $"{name}.json");
        if (!File.Exists(path))
            return null;

        try
        {
            var json = File.ReadAllText(path);
            var node = JsonNode.Parse(json);
            return Parse(node, name);
        }
        catch
        {
            return null;
        }
    }

    public static void Save(ProfileModel model)
    {
        var node = model.ToJson();
        var json = node.ToJsonString(new JsonSerializerOptions
        {
            WriteIndented = true
        });

        Directory.CreateDirectory(ProfileDir);
        var path = Path.Combine(ProfileDir, $"{model.Name}.json");
        File.WriteAllText(path, json);
    }

    public static void Delete(string name)
    {
        var path = Path.Combine(ProfileDir, $"{name}.json");
        if (File.Exists(path))
            File.Delete(path);
    }

    private static ProfileModel Parse(JsonNode? node, string name)
    {
        var model = new ProfileModel(name);

        var fans = node?["fans"]?.AsArray() ?? [];
        foreach (var fanNode in fans)
            model.AddFan(fanNode);

        var sensors = node?["sensors"]?.AsArray() ?? [];
        foreach (var sensorNode in sensors)
            model.AddSensor(sensorNode);

        var mix = node?["mix"]?.AsArray() ?? [];
        foreach (var mixNode in mix)
            model.AddMix(mixNode);

        var triggers = node?["triggers"]?.AsArray() ?? [];
        foreach (var triggerNode in triggers)
            model.AddTrigger(triggerNode);

        return model;
    }
}
