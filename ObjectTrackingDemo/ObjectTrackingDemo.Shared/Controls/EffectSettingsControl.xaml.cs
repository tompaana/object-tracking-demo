using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;

namespace ObjectTrackingDemo
{
    public sealed partial class EffectSettingsControl : UserControl
    {
        public event RoutedEventHandler ColorButtonClicked
        {
            add
            {
                colorPickerButton.Click += value;
            }
            remove
            {
                colorPickerButton.Click -= value;
            }
        }
        public event RangeBaseValueChangedEventHandler SliderValueChanged
        {
            add
            {
                thresholdSlider.ValueChanged += value;
            }
            remove
            {
                thresholdSlider.ValueChanged -= value;
            }
        }

        public Color ColorButtonColor
        {
            get
            {
                return (Color)GetValue(ColorButtonColorProperty);
            }
            set
            {
                SetValue(ColorButtonColorProperty, value);
            }
        }
        private static readonly DependencyProperty ColorButtonColorProperty =
            DependencyProperty.Register("ColorButtonColor", typeof(Color), typeof(EffectSettingsControl),
                new PropertyMetadata(Color.FromArgb(0xff, 0x0, 0x0, 0x0)));

        public double SliderValue
        {
            get
            {
                return thresholdSlider.Value;
            }
            set
            {
                thresholdSlider.Value = value;
            }
        }

        public EffectSettingsControl()
        {
            this.InitializeComponent();
            thresholdSlider.Value = 0;
        }

        public void SetSliderText(string text)
        {
            thresholdTextBlock.Text = text;
        }
    }
}
