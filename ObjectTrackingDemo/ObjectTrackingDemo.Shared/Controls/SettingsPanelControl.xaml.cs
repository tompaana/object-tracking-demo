using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Media.Devices;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;

#if WINDOWS_PHONE_APP
using Windows.Phone.UI.Input;
#endif

namespace ObjectTrackingDemo
{
    public sealed partial class SettingsPanelControl : UserControl
    {
        public event EventHandler<Mode> ModeChanged;
        public event EventHandler<bool> RemoveNoiseChanged;
        public event EventHandler<bool> ApplyEffectOnlyChanged;
        public event EventHandler<IsoSpeedPreset> IsoChanged;
        public event EventHandler<int> ExposureChanged;
        public event EventHandler<Visibility> VisibilityChanged;

        private Settings _settings;
        private int _exposureStep = VideoEngine.ExposureAutoValue; // -1 == auto, 0 == min, 1..200 normal range
        bool _isInitialized;

        public bool AreCameraSettingsVisible
        {
            get
            {
                return (bool)GetValue(AreCameraSettingsVisibleProperty);
            }
            set
            {
                SetValue(AreCameraSettingsVisibleProperty, value);
            }
        }
        private static readonly DependencyProperty AreCameraSettingsVisibleProperty =
            DependencyProperty.Register("AreCameraSettingsVisible", typeof(bool), typeof(SettingsPanelControl),
                new PropertyMetadata(true, OnAreCameraSettingsVisiblePropertyChanged));

        private static void OnAreCameraSettingsVisiblePropertyChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            SettingsPanelControl control = d as SettingsPanelControl;

            if (control != null)
            {
                Visibility visibility = (bool)e.NewValue ? Visibility.Visible : Visibility.Collapsed;
                control.isoGrid.Visibility = visibility;
                control.exposureGrid.Visibility = visibility;
            }
        }

        public SettingsPanelControl()
        {
            this.InitializeComponent();

            _settings = App.Settings;

            if (modeSelectorListBox.Items.Count == 0)
            {
                foreach (string enumAsString in Enum.GetNames(typeof(Mode)).ToList())
                {
                    modeSelectorListBox.Items.Add(enumAsString);
                }

                if (modeSelectorListBox.Items.Count > 0)
                {
                    modeSelectorListBox.SelectedIndex =
                        modeSelectorListBox.Items.IndexOf(_settings.Mode.ToString());
                }
            }

            removeNoiseCheckBox.IsChecked = _settings.RemoveNoise;
            applyEffectOnlyCheckBox.IsChecked = _settings.ApplyEffectOnly;
            _isInitialized = true;
        }

#if WINDOWS_PHONE_APP
        private void OnBackPressed(object sender, BackPressedEventArgs e)
        {
            if (Visibility == Visibility.Visible)
            {
                Hide();
                e.Handled = true;
            }
            else
            {
                throw new InvalidOperationException("This should not happen!");
            }
        }
#endif

        public void Show()
        {
            if (modeSelectorListBox.Items.Count == 0)
            {
                foreach (string enumAsString in Enum.GetNames(typeof(Mode)).ToList())
                {
                    modeSelectorListBox.Items.Add(enumAsString);
                }

                if (modeSelectorListBox.Items.Count > 0)
                {
                    modeSelectorListBox.SelectedIndex =
                        modeSelectorListBox.Items.IndexOf(_settings.Mode.ToString());
                }
            }

            if (AreCameraSettingsVisible)
            {
                VideoEngine videoEngine = VideoEngine.Instance;

                if (videoEngine.IsIsoSupported)
                {
                    isoGrid.Visibility = Visibility.Visible;

                    if (isoSelectorListBox.Items.Count == 0)
                    {
                        IReadOnlyList<IsoSpeedPreset> supportedIsoSpeedPresets =
                            _settings.SupportedIsoSpeedPresets;

                        if (supportedIsoSpeedPresets != null)
                        {
                            foreach (IsoSpeedPreset element in supportedIsoSpeedPresets)
                            {
                                isoSelectorListBox.Items.Add(element.ToString());
                            }

                            isoSelectorListBox.SelectedIndex =
                                isoSelectorListBox.Items.IndexOf(_settings.IsoSpeedPreset.ToString());
                        }
                    }
                }
                else
                {
                    isoGrid.Visibility = Visibility.Collapsed;
                }

                if (videoEngine.IsExposureSupported)
                {
                    exposureGrid.Visibility = Visibility.Visible;
                    exposureSlider.Minimum = videoEngine.ExposureMinStep;
                    exposureSlider.Maximum = videoEngine.ExposureMaxStep;
                    _exposureStep = _settings.Exposure;

                    if (_exposureStep < videoEngine.ExposureMinStep || _exposureStep > videoEngine.ExposureMaxStep)
                    {
                        _exposureStep = VideoEngine.ExposureAutoValue;
                    }

                    exposureSlider.Value = _exposureStep;
                    OnExposureChanged();
                }
                else
                {
                    exposureGrid.Visibility = Visibility.Collapsed;
                }
            }
            else
            {
                isoGrid.Visibility = Visibility.Collapsed;
                exposureGrid.Visibility = Visibility.Collapsed;
            }

            Visibility = Visibility.Visible;

#if WINDOWS_PHONE_APP
            HardwareButtons.BackPressed += OnBackPressed;
#endif

            if (VisibilityChanged != null)
            {
                VisibilityChanged(this, Visibility.Visible);
            }
        }

        public void Hide()
        {
            Visibility = Visibility.Collapsed;

#if WINDOWS_PHONE_APP
            HardwareButtons.BackPressed -= OnBackPressed;
#endif

            if (VisibilityChanged != null)
            {
                VisibilityChanged(this, Visibility.Collapsed);
            }
        }

        private void OnExposureChanged()
        {
            if (_exposureStep == -1)
            {
                exposureHeaderTextBlock.Text = "Exposure: Auto";
            }
            else if (_exposureStep == 0)
            {
                exposureHeaderTextBlock.Text = "Exposure: Min";
            }
            else if (_exposureStep == 200)
            {
                exposureHeaderTextBlock.Text = "Exposure: Max";
            }
            else
            {
                exposureHeaderTextBlock.Text = "Exposure: " + _exposureStep.ToString();
            }

            _settings.Exposure = _exposureStep;
            _settings.Save();

            if (ExposureChanged != null)
            {
                ExposureChanged(this, _exposureStep);
            }
        }

        private void OnModeSelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            string selectedModeAsString = modeSelectorListBox.SelectedItem.ToString();
            Mode selectedMode = (Mode)Enum.Parse(typeof(Mode), selectedModeAsString);

            if (_isInitialized)
            {
                _settings.Mode = selectedMode;
                _settings.Save();

                if (ModeChanged != null)
                {
                    ModeChanged(this, selectedMode);
                }
            }
        }

        private void OnRemoveNoiseCheckBoxCheckedChanged(object sender, RoutedEventArgs e)
        {
            if (_isInitialized)
            {
                _settings.RemoveNoise = (bool)removeNoiseCheckBox.IsChecked;
                _settings.Save();

                if (RemoveNoiseChanged != null)
                {
                    RemoveNoiseChanged(this, (bool)removeNoiseCheckBox.IsChecked);
                }
            }
        }

        private void OnApplyEffectOnlyCheckBoxCheckedChanged(object sender, RoutedEventArgs e)
        {
            if (_isInitialized)
            {
                _settings.ApplyEffectOnly = (bool)applyEffectOnlyCheckBox.IsChecked;
                _settings.Save();

                if (ApplyEffectOnlyChanged != null)
                {
                    ApplyEffectOnlyChanged(this, (bool)applyEffectOnlyCheckBox.IsChecked);
                }
            }
        }

        private void OnIsoSelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (isoSelectorListBox.SelectedItem != null)
            {
                string selectedIsoSpeedPresetAsString = isoSelectorListBox.SelectedItem.ToString();

                if (_isInitialized)
                {
                    _settings.IsoSpeedPreset = (IsoSpeedPreset)Enum.Parse(typeof(IsoSpeedPreset), selectedIsoSpeedPresetAsString);
                    _settings.Save();

                    if (IsoChanged != null)
                    {
                        IsoChanged(this, _settings.IsoSpeedPreset);
                    }
                }
            }
        }

        private void OnExposureSliderValueChanged(object sender, RangeBaseValueChangedEventArgs e)
        {
            _exposureStep = (int)exposureSlider.Value;
            OnExposureChanged();
        }

        private void OnAutoExposureButtonClicked(object sender, RoutedEventArgs e)
        {
            _exposureStep = VideoEngine.ExposureAutoValue;
            OnExposureChanged();
        }
    }
}
