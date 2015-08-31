using ObjectTrackingDemo.Common;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.ApplicationModel.Core;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Graphics.Display;
using Windows.UI;
using Windows.UI.Core;
using Windows.UI.ViewManagement;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Media.Imaging;
using Windows.UI.Xaml.Navigation;

#if WINDOWS_PHONE_APP
using Windows.ApplicationModel.Activation;
#endif

using VideoEffect;


namespace ObjectTrackingDemo
{
    /// <summary>
    /// 
    /// </summary>
    public sealed partial class PhotoPage : Page, IFileOpenPickerContinuable
    {
        private const int MaxFrameCount = 2;

        private CPhotoProcessor _photoProcessor;
        private ActionQueue _actionQueue;
        private Settings _settings;
        private WriteableBitmap _processedPhotoAsWriteableBitmap;
        private byte[][] _yuvPixelArray;
        private int[] _imageWidthInPixels;
        private int[] _imageHeightInPixels;
        private int _imageIndex;
        private bool _isProcessing;

        private NavigationHelper _navigationHelper;
        /// <summary>
        /// Gets the <see cref="NavigationHelper"/> associated with this <see cref="Page"/>.
        /// </summary>
        public NavigationHelper NavigationHelper
        {
            get { return this._navigationHelper; }
        }

        public PhotoPage()
        {
            this.InitializeComponent();

            this._navigationHelper = new NavigationHelper(this);

            _photoProcessor = new CPhotoProcessor();
            _settings = App.Settings;
            _yuvPixelArray = new byte[MaxFrameCount][];
            _imageWidthInPixels = new int[MaxFrameCount];
            _imageHeightInPixels = new int[MaxFrameCount];

            colorPickerControl.CurrentColor = _settings.TargetColor;

            _actionQueue = new ActionQueue();
            _actionQueue.ExecuteIntervalInMilliseconds = 50;

            saveImageButton.IsEnabled = false;
        }

        #region NavigationHelper registration

        /// <summary>
        /// The methods provided in this section are simply used to allow
        /// NavigationHelper to respond to the page's navigation methods.
        /// <para>
        /// Page specific logic should be placed in event handlers for the  
        /// <see cref="NavigationHelper.LoadState"/>
        /// and <see cref="NavigationHelper.SaveState"/>.
        /// The navigation parameter is available in the LoadState method 
        /// in addition to page state preserved during an earlier session.
        /// </para>
        /// </summary>
        /// <param name="e">Provides data for navigation methods and event
        /// handlers that cannot cancel the navigation request.</param>
        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            this._navigationHelper.OnNavigatedTo(e);
            settingsPanelControl.ModeChanged += OnModeChanged;
            settingsPanelControl.RemoveNoiseChanged += OnRemoveNoiseChanged;
            settingsPanelControl.ApplyEffectOnlyChanged += OnApplyEffectOnlyChanged;
            _photoProcessor.SetMode((int)_settings.Mode);
            _photoProcessor.SetThreshold((float)_settings.Threshold);
            SetColor(_settings.TargetColor, false);
            effectSettingsControl.SliderValue = _settings.Threshold;
            colorPickerControl.ColorChanged += OnColorPickerControlColorChanged;
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            this._navigationHelper.OnNavigatedFrom(e);
            settingsPanelControl.ModeChanged -= OnModeChanged;
            settingsPanelControl.RemoveNoiseChanged -= OnRemoveNoiseChanged;
            settingsPanelControl.ApplyEffectOnlyChanged -= OnApplyEffectOnlyChanged;
            colorPickerControl.ColorChanged -= OnColorPickerControlColorChanged;
        }

        #endregion

#if WINDOWS_PHONE_APP
        public void ContinueFileOpenPicker(FileOpenPickerContinuationEventArgs args)
        {
            ImageFileManager.Instance.ContinueFileOpenPickerAsync(args);
        }
#endif

        /// <summary>
        /// Sets the new target color.
        /// </summary>
        /// <param name="newColor">The new color.</param>
        /// <param name="saveSettings">If true, will save the color to the local storage.</param>
        private void SetColor(Color newColor, bool saveSettings = true)
        {
            colorPickerControl.CurrentColor = newColor;
            _settings.TargetColor = newColor;

            if (colorPickerControl.Visibility == Visibility.Visible)
            {
                colorPickerControl.PreviewColor = newColor;
            }

            byte[] yuv = ImageProcessingUtils.RgbToYuv(newColor.R, newColor.G, newColor.B);

            _photoProcessor.SetTargetYUV(new float[]
                {
                    (float)yuv[0],
                    (float)yuv[1],
                    (float)yuv[2]
                });

            if (saveSettings)
            {
                _settings.Save();
            }

            ApplyEffectAsync();
        }

        private void StoreLoadedImage(ImageData imageData)
        {
            progressBar.Visibility = Visibility.Visible;

            BitmapImage bitmapImage = ImageProcessingUtils.ImageMemoryStreamToBitmapImage(imageData.ImageMemoryStream);

            if (_imageIndex == 0)
            {
                thumbnailImage1.Source = bitmapImage;
            }
            else
            {
                thumbnailImage2.Source = bitmapImage;
            }

            _imageWidthInPixels[_imageIndex] = imageData.ImageWidthInPixels;
            _imageHeightInPixels[_imageIndex] = imageData.ImageHeightInPixels;
            byte[] rgbPixelArray = ImageProcessingUtils.ImageMemoryStreamToRgbPixelArray(imageData.ImageMemoryStream);
            _yuvPixelArray[_imageIndex] = ImageProcessingUtils.RgbPixelArrayToYuy2PixelArray(rgbPixelArray, _imageWidthInPixels[_imageIndex], _imageHeightInPixels[_imageIndex]);

            _photoProcessor.SetFrame(
                _yuvPixelArray[_imageIndex],
                _imageWidthInPixels[_imageIndex], _imageHeightInPixels[_imageIndex],
                _imageIndex);

            progressBar.Visibility = Visibility.Collapsed;
        }

        private async void ApplyEffectAsync()
        {
            await CoreApplication.MainView.CoreWindow.Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
            {
                progressBar.Visibility = Visibility.Visible;
                saveImageButton.IsEnabled = false;
            });

            if (_isProcessing)
            {
                _actionQueue.Execute(() => ApplyEffectAsync());
            }
            else
            {
                await Task.Factory.StartNew(() => ApplyEffectInternalAsync());
            }

            await CoreApplication.MainView.CoreWindow.Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
            {
                progressBar.Visibility = Visibility.Collapsed;

                if (_processedPhotoAsWriteableBitmap != null)
                {
                    saveImageButton.IsEnabled = true;
                }
            });
        }

        internal async void ApplyEffectInternalAsync()
        {
            _isProcessing = true;
            _processedPhotoAsWriteableBitmap = null;
            byte[] yuvPixelArray = null;

            try
            {
                yuvPixelArray = _photoProcessor.ProcessFrames(0, _settings.RemoveNoise, _settings.ApplyEffectOnly);
            }
            catch (Exception e)
            {
                System.Diagnostics.Debug.WriteLine(e.Message);
            }

            if (yuvPixelArray != null)
            {
                await CoreApplication.MainView.CoreWindow.Dispatcher.RunAsync(CoreDispatcherPriority.Normal, async () =>
                {
                    _processedPhotoAsWriteableBitmap =
                        await ImageProcessingUtils.Yuy2PixelArrayToWriteableBitmapAsync(
                            yuvPixelArray, _imageWidthInPixels[0], _imageHeightInPixels[0]);

                    pageImage.Source = _processedPhotoAsWriteableBitmap;
                    _processedPhotoAsWriteableBitmap.Invalidate();
                    _isProcessing = false;
                });
            }
            else
            {
                _isProcessing = false;
            }
        }

        private void OnModeChanged(object sender, Mode e)
        {
            _settings.Mode = e;
            _photoProcessor.SetMode((int)e);
            ApplyEffectAsync();
            _settings.Save();
        }

        private void OnRemoveNoiseChanged(object sender, bool e)
        {
            ApplyEffectAsync();
        }

        private void OnApplyEffectOnlyChanged(object sender, bool e)
        {
            ApplyEffectAsync();
        }

        private void OnColorButtonClicked(object sender, RoutedEventArgs e)
        {
            colorPickerControl.PreviewColor = _settings.TargetColor;
            colorPickerControl.Visibility = Visibility.Visible;
        }

        private void OnSliderValueChanged(object sender, RangeBaseValueChangedEventArgs e)
        {
            double newThreshold = e.NewValue;
            _settings.Threshold = newThreshold;
            _photoProcessor.SetThreshold((float)newThreshold);
            ApplyEffectAsync();
            _settings.Save();
        }

        private void OnColorPickerControlColorChanged(object sender, Color e)
        {
            SetColor(e);
        }

        private void OnImageFileLoadedResult(object sender, bool wasSuccessful)
        {
            System.Diagnostics.Debug.WriteLine("OnImageFileLoadedResult(): " + wasSuccessful);
            ImageFileManager imageFileManager = ImageFileManager.Instance;

#if WINDOWS_PHONE_APP
            imageFileManager.ImageFileLoadedResult -= OnImageFileLoadedResult;
#endif

            if (wasSuccessful)
            {
                StoreLoadedImage(imageFileManager.ImageData);
                ApplyEffectAsync();
            }
        }

        private void OnPageImageTapped(object sender, TappedRoutedEventArgs e)
        {
        }

        private void OnSwitchToCameraButtonClicked(object sender, RoutedEventArgs e)
        {
            if (Frame.BackStack.Count == 0)
            {
                App.Settings.AppMode = AppMode.Camera;
                App.Settings.Save();
                Frame.Navigate(typeof(CameraPage));
            }
            else
            {
                NavigationHelper.GoBack();
            }   
        }

        private async void OnLoadImageButtonClickedAsync(object sender, RoutedEventArgs e)
        {
            _imageIndex = (sender == loadImage1Button) ? 0 : 1;

            ImageFileManager imageFileManager = ImageFileManager.Instance;
            bool success = await imageFileManager.GetImageFileAsync();

            if (success)
            {
#if WINDOWS_PHONE_APP
                imageFileManager.ImageFileLoadedResult += OnImageFileLoadedResult;
#else
                OnImageFileLoadedResult(this, success);
#endif
            }
        }

        private async void OnSaveImageButtonClickedAsync(object sender, RoutedEventArgs e)
        {
            if (_processedPhotoAsWriteableBitmap != null)
            {
                await ImageFileManager.Instance.SaveImageFileAsync(_processedPhotoAsWriteableBitmap, "OTD_");
            }
        }

        private void OnThumbnailImageTapped(object sender, TappedRoutedEventArgs e)
        {
            object loadImageButton = (sender == thumbnailImage1) ? loadImage1Button : loadImage2Button;
            OnLoadImageButtonClickedAsync(loadImageButton, null);
        }
    }
}
