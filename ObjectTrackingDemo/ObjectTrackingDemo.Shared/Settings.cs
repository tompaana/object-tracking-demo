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
        private const string KeyMode = "Mode";
        private const string KeyIsoSpeedPreset = "IsoSpeedPreset";
        private const string KeyExposure = "Exposure";

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

        public Mode Mode
        {
            get;
            set;
        }

        public IsoSpeedPreset IsoSpeedPreset
        {
            get;
            set;
        }

        public IReadOnlyList<IsoSpeedPreset> SupportedIsoSpeedPresets
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

            if (_localSettings.Values.ContainsKey(KeyMode))
            {
                Mode = (Mode)Enum.Parse(typeof(Mode), (string)_localSettings.Values[KeyMode]);
            }
            else
            {
                Mode = Mode.ChromaFilter;
            }

            if (_localSettings.Values.ContainsKey(KeyIsoSpeedPreset))
            {
                IsoSpeedPreset = (IsoSpeedPreset)Enum.Parse(typeof(IsoSpeedPreset), (string)_localSettings.Values[KeyIsoSpeedPreset]);
            }
            else
            {
                IsoSpeedPreset = IsoSpeedPreset.Auto;
            }

            if (_localSettings.Values.ContainsKey(KeyExposure))
            {
                Exposure = (int)_localSettings.Values[KeyExposure];
            }
            else
            {
                Exposure = VideoEngine.ExposureAutoValue; // Auto by default, -1
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
            _localSettings.Values[KeyMode] = Mode.ToString();
            _localSettings.Values[KeyIsoSpeedPreset] = IsoSpeedPreset.ToString();
            _localSettings.Values[KeyExposure] = Exposure;
        }
    }
}
