using System.Text.Json.Nodes;

public class ProfileModel
{
    public string Name { get; }
    public List<JsonNode> Fans { get; } = new();
    public List<JsonNode> Sensors { get; } = new();
    public List<JsonNode> Mixes { get; } = new();
    public List<JsonNode> Triggers { get; } = new();

    public ProfileModel(string name)
    {
        Name = name;
    }

    public void AddFan(JsonNode fan)
    {
        if (fan != null) Fans.Add(fan);
    }

    public void AddSensor(JsonNode sensor)
    {
        if (sensor != null) Sensors.Add(sensor);
    }

    public void AddMix(JsonNode mix)
    {
        if (mix != null) Mixes.Add(mix);
    }

    public void AddTrigger(JsonNode trigger)
    {
        if (trigger != null) Triggers.Add(trigger);
    }

    public JsonObject ToJson()
    {
        return new JsonObject
        {
            ["name"] = Name,
            ["fans"] = new JsonArray(Fans.ToArray()),
            ["sensors"] = new JsonArray(Sensors.ToArray()),
            ["mix"] = new JsonArray(Mixes.ToArray()),
            ["triggers"] = new JsonArray(Triggers.ToArray())
        };
    }
}
