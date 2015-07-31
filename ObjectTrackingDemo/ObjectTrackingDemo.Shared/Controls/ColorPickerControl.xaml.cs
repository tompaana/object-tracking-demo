using System;
using Windows.Foundation;
using Windows.Storage.Streams;
using Windows.UI;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Media.Imaging;

namespace ObjectTrackingDemo
{
    public sealed partial class ColorPickerControl : UserControl
    {
        public event EventHandler<Color> ColorChanged;
        private Color _originalColor;
        private bool _brightnessSliderValueChangedByPickedColor;

        public Color CurrentColor
        {
            get { return (Color)this.GetValue(CurrentColorProperty); }
            set { this.SetValue(CurrentColorProperty, value); }
        }
        public static readonly DependencyProperty CurrentColorProperty =
            DependencyProperty.Register("CurrentColor", typeof(Color), typeof(ColorPickerControl),
            new PropertyMetadata(Color.FromArgb(0xff, 0x0, 0x0, 0x0), OnCurrentColorPropertyChanged));

        private static void OnCurrentColorPropertyChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            ColorPickerControl control = d as ColorPickerControl;

            if (control != null)
            {
                Color newColor = (Color)e.NewValue;
                control._originalColor = newColor;
                control.currentColorGrid.Background = new SolidColorBrush(newColor);
            }
        }

        public Color PreviewColor
        {
            get { return (Color)this.GetValue(PreviewColorProperty); }
            set { this.SetValue(PreviewColorProperty, value); }
        }
        public static readonly DependencyProperty PreviewColorProperty =
            DependencyProperty.Register("PreviewColor", typeof(Color), typeof(ColorPickerControl),
            new PropertyMetadata(Color.FromArgb(0xff, 0x0, 0x0, 0x0), OnPreviewColorPropertyChanged));

        private static void OnPreviewColorPropertyChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            ColorPickerControl control = d as ColorPickerControl;

            if (control != null)
            {
                Color newColor = (Color)e.NewValue;
                control.brightnessSlider.Value = ImageProcessingUtils.BrightnessFromColor(newColor);
                control.previewColorGrid.Background = new SolidColorBrush(newColor);
            }
        }

        public ColorPickerControl()
        {
            this.InitializeComponent();
        }

        private async void OnColorMapImageTapped(object sender, TappedRoutedEventArgs e)
        {
            RenderTargetBitmap bitmap = new RenderTargetBitmap();
            await bitmap.RenderAsync(colorMapImage);

            uint width = (uint)bitmap.PixelWidth;
            uint height = (uint)bitmap.PixelHeight;

            Point point = e.GetPosition(colorMapImage);
            int pointX = (int)((double)(point.X / colorMapImage.ActualWidth) * width);
            int pointY = (int)((double)(point.Y / colorMapImage.ActualHeight) * height);

            System.Diagnostics.Debug.WriteLine("Picked point is [" + (int)point.X + "; " + (int)point.Y
                + "], size of the color map image is "
                + (int)colorMapImage.ActualWidth + "x" + (int)colorMapImage.ActualHeight
                + " => point in image is [" + pointX + "; " + pointY + "]");

            IBuffer pixelBuffer = await bitmap.GetPixelsAsync();

            _brightnessSliderValueChangedByPickedColor = true;
            PreviewColor = ImageProcessingUtils.GetColorAtPoint(pixelBuffer, width, height, new Point() { X = pointX, Y = pointY });
            _originalColor = PreviewColor;
        }

        private void OnBrightnessSliderValueChanged(object sender, RangeBaseValueChangedEventArgs e)
        {
            if (!_brightnessSliderValueChangedByPickedColor)
            {
                PreviewColor = ImageProcessingUtils.ApplyBrightnessToColor(_originalColor, e.NewValue);
            }

            _brightnessSliderValueChangedByPickedColor = false;
        }

        private void OnCloseButtonClicked(object sender, RoutedEventArgs e)
        {
            if (ColorChanged != null)
            {
                ColorChanged(this, PreviewColor);
            }

            Visibility = Visibility.Collapsed;
        }
    }
}
