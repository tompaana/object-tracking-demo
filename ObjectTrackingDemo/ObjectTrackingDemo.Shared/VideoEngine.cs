using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;
using Windows.Foundation.Collections;
using Windows.Media.Capture;
using Windows.Media.Devices;
using Windows.Media.MediaProperties;
using Windows.Storage.Streams;
using VideoEffect;

namespace ObjectTrackingDemo
{
    /// <summary>
    /// This class manages the device camera and the native effects.
    /// </summary>
    public class VideoEngine
    {
        // Constants
        public const string EffectActivationId = "ObjectFinderEffectTransform.ObjectFinderEffectEffect";
        public const string BufferEffectActivationId = "BufferEffect.BufferEffect";
        public readonly MediaStreamType PreviewMediaStreamType = MediaStreamType.VideoPreview;
        public readonly MediaStreamType RecordMediaStreamType = MediaStreamType.VideoRecord;

        private const string PropertyKeyThreshold = "Threshold";
        private const string PropertyKeyY = "PropertyY";
        private const string PropertyKeyU = "PropertyU";
        private const string PropertyKeyV = "PropertyV";
        private const string propertyKeyCommunication = "Communication";
        private const int MinimumThreshold = 0;
        private const int MaximumThreshold = 255;
        private const uint PreviewFrameRate = 30;
        private const uint MaximumVideoWidth = 1280;

        public event EventHandler<string> ShowMessageRequest;

        private InMemoryRandomAccessStream _recordingStream = new InMemoryRandomAccessStream();
        private VideoDeviceController _videoDeviceController;
        private DeviceInformation _deviceInformation;
        private DeviceInformationCollection _deviceInformationCollection;
        private VideoEncodingProperties _hfrVideoEncodingProperties;
        private float[] _yuv;
        private bool _changingThreshold;

        private static VideoEngine _instance;
        /// <summary>
        /// The singleton instance of this class.
        /// </summary>
        public static VideoEngine Instance
        {
            get
            {
                if (_instance == null)
                {
                    _instance = new VideoEngine();
                }

                return _instance;
            }
        }

        #region Properties

        public MediaCapture MediaCapture
        {
            get;
            private set;
        }

        public VideoEffectMessenger Messenger
        {
            get;
            private set;
        }

        public StateManager StateManager
        {
            get;
            private set;
        }

        public bool Flash
        {
            get
            {
                if (_videoDeviceController != null)
                {
                    return _videoDeviceController.FlashControl.Enabled;
                }

                return false;
            }
            set
            {
                if (_videoDeviceController != null && _videoDeviceController.FlashControl.Supported)
                {
                    try
                    {
                        _videoDeviceController.FlashControl.Enabled = value;
                    }
                    catch (Exception e)
                    {
                        System.Diagnostics.Debug.WriteLine("Cannot change flash settings: " + e.ToString());
                    }
                }
            }
        }

        public bool Torch
        {
            get
            {
                if (_videoDeviceController != null)
                {
                    try
                    {
                        return _videoDeviceController.TorchControl.Enabled;
                    }
                    catch (Exception)
                    {
                        return false;
                    }
                }

                return false;
            }
            set
            {
                if (_videoDeviceController != null && _videoDeviceController.TorchControl.Supported)
                {
                    try
                    {
                        _videoDeviceController.TorchControl.Enabled = value;
                    }
                    catch (Exception e)
                    {
                        System.Diagnostics.Debug.WriteLine("Cannot change torch settings: " + e.ToString());
                    }
                }
            }
        }

        public bool Initialized
        {
            get;
            private set;
        }

        public bool Started
        {
            get
            {
                return (PreviewStarted && RecordingStarted);
            }
        }

        public bool PreviewStarted
        {
            get;
            private set;
        }

        public bool RecordingStarted
        {
            get;
            private set;
        }

        public bool EffectSet
        {
            get
            {
                return StateManager.State != VideoEffectState.Idle;
            }
        }

        public int ResolutionWidth
        {
            get;
            private set;
        }
   
        public int ResolutionHeight
        { 
            get;
            private set;
        }

        public PropertySet Properties
        {
            get;
            private set;
        }

        #endregion

        /// <summary>
        /// Constructor
        /// </summary>
        private VideoEngine()
        {
            StateManager = new StateManager();
            Messenger = new VideoEffectMessenger(StateManager);

            _yuv = new float[3]
            {
                128f, 128f, 128f
            };

            Properties = new PropertySet();
            Properties[PropertyKeyThreshold] = 0f;
            Properties[PropertyKeyY] = _yuv[0];
            Properties[PropertyKeyU] = _yuv[1];
            Properties[PropertyKeyV] = _yuv[2];
            Properties[propertyKeyCommunication] = Messenger;

            StateManager.StateChanged += OnStateChanged;
        }

        public async Task<bool> InitializeAsync()
        {
            if (!Initialized)
            {
                _deviceInformationCollection = await DeviceInformation.FindAllAsync(DeviceClass.VideoCapture);                
                _deviceInformation = null;

                if (_deviceInformationCollection.Count != 0)
                {
                    _deviceInformation = _deviceInformationCollection[0];
                }

                if (_deviceInformation != null)
                {
                    MediaCapture = new MediaCapture();
                    MediaCaptureInitializationSettings settings = new MediaCaptureInitializationSettings();
                    settings.StreamingCaptureMode = StreamingCaptureMode.Video;
                    settings.VideoDeviceId = _deviceInformation.Id;
                    await MediaCapture.InitializeAsync(settings);
                }

                _videoDeviceController = MediaCapture.VideoDeviceController;

                IList<VideoEncodingProperties> listOfPropertiesWithHighestFrameRate =
                    CameraUtils.ResolveAllVideoEncodingPropertiesWithHighestFrameRate(_videoDeviceController, RecordMediaStreamType);

                if (listOfPropertiesWithHighestFrameRate != null && listOfPropertiesWithHighestFrameRate.Count > 0)
                {
                    VideoEncodingProperties selectedRecordingVideoEncodingProperties = listOfPropertiesWithHighestFrameRate.ElementAt(0);
                    uint selectedRecordingVideoWidth = selectedRecordingVideoEncodingProperties.Width;

                    for (int i = 1; i < listOfPropertiesWithHighestFrameRate.Count; ++i)
                    {
                        VideoEncodingProperties currentProperties = listOfPropertiesWithHighestFrameRate.ElementAt(i);

                        if (selectedRecordingVideoWidth > MaximumVideoWidth ||
                            (currentProperties.Width <= MaximumVideoWidth && currentProperties.Width > selectedRecordingVideoWidth))
                        {
                            selectedRecordingVideoEncodingProperties = currentProperties;
                            selectedRecordingVideoWidth = selectedRecordingVideoEncodingProperties.Width;
                        }
                    }

                    _hfrVideoEncodingProperties = selectedRecordingVideoEncodingProperties;
                    await MediaCapture.VideoDeviceController.SetMediaStreamPropertiesAsync(RecordMediaStreamType, selectedRecordingVideoEncodingProperties);

                    VideoEncodingProperties previewVideoEncodingProperties =
                        CameraUtils.FindVideoEncodingProperties(
                            _videoDeviceController, PreviewMediaStreamType,
                            PreviewFrameRate, selectedRecordingVideoWidth, selectedRecordingVideoEncodingProperties.Height);

                    System.Diagnostics.Debug.WriteLine("Highest framerate for recording is "
                        + CameraUtils.ResolveFrameRate(selectedRecordingVideoEncodingProperties)
                        + " frames per second with selected resolution of "
                        + selectedRecordingVideoWidth + "x" + selectedRecordingVideoEncodingProperties.Height
                        + "\nThe preview properties for viewfinder are "
                        + CameraUtils.ResolveFrameRate(previewVideoEncodingProperties) + " FPS and "
                        + previewVideoEncodingProperties.Width + "x" + previewVideoEncodingProperties.Height);

                    await MediaCapture.VideoDeviceController.SetMediaStreamPropertiesAsync(PreviewMediaStreamType, previewVideoEncodingProperties);
                }

                Initialized = true;
            }

            await MediaCapture.AddEffectAsync(RecordMediaStreamType, BufferEffectActivationId, Properties);

            if (MediaCapture.VideoDeviceController.WhiteBalanceControl.Supported)
            {
                await MediaCapture.VideoDeviceController.WhiteBalanceControl.SetPresetAsync(ColorTemperaturePreset.Fluorescent);
            }

            return Initialized;
        }

        public async Task<bool> StartAsync()
        {
            if (Initialized && !Started)
            {
                await StartPreviewAsync();
                await StartRecordingAsync();
            }

            return Started;
        }

        public async Task<bool> StartPreviewAsync()
        {
            if (Initialized && !PreviewStarted)
            {
                await MediaCapture.StartPreviewAsync();
                PreviewStarted = true;
            }

            return PreviewStarted;
        }

        public async Task<bool> StartRecordingAsync()
        {
            if (Initialized && PreviewStarted && !RecordingStarted)
            {
#if WINDOWS_PHONE_APP
                MediaEncodingProfile recordProfile = MediaEncodingProfile.CreateMp4(VideoEncodingQuality.HD720p);
                recordProfile.Video.FrameRate.Numerator = _hfrVideoEncodingProperties.FrameRate.Numerator;
                recordProfile.Video.FrameRate.Denominator = _hfrVideoEncodingProperties.FrameRate.Denominator;
                double factor = (double)(recordProfile.Video.FrameRate.Numerator) / (recordProfile.Video.FrameRate.Denominator * 4);
                recordProfile.Video.Bitrate = (uint)(recordProfile.Video.Width * recordProfile.Video.Height * factor);
                await MediaCapture.StartRecordToStreamAsync(recordProfile, _recordingStream);
#else // WINDOWS_PHONE_APP
                MediaEncodingProfile recordProfile = MediaEncodingProfile.CreateMp4(VideoEncodingQuality.Auto);
                await MediaCapture.StartRecordToStreamAsync(recordProfile, _recordingStream);
#endif // WINDOWS_PHONE_APP

                // Get camera's resolution
                VideoEncodingProperties resolution =
                    (VideoEncodingProperties)MediaCapture.VideoDeviceController.GetMediaStreamProperties(MediaStreamType.VideoRecord);
                ResolutionWidth = (int)resolution.Width;
                ResolutionHeight = (int)resolution.Height;
           
                RecordingStarted = true;
            }

            return RecordingStarted;
        }

        public async Task<bool> StopRecordingAsync()
        {
            if (RecordingStarted)
            {
                await MediaCapture.StopRecordAsync();
                RecordingStarted = false;
            }

            return RecordingStarted;
        }

        public async void DisposeAsync()
        {
            if (Initialized && MediaCapture != null)
            {
                await MediaCapture.StopRecordAsync();
                await MediaCapture.StopPreviewAsync();
                MediaCapture.Dispose();
                MediaCapture = null;
                PreviewStarted = false;
                RecordingStarted = false;
                Initialized = false;
            }
        }

        /// <summary>
        /// Sets the target YUV values for the effect.
        /// </summary>
        /// <param name="yuv">The new target YUV.</param>
        public async void SetYUVAsync(byte[] yuv)
        {
            if (EffectSet)
            {
                await MediaCapture.ClearEffectsAsync(PreviewMediaStreamType);
            }

            _yuv[0] = (float)yuv[0];
            _yuv[1] = (float)yuv[1];
            _yuv[2] = (float)yuv[2];
            Properties[PropertyKeyY] = _yuv[0];
            Properties[PropertyKeyU] = _yuv[1];
            Properties[PropertyKeyV] = _yuv[2];
            Properties[propertyKeyCommunication] = Messenger;

            if (EffectSet)
            {
                await MediaCapture.AddEffectAsync(PreviewMediaStreamType, EffectActivationId, Properties);
            }
        }

        /// <summary>
        /// Sets the given threshold value for the effect.
        /// </summary>
        /// <param name="threshold">The new threshold value.</param>
        /// <returns>True, if successful. False otherwise.</returns>
        public async Task<bool> SetThresholdAsync(float threshold)
        {
            if (Initialized && !_changingThreshold && threshold >= MinimumThreshold && threshold <= MaximumThreshold)
            {
                System.Diagnostics.Debug.WriteLine("SetThresholdAsync: " + threshold);
                _changingThreshold = true;

                await MediaCapture.ClearEffectsAsync(PreviewMediaStreamType);
                Properties[PropertyKeyThreshold] = threshold;
                await MediaCapture.AddEffectAsync(PreviewMediaStreamType, EffectActivationId, Properties);

                _changingThreshold = false;

                return true;
            }

            return false;
        }

        /// <summary>
        /// Starts/stops the effect.
        /// </summary>
        /// <returns>True, if the effect was set on. False, if it was turned off.</returns>
        public async Task<bool> ToggleEffectAsync()
        {
            if (Started)
            {
                if (EffectSet)
                {
                    await MediaCapture.ClearEffectsAsync(PreviewMediaStreamType);
                    StateManager.State = VideoEffectState.Idle;
                }
                else
                {
                    await MediaCapture.AddEffectAsync(PreviewMediaStreamType, EffectActivationId, Properties);
                    StateManager.State = VideoEffectState.Locking;
                }
            }

            return EffectSet;
        }

        private async void OnStateChanged(VideoEffectState newState, VideoEffectState oldState)
        {
            switch (newState)
            {
                case VideoEffectState.Idle:
                    await MediaCapture.ClearEffectsAsync(PreviewMediaStreamType);
                    break;
                case VideoEffectState.Locked:
                case VideoEffectState.PostProcess:
                    break;
            }
        }
    }
}
