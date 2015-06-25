using System;
using System.Collections.Generic;
using System.Text;
using Windows.Media.Devices;
using Windows.Storage;
using Windows.UI;

namespace ObjectTrackingDemo
{
    public class Settings
    {
        public static readonly Color DefaultTargetColor = Color.FromArgb(0xff, 0xee, 0x10, 0x10);
        public static readonly double DefaultThreshold = 20d;
        private const string KeyTargetColorR = "TargetColorR";
        private const string KeyTargetColorG = "TargetColorG";
        private const string KeyTargetColorB = "TargetColorB";
        private const string KeyThreshold = "Threshold";
        private const string KeyFlash = "Flash";
        private const string KeyTorch = "Torch";

        private ApplicationDataContainer _localSettings = ApplicationData.Current.LocalSettings;

        public Color TargetColor
        {
            get;
            set;
        }

        public double Threshold
        {
            get;
            set;
        }

        public bool Flash
        {
            get;
            set;
        }

        public bool Torch
        {
            get;
            set;
        }
        public IsoSpeedPreset SelectedIso
        {
            get;
            set;
        }
        public IReadOnlyList<IsoSpeedPreset> SupportedIsoSpeeds
        {
            get;
            set;
        }
        public int Exposure
        {
            get;
            set;
        }

        public void Load()
        {
            if (_localSettings.Values.ContainsKey(KeyTargetColorR))
            {
                byte r = (byte)_localSettings.Values[KeyTargetColorR];
                byte g = (byte)_localSettings.Values[KeyTargetColorG];
                byte b = (byte)_localSettings.Values[KeyTargetColorB];
                TargetColor = Color.FromArgb(0xff, r, g, b);
            }
            else
            {
                TargetColor = DefaultTargetColor;
            }

            if (_localSettings.Values.ContainsKey(KeyThreshold))
            {
                Threshold = (double)_localSettings.Values[KeyThreshold];
            }
            else
            {
                Threshold = DefaultThreshold;
            }

            if (_localSettings.Values.ContainsKey(KeyFlash))
            {
                Flash = (bool)_localSettings.Values[KeyFlash];
            }

            if (_localSettings.Values.ContainsKey(KeyTorch))
            {
                Torch = (bool)_localSettings.Values[KeyTorch];
            }

        }

        public void Save()
        {
            _localSettings.Values[KeyTargetColorR] = TargetColor.R;
            _localSettings.Values[KeyTargetColorG] = TargetColor.G;
            _localSettings.Values[KeyTargetColorB] = TargetColor.B;
            _localSettings.Values[KeyThreshold] = Threshold;
            _localSettings.Values[KeyFlash] = Flash;
            _localSettings.Values[KeyTorch] = Torch;
        }
    }
}
