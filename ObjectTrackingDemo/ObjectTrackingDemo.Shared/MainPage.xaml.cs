using System;
using System.ComponentModel;
using System.Threading;
using System.Threading.Tasks;
using Windows.ApplicationModel.Core;
using Windows.Foundation;
using Windows.Media.Capture;
using Windows.UI;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Media.Imaging;
using Windows.UI.Xaml.Navigation;
using Windows.UI.Xaml.Shapes;

namespace ObjectTrackingDemo
{
    public sealed partial class MainPage : Page
    {
        private const string VideoEncodingPropertiesSubTypeYUV2 = "YUY2";
        private const string VideoEncodingPropertiesSubTypeMJPG = "MJPG";
        private const int ColorPickFrameRequestId = 42;

        public Visibility ControlsVisibility
        {
            get
            {
                return (Visibility)GetValue(ControlsVisibilityProperty);
            }
            private set
            {
                SetValue(ControlsVisibilityProperty, value);
            }
        }
        public static readonly DependencyProperty ControlsVisibilityProperty =
            DependencyProperty.Register("ControlsVisibility", typeof(Visibility), typeof(MainPage),
                new PropertyMetadata(Visibility.Visible));

        private VideoEngine _videoEngine;
        private ActionQueue _actionQueue;
        private MediaElement _lockedSound;
        private Timer _hideCapturePhotoImageTimer;
        private Timer _updateInfoTextTimer;
        private Point _viewFinderCanvasTappedPoint;
        private double _screenResolutionX;
        private double _screenResolutionY;

        public MainPage()
        {
            InitializeComponent();

            controlBar.HideButtonClicked += OnHideButtonClicked;
            controlBar.ToggleEffectButtonClicked += OnToggleEffectButtonClickedAsync;
            controlBar.ToggleFlashButtonClicked += OnToggleFlashButtonClicked;

            _videoEngine = VideoEngine.Instance;
            colorPickerControl.CurrentColor = Settings.DefaultTargetColor;
            thresholdSlider.Value = 0;

            NavigationCacheMode = NavigationCacheMode.Required;

            try
            {
                _lockedSound = new MediaElement();
                _lockedSound.Source = new Uri(this.BaseUri, "Assets/tos_keypress1.mp3");
                _lockedSound.AutoPlay = false;
                LayoutGrid.Children.Add(_lockedSound);
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine(ex);
            }

            DisplayUtils displayUtils = new DisplayUtils();
            displayUtils.ResolveScreenResolution(out _screenResolutionX, out _screenResolutionY);
        }

        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            base.OnNavigatedTo(e);

            _actionQueue = new ActionQueue();
            _actionQueue.ExecuteIntervalInMilliseconds = 500;

            ((App)Application.Current)._settings.Load();
            await _videoEngine.InitializeAsync();
            captureElement.Source = _videoEngine.MediaCapture;


            await _videoEngine.StartAsync();
            
            SetColor(((App)Application.Current)._settings.TargetColor, false);
            SetThreshold(((App)Application.Current)._settings.Threshold);
            SetFlash(((App)Application.Current)._settings.Flash, false);

            controlBar.IsEffectOn = _videoEngine.EffectSet;

            colorPickerControl.ColorChanged += OnColorPickerControlColorChanged;
            _videoEngine.StateManager.StateChanged += OnEffectStateChangedAsync;
            _videoEngine.ShowMessageRequest += OnVideoEngineShowMessageRequestAsync;
            _videoEngine.Messenger.FrameCaptured += OnFrameCapturedAsync;
            _videoEngine.Messenger.PostProcessComplete += OnPostProcessCompleteAsync;

            Window.Current.VisibilityChanged += OnVisibilityChangedAsync;

            DataContext = this;
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            _videoEngine.Torch = false;
            _actionQueue.Dispose();
            ((App)Application.Current)._settings.Save();

            colorPickerControl.ColorChanged -= OnColorPickerControlColorChanged;
            _videoEngine.StateManager.StateChanged -= OnEffectStateChangedAsync;
            _videoEngine.ShowMessageRequest -= OnVideoEngineShowMessageRequestAsync;
            _videoEngine.Messenger.FrameCaptured -= OnFrameCapturedAsync;
            _videoEngine.Messenger.PostProcessComplete -= OnPostProcessCompleteAsync;

            Window.Current.VisibilityChanged -= OnVisibilityChangedAsync;

            captureElement.Source = null;
            _videoEngine.DisposeAsync();

            base.OnNavigatedFrom(e);
        }

        /// <summary>
        /// Sets the new target color.
        /// </summary>
        /// <param name="newColor">The new color.</param>
        /// <param name="saveSettings">If true, will save the color to the local storage.</param>
        private void SetColor(Color newColor, bool saveSettings = true)
        {
            colorPickerControl.CurrentColor = newColor;
            ((App)Application.Current)._settings.TargetColor = newColor;

            if (colorPickerControl.Visibility == Visibility.Visible)
            {
                colorPickerControl.PreviewColor = newColor;
            }

            byte[] yuv = ImageProcessingUtils.RGBToYUV(newColor.R, newColor.G, newColor.B);
            _videoEngine.SetYUVAsync(yuv);

            if (saveSettings)
            {
                ((App)Application.Current)._settings.Save();
            }
        }

        /// <summary>
        /// Sets the effect threshold.
        /// </summary>
        /// <param name="threshold">The new threshold value.</param>
        private void SetThreshold(double threshold)
        {
            thresholdSlider.Value = threshold;
        }

        /// <summary>
        /// Sets the flash on/off.
        /// </summary>
        /// <param name="enabled">If true, will try to set the flash on.</param>
        /// <param name="saveSettings">If true, will save the color to the local storage.</param>
        private void SetFlash(bool enabled, bool saveSettings = true)
        {
            _videoEngine.Flash = _videoEngine.Torch = enabled;
            ((App)Application.Current)._settings.Flash = ((App)Application.Current)._settings.Torch = _videoEngine.Torch;

            controlBar.IsFlashOn = _videoEngine.Torch;

            if (saveSettings)
            {
                ((App)Application.Current)._settings.Save();
            }
        }

        /// <summary>
        /// Sets the position and the size of the rectangle to bound the given area
        /// (defined by ObjectDetails instance).
        /// </summary>
        /// <param name="rectangle">The rectangle object.</param>
        /// <param name="rectangleTransform">The transform of the rectangle.</param>
        /// <param name="context">The context i.e. the UI element "inside" which the rectangle is drawn (e.g. capture element).</param>
        /// <param name="objectDetails">The object details defining the area to bound.</param>
        private void SetRectangleSizeAndPositionBasedOnObjectDetails(
            ref Rectangle rectangle, ref TranslateTransform rectangleTransform,
            FrameworkElement context, ObjectDetails objectDetails)
        {
            double scaleX = context.ActualWidth / _videoEngine.ResolutionWidth;
            double scaleY = context.ActualHeight / _videoEngine.ResolutionHeight;

            rectangle.Width = objectDetails.width * scaleX;
            rectangle.Height = objectDetails.height * scaleY;

            rectangleTransform.X = objectDetails.centerX * scaleX - context.ActualWidth / 2;
            rectangleTransform.Y = objectDetails.centerY * scaleY - context.ActualHeight / 2;
        }

        /// <summary>
        /// Converts the given pixel array into a writeable bitmap.
        /// </summary>
        /// <param name="pixelArray">The pixel array.</param>
        /// <param name="width">The width of the image in pixels.</param>
        /// <param name="height">The height of the image in pixels.</param>
        /// <returns></returns>
        private async Task<WriteableBitmap> PixelArrayToWriteableBitmapAsync(byte[] pixelArray, int width, int height)
        {
            string videoRecordFormat = CameraUtils.ResolveVideoRecordFormat(_videoEngine.MediaCapture);

            WriteableBitmap bitmap =
                (videoRecordFormat.Equals(VideoEncodingPropertiesSubTypeYUV2)
                 || videoRecordFormat.Equals(VideoEncodingPropertiesSubTypeMJPG))
                    ? await ImageProcessingUtils.YUY2PixelArrayToWriteableBitmapAsync(pixelArray, width, height)
                    : await ImageProcessingUtils.NV12PixelArrayToWriteableBitmapAsync(pixelArray, width, height);

            return bitmap;
        }

        #region Timer callbacks

        private async void UpdateInfoTextAsync(object state)
        {
            await CoreApplication.MainView.CoreWindow.Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
            {
                infoTextBlock.Text = string.Empty;

                if (_videoEngine.Messenger.OperationDurationInMilliseconds == 0)
                {
                    infoTextBlock.Text = "< 1 ms/op";
                }
                else if (_videoEngine.Messenger.OperationDurationInMilliseconds == VideoEffectMessenger.NotDefined)
                {
                    infoTextBlock.Text = "? ms/op";
                }
                else
                {
                    infoTextBlock.Text = _videoEngine.Messenger.OperationDurationInMilliseconds.ToString() + " ms/op";
                }
            });
        }

        private async void HideCapturedPhotoImageAsync(object state = null)
        {
            await CoreApplication.MainView.CoreWindow.Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
            {
                capturedPhotoImage.Visibility = Visibility.Collapsed;
            });

            if (_hideCapturePhotoImageTimer != null)
            {
                _hideCapturePhotoImageTimer.Dispose();
                _hideCapturePhotoImageTimer = null;
            }
        }

        #endregion

        #region Event handlers

        private async void OnVisibilityChangedAsync(object sender, VisibilityChangedEventArgs e)
        {
            if (e.Visible == true)
            {
                _actionQueue = new ActionQueue();
                _actionQueue.ExecuteIntervalInMilliseconds = 500;

                await _videoEngine.InitializeAsync();
                captureElement.Source = _videoEngine.MediaCapture;
                await _videoEngine.StartAsync();

                controlBar.IsEffectOn = _videoEngine.EffectSet;

                _videoEngine.StateManager.StateChanged += OnEffectStateChangedAsync;
                _videoEngine.Messenger.FrameCaptured += OnFrameCapturedAsync;
                _videoEngine.Messenger.PostProcessComplete += OnPostProcessCompleteAsync;
            }
            else
            {
                _actionQueue.Dispose();
                _actionQueue = null;

                ((App)Application.Current)._settings.Save();

                _videoEngine.StateManager.StateChanged -= OnEffectStateChangedAsync;
                _videoEngine.Messenger.FrameCaptured -= OnFrameCapturedAsync;
                _videoEngine.Messenger.PostProcessComplete -= OnPostProcessCompleteAsync;

                captureElement.Source = null;
                _videoEngine.DisposeAsync();
            }
        }

        private void OnColorPickerControlColorChanged(object sender, Color e)
        {
            SetColor(e);
        }

        private async void OnEffectStateChangedAsync(VideoEffectState newState, VideoEffectState oldState)
        {
#if WINDOWS_APP
            await CoreApplication.MainView.CoreWindow.Dispatcher.RunAsync(CoreDispatcherPriority.Normal, async () =>
#else
            await CoreApplication.MainView.CoreWindow.Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
#endif
                {
                    stateTextBlock.Text = "State: " + newState.ToString();

                    /*if (newState.Equals("Locked") && _lockedSound != null)
                    {
                        _lockedSound.Play();
                    }*/

                    switch (newState)
                    {
                        case VideoEffectState.Idle:
                            progressBar.Visibility = Visibility.Visible;
                            controlBar.IsEffectOn = false;
                            lockedRectangle.Visibility = Visibility.Collapsed;
                            progressBar.Visibility = Visibility.Collapsed;
                            break;

                        case VideoEffectState.Locking:
                            lockedRectangle.Visibility = Visibility.Collapsed;
                            progressBar.Visibility = Visibility.Visible;
                            break;

                        case VideoEffectState.Locked:
                            if (lockedRectangle.Visibility == Visibility.Collapsed)
                            {
                                SetRectangleSizeAndPositionBasedOnObjectDetails(
                                    ref lockedRectangle, ref lockedRectangleTranslateTransform,
                                    captureElement, _videoEngine.Messenger.LockedRect);

                                lockedRectangle.Visibility = Visibility.Visible;
                            }

                            progressBar.Visibility = Visibility.Collapsed;
                            break;

                        case VideoEffectState.Triggered:
#if WINDOWS_APP
                            await _videoEngine.MediaCapture.AddEffectAsync(_videoEngine.RecordMediaStreamType, VideoEngine.BufferEffectActivationId, _videoEngine.Properties);
#endif // WINDOWS_APP
                            lockedRectangle.Visibility = Visibility.Collapsed;
                            progressBar.Visibility = Visibility.Visible;
                            break;

                        case VideoEffectState.PostProcess:
                            progressBar.Visibility = Visibility.Visible;
                            break;

                        default:
                            break;
                    }

                });
        }

        private async void OnFrameCapturedAsync(byte[] pixelArray, int frameWidth, int frameHeight, int frameId)
        {
            _videoEngine.Messenger.FrameCaptured -= OnFrameCapturedAsync;
            System.Diagnostics.Debug.WriteLine("OnFrameCapturedAsync");

            await CoreApplication.MainView.CoreWindow.Dispatcher.RunAsync(CoreDispatcherPriority.Normal, async () =>
                {
                    WriteableBitmap bitmap = await PixelArrayToWriteableBitmapAsync(pixelArray, frameWidth, frameHeight);

                    if (bitmap != null)
                    {
                        capturedPhotoImage.Source = bitmap;
                        bitmap.Invalidate();
                        capturedPhotoImage.Visibility = Visibility.Visible;

                        if (frameId == ColorPickFrameRequestId)
                        {
                            int scaledWidth = (int)(_videoEngine.ResolutionWidth * viewfinderCanvas.ActualHeight / _videoEngine.ResolutionHeight);
                            int pointX = (int)(((_viewFinderCanvasTappedPoint.X - (viewfinderCanvas.ActualWidth - scaledWidth) / 2) / scaledWidth) * frameWidth);
                            int pointY = (int)((_viewFinderCanvasTappedPoint.Y / viewfinderCanvas.ActualHeight) * frameHeight);
                                                      
                            SetColor(ImageProcessingUtils.GetColorAtPoint(
                                bitmap, (uint)frameWidth, (uint)frameHeight, new Point() { X = pointX, Y = pointY }));
                        }

                        if (_hideCapturePhotoImageTimer != null)
                        {
                            _hideCapturePhotoImageTimer.Dispose();
                            _hideCapturePhotoImageTimer = null;
                        }

                        _hideCapturePhotoImageTimer = new Timer(HideCapturedPhotoImageAsync, null, 8000, Timeout.Infinite);
                    }

                    _videoEngine.Messenger.FrameCaptured += OnFrameCapturedAsync;
                });
        }

        private async void OnPostProcessCompleteAsync(
            byte[] pixelArray, int imageWidth, int imageHeight,
            ObjectDetails fromObjectDetails, ObjectDetails toObjectDetails)
        {
            await CoreApplication.MainView.CoreWindow.Dispatcher.RunAsync(CoreDispatcherPriority.Normal, async () =>
            {
                WriteableBitmap bitmap = await PixelArrayToWriteableBitmapAsync(pixelArray, imageWidth, imageHeight);

                if (bitmap != null)
                {
                    processingResultImage.Source = bitmap;
                    bitmap.Invalidate();

                    if (fromObjectDetails.centerX > toObjectDetails.centerX) // TODO This is wrong, but the problem is in BufferEffect
                    {
                        imageLeftSideLabel.Text = "1";
                        imageRightSideLabel.Text = "2";
                    }
                    else
                    {
                        imageLeftSideLabel.Text = "2";
                        imageRightSideLabel.Text = "1";
                    }

                    if (imageWidth > 0)
                    {
                        SetRectangleSizeAndPositionBasedOnObjectDetails(
                            ref firstRectangle, ref firstRectangleTranslateTransform,
                            processingResultImage, fromObjectDetails);

                        SetRectangleSizeAndPositionBasedOnObjectDetails(
                            ref secondRectangle, ref secondRectangleTranslateTransform,
                            processingResultImage, toObjectDetails);
                    }

                    processingResultGrid.Visibility = Visibility.Visible;
                }
            });

        }

        private void OnHideButtonClicked(object sender, RoutedEventArgs e)
        {
            ControlsVisibility = Visibility.Collapsed;
        }

        private async void OnToggleEffectButtonClickedAsync(object sender, RoutedEventArgs e)
        {
            controlBar.IsEffectOn = await _videoEngine.ToggleEffectAsync();

            if (_updateInfoTextTimer != null)
            {
                _updateInfoTextTimer.Dispose();
                _updateInfoTextTimer = null;
            }

            infoTextBlock.Text = "Hello there!";

            if (_videoEngine.EffectSet)
            {
                await _videoEngine.SetThresholdAsync((float)thresholdSlider.Value);
                _updateInfoTextTimer = new Timer(UpdateInfoTextAsync, null, 1000, 1000);
            }

            ((App)Application.Current)._settings.Save();
        }

        private void OnToggleFlashButtonClicked(object sender, RoutedEventArgs e)
        {
            SetFlash(!_videoEngine.Torch);
        }

        private void OnSliderValueChanged(object sender, RangeBaseValueChangedEventArgs e)
        {
            double newThreshold = e.NewValue;

            if (_videoEngine != null && _videoEngine.EffectSet)
            {
                _actionQueue.Execute(async () =>
                    {
                        await CoreApplication.MainView.CoreWindow.Dispatcher.RunAsync(CoreDispatcherPriority.Normal, async () =>
                            {
                                await _videoEngine.SetThresholdAsync((float)newThreshold);
                            });
                    }
                 );
            }

            ((App)Application.Current)._settings.Threshold = newThreshold;
            thresholdTextBlock.Text = "Threshold: " + newThreshold;
        }

        /// <summary>
        /// Picks and sets the color from the point, which was tapped.
        /// However, if controls were hidden, their visibility is restored but no color is picked.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void OnViewfinderCanvasTapped(object sender, TappedRoutedEventArgs e)
        {
            if (ControlsVisibility == Visibility.Collapsed)
            {
                ControlsVisibility = Visibility.Visible;
                return;
            }

            _viewFinderCanvasTappedPoint = e.GetPosition(viewfinderCanvas);
            _videoEngine.Messenger.RequestFrame(ColorPickFrameRequestId);
        }

        private void OnColorPickerButtonClicked(object sender, RoutedEventArgs e)
        {
            colorPickerControl.PreviewColor = ((App)Application.Current)._settings.TargetColor;
            colorPickerControl.Visibility = Visibility.Visible;
        }

        private async void OnVideoEngineShowMessageRequestAsync(object sender, string message)
        {
            await CoreApplication.MainView.CoreWindow.Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
            {
                infoTextBlock.Text = message;
            });
        }

        private void OnProcessingResultGridTapped(object sender, Windows.UI.Xaml.Input.TappedRoutedEventArgs e)
        {
            processingResultGrid.Visibility = Windows.UI.Xaml.Visibility.Collapsed;
        }

        #endregion
    }
}
