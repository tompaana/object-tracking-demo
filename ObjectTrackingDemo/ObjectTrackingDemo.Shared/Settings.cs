using System;
using System.Collections.Generic;
using System.Text;
using Windows.Media.Devices;
using Windows.Storage;
using Windows.UI;

namespace ObjectTrackingDemo
{
    public enum AppMode
    {
        Photo,
        Camera
    };

    public class Settings
    {
        public static readonly Color DefaultTargetColor = Color.FromArgb(0xff, 0xee, 0x10, 0x10);
        public static readonly double DefaultThreshold = 20d;
        private const string KeyAppMode = "AppMode";
        private const string KeyTargetColorR = "TargetColorR";
        private const string KeyTargetColorG = "TargetColorG";
        private const string KeyTargetColorB = "TargetColorB";
        private const string KeyThreshold = "Threshold";
        private const string KeyFlash = "Flash";
        private const string KeyTorch = "Torch";
        private const string KeyMode = "Mode";
        private const string KeyRemoveNoise = "RemoveNoise";
        private const string KeyApplyEffectOnly = "ApplyEffectOnly";
        private const string KeyIsoSpeedPreset = "IsoSpeedPreset";
        private const string KeyExposure = "Exposure";

        private ApplicationDataContainer _localSettings = ApplicationData.Current.LocalSettings;

        public AppMode AppMode
        {
            get;
            set;
        }

        public Color TargetColor
        {
            get;
            set;
        }

        private double _threshold;
        public double Threshold
        {
            get
            {
                return _threshold;
            }
            set
            {
                _threshold = value;
            }
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

        public bool RemoveNoise
        {
            get;
            set;
        }

        public bool ApplyEffectOnly
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
            if (_localSettings.Values.ContainsKey(KeyAppMode))
            {
                AppMode = (AppMode)Enum.Parse(typeof(AppMode), (string)_localSettings.Values[KeyAppMode]);
            }
            else
            {
                AppMode = AppMode.Photo;
            }

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

            if (_localSettings.Values.ContainsKey(KeyRemoveNoise))
            {
                RemoveNoise = (bool)_localSettings.Values[KeyRemoveNoise];
            }

            if (_localSettings.Values.ContainsKey(KeyApplyEffectOnly))
            {
                ApplyEffectOnly = (bool)_localSettings.Values[KeyApplyEffectOnly];
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
            _localSettings.Values[KeyAppMode] = AppMode.ToString();
            _localSettings.Values[KeyTargetColorR] = TargetColor.R;
            _localSettings.Values[KeyTargetColorG] = TargetColor.G;
            _localSettings.Values[KeyTargetColorB] = TargetColor.B;
            _localSettings.Values[KeyThreshold] = Threshold;
            _localSettings.Values[KeyFlash] = Flash;
            _localSettings.Values[KeyTorch] = Torch;
            _localSettings.Values[KeyMode] = Mode.ToString();
            _localSettings.Values[KeyRemoveNoise] = RemoveNoise;
            _localSettings.Values[KeyApplyEffectOnly] = ApplyEffectOnly;
            _localSettings.Values[KeyIsoSpeedPreset] = IsoSpeedPreset.ToString();
            _localSettings.Values[KeyExposure] = Exposure;
        }
    }
}
