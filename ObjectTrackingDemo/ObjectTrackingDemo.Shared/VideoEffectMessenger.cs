using System;
using System.Collections.Generic;
using System.Text;
using System.Threading.Tasks;
using VideoEffect;
using Windows.UI.Xaml.Media.Imaging;

namespace ObjectTrackingDemo
{
    public delegate void FrameCapturedDelegate(byte[] pixelArray, int width, int height, int frameId);
    
    public delegate void PostProcessCompleteDelegate(
        byte[] pixelArray, int imageWidth, int imageHeight,
        ObjectDetails fromObjectDetails, ObjectDetails toObjectDetails);

    public struct ObjectDetails
    {
        public int centerX;
        public int centerY;
        public int width;
        public int height;
    }

    /// <summary>
    /// Implements MessengerInterface defined in the native video effect. This implementation
    /// enables the communication between the native and the managed side.
    /// </summary>
    public class VideoEffectMessenger : MessengerInterface
    {
        public const int NotDefined = 99999;

        public event FrameCapturedDelegate FrameCaptured;
        public event PostProcessCompleteDelegate PostProcessComplete;

        private StateManager _stateManager;
        private int _operationDurationInMilliseconds;
        private int _frameRequestId;

        public ObjectDetails LockedRect
        {
            get;
            private set;
        }

        public int OperationDurationInMilliseconds
        {
            get
            {
                return _operationDurationInMilliseconds;
            }
            private set
            {
                _operationDurationInMilliseconds = value;
            }
        }

        public VideoEffectMessenger(StateManager stateManager)
        {
            _operationDurationInMilliseconds = NotDefined;
            _stateManager = stateManager;
        }

        public int State()
        {
            return (int)_stateManager.State;
        }

        public void SetState(int state)
        {
            _stateManager.SetState(state);
        }

        public void SetLockedRect(int centerX, int centerY, int width, int height)
        {
            ObjectDetails lockedRect;
            lockedRect.centerX = centerX;
            lockedRect.centerY = centerY;
            lockedRect.width = width;
            lockedRect.height = height;
            LockedRect = lockedRect;
        }

        public void UpdateOperationDurationInMilliseconds(int milliseconds)
        {
            OperationDurationInMilliseconds = milliseconds;
        }

        public void SaveFrame(byte[] pictureArray, int width, int height, int counter, int seriesIdentifier)
        {
            System.Diagnostics.Debug.WriteLine("SaveFrame(): " + counter);

            try
            {
                String filename = "img_" + seriesIdentifier.ToString() + "_" + counter.ToString() + ".jpg";
                //Task saver = Utils.NV12PixelArrayToWriteableBitmapFileAsync(pictureArray, width, height, filename);
                //saver.Wait();
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine("SaveFrame(): " + ex.ToString());
            }
        }

        public void RequestFrame(int id)
        {
            _frameRequestId = id;
        }

        public int FrameRequested()
        {
            int tmpFrameRequired = _frameRequestId;
            _frameRequestId = 0;
            return tmpFrameRequired;
        }

        public void NotifyFrameCaptured(byte[] pixelArray, int width, int height, int frameId)
        {
            if (FrameCaptured != null)
            {
                FrameCaptured(pixelArray, width, height, frameId);
            }
        }

        public void NotifyPostProcessComplete(
            byte[] pixelArray, int imageWidth, int imageHeight,
            int fromCenterX, int fromCenterY, int fromWidth, int fromHeight,
            int toCenterX, int toCenterY, int toWidth, int toHeight)
        {
            if (PostProcessComplete != null)
            {
                PostProcessComplete(pixelArray, imageWidth, imageHeight,
                    new ObjectDetails
                    {
                        centerX = fromCenterX,
                        centerY = fromCenterY,
                        width = fromWidth,
                        height = fromHeight
                    },
                    new ObjectDetails
                    {
                        centerX = toCenterX,
                        centerY = toCenterY,
                        width = toWidth,
                        height = toHeight
                    });
            }
        }
    }
}
