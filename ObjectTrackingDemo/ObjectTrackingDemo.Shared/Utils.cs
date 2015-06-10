using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices.WindowsRuntime; // For AsStream()
using System.Text;
using System.Threading.Tasks;
using Windows.Graphics.Display;
using Windows.Graphics.Imaging;
using Windows.Media.Capture;
using Windows.Media.Devices;
using Windows.Media.MediaProperties;
using Windows.Storage;
using Windows.Storage.Streams;
using Windows.UI;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Media.Imaging;

namespace ObjectTrackingDemo
{
    public class Utils
    {
        public static IRandomAccessStream ByteArrayToRandomAccessStream(byte[] byteArray)
        {
            MemoryStream memoryStream = new MemoryStream(byteArray);
            IRandomAccessStream randomAccessStream = memoryStream.AsRandomAccessStream();
            randomAccessStream.Seek(0);
            return randomAccessStream;
        }

        public static byte[] StreamToByteArray(Stream input)
        {
            byte[] buffer = new byte[16 * 1024];

            using (MemoryStream memoryStream = new MemoryStream())
            {
                int read = 0;
                long length = (buffer.Length > input.Length - input.Position) ? (input.Length - input.Position) : buffer.Length;

                while ((read = input.Read(buffer, 0, (int)length)) > 0)
                {
                    memoryStream.Write(buffer, 0, read);
                    length = (buffer.Length > input.Length - input.Position) ? (input.Length - input.Position) : buffer.Length;
                }

                return memoryStream.ToArray();
            }
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="yuvPixelArray"></param>
        /// <param name="width"></param>
        /// <param name="height"></param>
        /// <returns></returns>
        public async static Task<WriteableBitmap> NV12PixelArrayToWriteableBitmapAsync(byte[] yuvPixelArray, int width, int height)
        {
            WriteableBitmap writeableBitmap = null;
            byte[] rgbPixelArray = ImageProcessingUtils.NV12PixelArrayToRGBPixelArray(yuvPixelArray, width, height);

            if (rgbPixelArray != null)
            {
                writeableBitmap = new WriteableBitmap(width, height);

                using (Stream stream = writeableBitmap.PixelBuffer.AsStream())
                {
                    if (stream.CanWrite)
                    {
                        await stream.WriteAsync(rgbPixelArray, 0, rgbPixelArray.Length);
                        stream.Flush();
                    }
                }
            }

            return writeableBitmap;
        }

        public async static Task NV12PixelArrayToWriteableBitmapFileAsync(byte[] yuvPixelArray, int width, int height, string fileName)
        {
            byte[] rgbPixelArray = ImageProcessingUtils.NV12PixelArrayToRGBPixelArray(yuvPixelArray, width, height);

            var file = await Windows.Storage.ApplicationData.Current.LocalFolder.CreateFileAsync(fileName, CreationCollisionOption.GenerateUniqueName);
            using (IRandomAccessStream stream = await file.OpenAsync(FileAccessMode.ReadWrite))
            {
                Guid BitmapEncoderGuid = BitmapEncoder.JpegEncoderId;
                BitmapEncoder encoder = await BitmapEncoder.CreateAsync(BitmapEncoderGuid, stream);

                encoder.SetPixelData(BitmapPixelFormat.Bgra8, BitmapAlphaMode.Ignore,
                                    (uint)width,
                                    (uint)height,
                                    96.0,
                                    96.0,
                                    rgbPixelArray);
                await encoder.FlushAsync();
            }
       }
        

        /// <summary>
        /// 
        /// </summary>
        /// <param name="yuvPixelArray"></param>
        /// <param name="width"></param>
        /// <param name="height"></param>
        /// <returns></returns>
        public static BitmapImage NV12PixelArrayToBitmapImage(byte[] yuvPixelArray, int width, int height)
        {
            BitmapImage bitmapImage = null;
            byte[] rgbPixelArray = ImageProcessingUtils.NV12PixelArrayToRGBPixelArray(yuvPixelArray, width, height);

            if (rgbPixelArray != null)
            {
                bitmapImage = new BitmapImage();

                InMemoryRandomAccessStream stream = new InMemoryRandomAccessStream();
                stream.AsStreamForWrite().Write(rgbPixelArray, 0, rgbPixelArray.Length);
                stream.Seek(0);

                try
                {
                    bitmapImage.SetSource(stream);
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine(ex.Message);
                    bitmapImage = null;
                }
            }          

            return bitmapImage;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="yuvPixelArray"></param>
        /// <param name="width"></param>
        /// <param name="height"></param>
        /// <returns></returns>
        public async static Task<WriteableBitmap> YUY2PixelArrayToWriteableBitmapAsync(byte[] yuvPixelArray, int width, int height)
        {
            WriteableBitmap writeableBitmap = null;
            byte[] rgbPixelArray = ImageProcessingUtils.YUY2PixelArrayToRGBPixelArray(yuvPixelArray, width, height);

            if (rgbPixelArray != null)
            {
                writeableBitmap = new WriteableBitmap(width, height);

                using (Stream stream = writeableBitmap.PixelBuffer.AsStream())
                {
                    if (stream.CanWrite)
                    {
                        await stream.WriteAsync(rgbPixelArray, 0, rgbPixelArray.Length);
                        stream.Flush();
                    }
                }
            }

            return writeableBitmap;
        }

        public async static Task YUY2PixelArrayToWriteableBitmapFileAsync(byte[] yuvPixelArray, int width, int height, string fileName)
        {
            byte[] rgbPixelArray = ImageProcessingUtils.YUY2PixelArrayToRGBPixelArray(yuvPixelArray, width, height);

            var file = await Windows.Storage.ApplicationData.Current.LocalFolder.CreateFileAsync(fileName, CreationCollisionOption.GenerateUniqueName);
            using (IRandomAccessStream stream = await file.OpenAsync(FileAccessMode.ReadWrite))
            {
                Guid BitmapEncoderGuid = BitmapEncoder.JpegEncoderId;
                BitmapEncoder encoder = await BitmapEncoder.CreateAsync(BitmapEncoderGuid, stream);

                encoder.SetPixelData(BitmapPixelFormat.Bgra8, BitmapAlphaMode.Ignore,
                                    (uint)width,
                                    (uint)height,
                                    96.0,
                                    96.0,
                                    rgbPixelArray);
                await encoder.FlushAsync();
            }
        }


        /// <summary>
        /// 
        /// </summary>
        /// <param name="yuvPixelArray"></param>
        /// <param name="width"></param>
        /// <param name="height"></param>
        /// <returns></returns>
        public static BitmapImage YUY2PixelArrayToBitmapImage(byte[] yuvPixelArray, int width, int height)
        {
            BitmapImage bitmapImage = null;
            byte[] rgbPixelArray = ImageProcessingUtils.YUY2PixelArrayToRGBPixelArray(yuvPixelArray, width, height);

            if (rgbPixelArray != null)
            {
                bitmapImage = new BitmapImage();

                InMemoryRandomAccessStream stream = new InMemoryRandomAccessStream();
                stream.AsStreamForWrite().Write(rgbPixelArray, 0, rgbPixelArray.Length);
                stream.Seek(0);

                try
                {
                    bitmapImage.SetSource(stream);
                }
                catch (Exception ex)
                {
                    System.Diagnostics.Debug.WriteLine(ex.Message);
                    bitmapImage = null;
                }
            }

            return bitmapImage;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="controller"></param>
        /// <param name="mediaStreamType"></param>
        /// <returns></returns>
        public static VideoEncodingProperties ResolveVideoEncodingPropertiesWithHighestFrameRate(
            VideoDeviceController controller, MediaStreamType mediaStreamType)
        {
            VideoEncodingProperties propertiesWithHighestFrameRate = null;

            if (controller != null)
            {
                IReadOnlyList<IMediaEncodingProperties> availableProperties =
                    controller.GetAvailableMediaStreamProperties(mediaStreamType);
                uint highestFrameRate = 0;

                foreach (IMediaEncodingProperties properties in availableProperties)
                {
                    VideoEncodingProperties videoEncodingProperties = properties as VideoEncodingProperties;

                    if (videoEncodingProperties != null)
                    {
                        uint frameRate = ResolveFrameRate(videoEncodingProperties);

                        if (frameRate > highestFrameRate)
                        {
                            propertiesWithHighestFrameRate = videoEncodingProperties;
                            highestFrameRate = frameRate;
                        }
                    }
                }
            }

            return propertiesWithHighestFrameRate;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="properties"></param>
        /// <returns></returns>
        public static uint ResolveFrameRate(VideoEncodingProperties properties)
        {
            uint frameRate = 0;

            if (properties != null)
            {
                frameRate = properties.FrameRate.Numerator / properties.FrameRate.Denominator;
            }

            return frameRate;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="controller"></param>
        /// <param name="mediaStreamType"></param>
        /// <param name="frameRate"></param>
        /// <param name="width"></param>
        /// <param name="height"></param>
        /// <returns></returns>
        public static VideoEncodingProperties FindVideoEncodingProperties(
            VideoDeviceController controller, MediaStreamType mediaStreamType,
            uint frameRate, uint width, uint height)
        {
            VideoEncodingProperties matchingProperties = null;

            if (controller != null)
            {
                IReadOnlyList<IMediaEncodingProperties> availableProperties =
                    controller.GetAvailableMediaStreamProperties(mediaStreamType);

                foreach (IMediaEncodingProperties properties in availableProperties)
                {
                    VideoEncodingProperties videoEncodingProperties = properties as VideoEncodingProperties;

                    if (videoEncodingProperties != null)
                    {
                        if (ResolveFrameRate(videoEncodingProperties) == frameRate
                            && videoEncodingProperties.Width == width
                            && videoEncodingProperties.Height == height)
                        {
                            matchingProperties = videoEncodingProperties;
                            break;
                        }
                    }
                }
            }

            return matchingProperties;
        }

        public void ResolveScreenResolution(out double widthInPixels, out double heightInPixels)
        {
            DisplayInformation displayInformation = DisplayInformation.GetForCurrentView();
            double rawPixelsPerViewPixel = 1.0d;

#if WINDOWS_PHONE_APP
            rawPixelsPerViewPixel = displayInformation.RawPixelsPerViewPixel;
#else
            switch (displayInformation.ResolutionScale)
            {
                case ResolutionScale.Scale100Percent:
                    rawPixelsPerViewPixel = 1.0d;
                    break;
                case ResolutionScale.Scale120Percent:
                    rawPixelsPerViewPixel = 1.2d;
                    break;
                case ResolutionScale.Scale140Percent:
                    rawPixelsPerViewPixel = 1.4d;
                    break;
                case ResolutionScale.Scale150Percent:
                    rawPixelsPerViewPixel = 1.5d;
                    break;
                case ResolutionScale.Scale160Percent:
                    rawPixelsPerViewPixel = 1.6d;
                    break;
                case ResolutionScale.Scale180Percent:
                    rawPixelsPerViewPixel = 1.8d;
                    break;
                case ResolutionScale.Scale225Percent:
                    rawPixelsPerViewPixel = 2.25d;
                    break;
            }
#endif

            double boundsWidth = Windows.ApplicationModel.Core.CoreApplication.MainView.CoreWindow.Bounds.Width;
            double boundsHeight = Windows.ApplicationModel.Core.CoreApplication.MainView.CoreWindow.Bounds.Height;
            widthInPixels = Math.Round(boundsWidth * rawPixelsPerViewPixel, 0);
            heightInPixels = Math.Round(boundsHeight * rawPixelsPerViewPixel, 0);
            System.Diagnostics.Debug.WriteLine("Screen resolution is " + widthInPixels + "x" + heightInPixels);
        }

        /// <summary>
        /// Resolves the display size of the device running this app.
        /// </summary>
        /// <returns>The display size in inches or less than zero if unable to resolve.</returns>
        public double ResolveDisplaySizeInInches()
        {
            double displaySize = -1d;

            double screenResolutionX = 0d;
            double screenResolutionY = 0d;
            ResolveScreenResolution(out screenResolutionX, out screenResolutionY);

            DisplayInformation displayInformation = DisplayInformation.GetForCurrentView();
            float rawDpiX = displayInformation.RawDpiX;
            float rawDpiY = displayInformation.RawDpiY;

            if (rawDpiX > 0 && rawDpiY > 0)
            {
                displaySize = Math.Sqrt(
                    Math.Pow(screenResolutionX / rawDpiX, 2) +
                    Math.Pow(screenResolutionY / rawDpiY, 2));
                displaySize = Math.Round(displaySize, 1); // One decimal is enough
            }

            return displaySize;
        }
		
		public static String GetRecordVideoFormat(MediaCapture mediaCapture)
        {
            VideoEncodingProperties vp = (VideoEncodingProperties)mediaCapture.VideoDeviceController.GetMediaStreamProperties(MediaStreamType.VideoRecord);

            return vp.Subtype;
        }
    }
}
