using System.Text.Json.Nodes;

public class RuleModel
{
    public string TriggerId { get; }
    public string TargetId { get; }
    public float[] CurvePoints { get; }
    public float Hysteresis { get; }
    public float Tau { get; }

    public RuleModel(string triggerId, string targetId, float[] curvePoints, float hysteresis, float tau)
    {
        TriggerId = triggerId;
        TargetId = targetId;
        CurvePoints = curvePoints;
        Hysteresis = hysteresis;
        Tau = tau;
    }

    public static List<RuleModel> FromJson(JsonNode? node)
    {
        var list = new List<RuleModel>();
        if (node is not JsonArray arr) return list;

        foreach (var item in arr)
        {
            var trigger = item?["trigger"]?.ToString() ?? "";
            var target = item?["target"]?.ToString() ?? "";
            var curve = item?["curve"]?.AsArray();
            var hyst = item?["hysteresis"]?.GetValue<float>() ?? 0f;
            var tau = item?["tau"]?.GetValue<float>() ?? 0f;

            var points = new float[3];
            for (int i = 0; i < 3; i++)
                points[i] = curve?[i]?.GetValue<float>() ?? 0f;

            list.Add(new RuleModel(trigger, target, points, hyst, tau));
        }

        return list;
    }
}
