using Gtk;
using System;
using FanControl.Gui.Views;

namespace FanControl.Gui.Views;

public class SensorsView : VBox
{
    private readonly Label _cpuLabel;
    private readonly Label _gpuLabel;
    private readonly Label _mbLabel;

    public SensorsView()
    {
        Spacing = 10;

        Label title = new(Translation.Get("sidebar.sensors"));
        title.SetMarkup($"<b>{Translation.Get("sidebar.sensors")}</b>");
        PackStart(title, false, false, 0);

        _cpuLabel = new Label("CPU: -- °C");
        _gpuLabel = new Label("GPU: -- °C");
        _mbLabel = new Label("Mainboard: -- °C");

        PackStart(_cpuLabel, false, false, 0);
        PackStart(_gpuLabel, false, false, 0);
        PackStart(_mbLabel, false, false, 0);
    }

    public void UpdateValues(double cpu, double gpu, double mb)
    {
        _cpuLabel.Text = $"CPU: {cpu:F1} °C";
        _gpuLabel.Text = $"GPU: {gpu:F1} °C";
        _mbLabel.Text = $"Mainboard: {mb:F1} °C";
    }
}
