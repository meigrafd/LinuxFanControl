using Gtk;
using System;
using GLib;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;

namespace FanControl.Gui.Widgets;

public class SensorTile : Frame
{
    private Label _nameLabel;
    private Label _valueLabel;
    private readonly string _sensorPath;
    private readonly RpcClient _rpc;
    private uint _refreshId;

    public SensorTile(string sensorPath)
    {
        _sensorPath = sensorPath;
        _rpc = new RpcClient();

        // Frame-Titel auf Sensor-Label setzen
        Label = Translation.Get("sensor.loading");
        var vbox = new VBox { Spacing = 2, BorderWidth = 4 };
        _nameLabel = new Label();
        _valueLabel = new Label();
        vbox.PackStart(_nameLabel, false, false, 0);
        vbox.PackStart(_valueLabel, false, false, 0);
        Add(vbox);

        Translation.LanguageChanged += Redraw;

        // Initial befüllen und dann zyklisch aktualisieren
        Redraw();
        _refreshId = Timeout.Add(2000, new TimeoutHandler(() =>
        {
            UpdateValue();
            return true;
        }));
    }

    private void Redraw()
    {
        // Name & Einheit aus listSensors auslesen
        try
        {
            var response = _rpc.SendRequest("listSensors");
            if (response is JsonObject obj && obj["result"] is JsonArray sensors)
            {
                foreach (var s in sensors)
                {
                    if (s?["path"]?.ToString() == _sensorPath)
                    {
                        var label = s["label"]?.ToString() ?? _sensorPath;
                        var unit = s["unit"]?.ToString() ?? "";
                        _nameLabel.Text = label;
                        Label = label; // Frame-Titel
                        _valueLabel.Text = $"-- {unit}";
                        break;
                    }
                }
            }
        }
        catch
        {
            _nameLabel.Text = Translation.Get("sensor.error");
            _valueLabel.Text = "";
        }
    }

    private void UpdateValue()
    {
        // Aktuellen Messwert vom Daemon holen
        try
        {
            var param = new JsonObject { ["path"] = _sensorPath };
            var response =
