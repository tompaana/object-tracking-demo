using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;

namespace ObjectTrackingDemo
{
    public sealed partial class ControlBar : UserControl
    {
        private const double ButtonOnOpacity = 1.0d;
        private const double ButtonOffOpacity = 0.4d;

        public event EventHandler<RoutedEventArgs> HideButtonClicked;
        public event EventHandler<RoutedEventArgs> ToggleEffectButtonClicked;
        public event EventHandler<RoutedEventArgs> ToggleFlashButtonClicked;
        public event EventHandler<RoutedEventArgs> SettingsButtonClicked;

        public bool IsToggleFlashButtonEnabled
        {
            get
            {
                return (bool)GetValue(IsToggleFlashButtonEnabledProperty);
            }
            set
            {
                SetValue(IsToggleFlashButtonEnabledProperty, value);
            }
        }
        private static readonly DependencyProperty IsToggleFlashButtonEnabledProperty =
            DependencyProperty.Register("IsToggleFlashButtonEnabled", typeof(bool), typeof(ControlBar),
                new PropertyMetadata(true));

        public bool IsEffectOn
        {
            get
            {
                return (bool)GetValue(IsEffectOnProperty);
            }
            set
            {
                SetValue(IsEffectOnProperty, value);
            }
        }
        private static readonly DependencyProperty IsEffectOnProperty =
            DependencyProperty.Register("IsEffectOn", typeof(bool), typeof(ControlBar),
                new PropertyMetadata(false, OnIsEffectOnPropertyChanged));

        private static void OnIsEffectOnPropertyChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            ControlBar control = d as ControlBar;
            bool value = (bool)e.NewValue;

            if (control != null)
            {
                control.effectButtonIcon.Opacity = value ? ButtonOnOpacity : ButtonOffOpacity;
            }
        }

        public bool IsFlashOn
        {
            get
            {
                return (bool)GetValue(IsFlashOnProperty);
            }
            set
            {
                SetValue(IsFlashOnProperty, value);
            }
        }
        private static readonly DependencyProperty IsFlashOnProperty =
            DependencyProperty.Register("IsFlashOn", typeof(bool), typeof(ControlBar),
                new PropertyMetadata(false, OnIsFlashOnPropertyChanged));

        private static void OnIsFlashOnPropertyChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            ControlBar control = d as ControlBar;
            bool value = (bool)e.NewValue;

            if (control != null)
            {
                control.flashButtonIcon.Opacity = value ? ButtonOnOpacity : ButtonOffOpacity;
            }
        }

        public Visibility SettingsButtonVisibility
        {
            get
            {
                return (Visibility)GetValue(SettingsButtonVisibilityProperty);
            }
            set
            {
                SetValue(SettingsButtonVisibilityProperty, value);
            }
        }
        private static readonly DependencyProperty SettingsButtonVisibilityProperty =
            DependencyProperty.Register("SettingsButtonVisibility", typeof(Visibility), typeof(ControlBar),
                new PropertyMetadata(Visibility.Visible));

        public Visibility FlashButtonVisibility
        {
            get
            {
                return (Visibility)GetValue(FlashButtonVisibilityProperty);
            }
            set
            {
                SetValue(FlashButtonVisibilityProperty, value);
            }
        }
        private static readonly DependencyProperty FlashButtonVisibilityProperty =
            DependencyProperty.Register("FlashButtonVisibility", typeof(Visibility), typeof(ControlBar),
                new PropertyMetadata(Visibility.Visible));

        public ControlBar()
        {
            this.InitializeComponent();

            effectButtonIcon.Opacity = IsEffectOn ? ButtonOnOpacity : ButtonOffOpacity;
            flashButtonIcon.Opacity = IsFlashOn ? ButtonOnOpacity : ButtonOffOpacity;
        }

        private void OnHideButtonClicked(object sender, RoutedEventArgs e)
        {
            if (HideButtonClicked != null)
            {
                HideButtonClicked(sender, e);
            }
        }

        private void OnToggleEffectButtonClicked(object sender, RoutedEventArgs e)
        {
            if (ToggleEffectButtonClicked != null)
            {
                ToggleEffectButtonClicked(sender, e);
            }
        }

        private void OnToggleFlashButtonClicked(object sender, RoutedEventArgs e)
        {
            if (ToggleFlashButtonClicked != null)
            {
                ToggleFlashButtonClicked(sender, e);
            }
        }

        private void OnSettingsButtonClicked(object sender, RoutedEventArgs e)
        {
            if (SettingsButtonClicked != null)
            {
                SettingsButtonClicked(sender, e);
            }
        }
    }
}
