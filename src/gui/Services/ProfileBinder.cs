using System.Collections.Generic;

namespace FanControl.Gui.Profile;

public class ProfileBinder
{
    private readonly Dictionary<string, List<(double x, double y)>> _profiles = new();
    private string _activeProfile = "default";

    public void Save(string name, List<(double x, double y)> curve)
    {
        _profiles[name] = new List<(double x, double y)>(curve);
    }

    public List<(double x, double y)> Load(string name)
    {
        if (_profiles.TryGetValue(name, out var curve))
            return new List<(double x, double y)>(curve);

        return new();
    }

    public void SetActive(string name)
    {
        _activeProfile = name;
    }

    public string GetActive()
    {
        return _activeProfile;
    }

    public List<(double x, double y)> GetActiveCurve()
    {
        return Load(_activeProfile);
    }

    public IEnumerable<string> ListProfiles()
    {
        return _profiles.Keys;
    }
}
