using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime; // For IBuffer.ToArray()
using System.Text;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Graphics.Imaging;
using Windows.Storage;
using Windows.Storage.Streams;
using Windows.UI;
using Windows.UI.Xaml.Media.Imaging;

namespace ObjectTrackingDemo
{
    class ImageProcessingUtils
    {
        /// <summary>
        /// Converts RGB888 pixel to YUV pixel.
        /// 
        /// From https://msdn.microsoft.com/en-us/library/aa917087.aspx
        /// Y = ( (  66 * R + 129 * G +  25 * B + 128) >> 8) +  16
        /// U = ( ( -38 * R -  74 * G + 112 * B + 128) >> 8) + 128
        /// V = ( ( 112 * R -  94 * G -  18 * B + 128) >> 8) + 128
        /// </summary>
        /// <param name="r"></param>
        /// <param name="g"></param>
        /// <param name="b"></param>
        /// <returns></returns>
        public static byte[] RGBToYUV(byte r, byte g, byte b)
        {
            byte[] yuv = new byte[3];
            yuv[0] = (byte)(((66 * r + 129 * g + 25 * b + 128) >> 8) + 16);
            yuv[1] = (byte)(((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128);
            yuv[2] = (byte)(((112 * r - 94 * g - 18 * b + 128) >> 8) + 128);
            return yuv;
        }

        /// <summary>
        /// Converts 8-bit YUV pixel to RGB888 pixel.
        /// </summary>
        /// <param name="y"></param>
        /// <param name="u"></param>
        /// <param name="v"></param>
        /// <returns></returns>
        public static byte[] YUVToRGB(byte y, byte u, byte v)
        {
            int c = y - 16;
            int d = u - 128;
            int e = v - 128;

            byte[] rgb = new byte[3];
            rgb[0] = (byte)Clip((298 * c + 409 * e + 128) >> 8);
            rgb[1] = (byte)Clip((298 * c - 100 * d - 208 * e + 128) >> 8);
            rgb[2] = (byte)Clip((298 * c + 516 * d + 128) >> 8);

            return rgb;
        }

        /// <summary>
        /// Converts the given NV12 pixel array format into an RGB pixel array.
        /// </summary>
        /// <param name="yuvPixelArray">The NV12 pixel array.</param>
        /// <param name="pixelWidth">The image width in pixels.</param>
        /// <param name="pixelHeight">The image height in pixels.</param>
        /// <param name="ignoreAlpha">If true, will ignore alpha channel.</param>
        /// <returns>An RGB pixel array.</returns>
        public static byte[] NV12PixelArrayToRGBPixelArray(byte[] yuvPixelArray, int pixelWidth, int pixelHeight, bool ignoreAlpha = false)
        {
            if (yuvPixelArray.Length < pixelWidth * pixelHeight * 1.5f)
            {
                System.Diagnostics.Debug.WriteLine(
                    "NV12PixelArrayToRGBPixelArray: Too few bytes: Was expecting "
                    + pixelWidth * pixelHeight * 1.5f + ", but got " + yuvPixelArray.Length);
                return null;
            }

            System.Diagnostics.Debug.WriteLine(
                "NV12PixelArrayToRGBPixelArray: " + yuvPixelArray.Length + " bytes, "
                + pixelWidth + "x" + pixelHeight);

            int bytesPerPixel = ignoreAlpha ? 3 : 4;
            int bytesPerLineRGB = pixelWidth * bytesPerPixel;
            byte[] rgbPixelArray = new byte[bytesPerLineRGB * pixelHeight];

            byte yy = 0;
            byte u = 0;
            byte v = 0;

            int uvPlaneStartIndex = pixelWidth * pixelHeight - 1;

            byte[] rgbPixel = null;
            int rgbIndex = 0;

            for (int y = 0; y < pixelHeight; ++y)
            {
                int cumulativeYIndex = y * pixelWidth;
                int cumulativeUVPlaneIndex = y / 2 * pixelWidth + uvPlaneStartIndex;

                for (int x = 0; (x + 1) < pixelWidth; x += 2)
                {
                    int yAsInt = ((int)yuvPixelArray[cumulativeYIndex + x] + (int)yuvPixelArray[cumulativeYIndex + x + 1]
                        + (int)yuvPixelArray[cumulativeYIndex + pixelWidth + x] + (int)yuvPixelArray[cumulativeYIndex + pixelWidth + x + 1]) / 4;
                    yy = (byte)yAsInt;
                    u = yuvPixelArray[cumulativeUVPlaneIndex + x];
                    v = yuvPixelArray[cumulativeUVPlaneIndex + x + 1];

                    rgbPixel = YUVToRGB(yy, u, v);

                    for (int i = 0; i < 2; ++i)
                    {
                        rgbPixelArray[rgbIndex] = rgbPixel[0]; // B
                        rgbPixelArray[rgbIndex + 1] = rgbPixel[1]; // G
                        rgbPixelArray[rgbIndex + 2] = rgbPixel[2]; // R

                        if (!ignoreAlpha)
                        {
                            rgbPixelArray[rgbIndex + 3] = 0xff;
                        }

                        rgbIndex += bytesPerPixel;
                    }
                }
            }

            return rgbPixelArray;
        }

        /// <summary>
        /// Converts the given YUV2 pixel array format into an RGB pixel array.
        /// </summary>
        /// <param name="yuvPixelArray">The YUV2 pixel array.</param>
        /// <param name="pixelWidth">The image width in pixels.</param>
        /// <param name="pixelHeight">The image height in pixels.</param>
        /// <param name="ignoreAlpha">If true, will ignore alpha channel.</param>
        /// <returns>An RGB pixel array.</returns>
        public static byte[] YUY2PixelArrayToRGBPixelArray(byte[] yuvPixelArray, int pixelWidth, int pixelHeight, bool ignoreAlpha = false)
        {
            if (yuvPixelArray.Length < pixelWidth * pixelHeight * 2)
            {
                System.Diagnostics.Debug.WriteLine(
                    "YUY2PixelArrayToRGBPixelArray: Too few bytes: Was expecting "
                    + pixelWidth * pixelHeight * 2 + ", but got " + yuvPixelArray.Length);
                return null;
            }

            System.Diagnostics.Debug.WriteLine(
                "YUY2PixelArrayToRGBPixelArray: " + yuvPixelArray.Length + " bytes, "
                + pixelWidth + "x" + pixelHeight);

            int bytesPerPixel = ignoreAlpha ? 3 : 4;
            int bytesPerLineRGB = pixelWidth * bytesPerPixel;
            byte[] rgbPixelArray = new byte[bytesPerLineRGB * pixelHeight];

            byte yy = 0;
            byte u = 0;
            byte v = 0;

            byte[] rgbPixel = null;
            int rgbIndex = 0;

            for (int y = 0; y < pixelHeight; ++y)
            {
                for (int x = 0; (x + 1) < pixelWidth; x += 2)
                {
                    int index = (y * pixelWidth + x) << 1;
                    yy = (byte)((yuvPixelArray[index] + yuvPixelArray[index + 2]) >> 1);
                    u = yuvPixelArray[index + 3];
                    v = yuvPixelArray[index + 1];

                    rgbPixel = YUVToRGB(yy, u, v);

                    for (int i = 0; i < 2; ++i)
                    {
                        rgbPixelArray[rgbIndex] = rgbPixel[0]; // B
                        rgbPixelArray[rgbIndex + 1] = rgbPixel[1]; // G
                        rgbPixelArray[rgbIndex + 2] = rgbPixel[2]; // R

                        if (!ignoreAlpha)
                        {
                            rgbPixelArray[rgbIndex + 3] = 0xff;
                        }

                        rgbIndex += bytesPerPixel;
                    }
                }
            }

            return rgbPixelArray;
        }

        /// <summary>
        /// Resolves and returns the color at the given point in the given image.
        /// 
        /// Note: At the moment the implementation expects that there are four bytes per pixels in
        /// the given array.
        /// </summary>
        /// <param name="pixelArray">A byte array containing the image data.</param>
        /// <param name="width">The width of the image.</param>
        /// <param name="height">The height of the image.</param>
        /// <param name="point">The point of the desired color.</param>
        /// <returns>A Color instance.</returns>
        public static Color GetColorAtPoint(byte[] pixelArray, uint width, uint height, Point point)
        {
            int bytesPerPixel = (int)(pixelArray.Length / height / width);

            System.Diagnostics.Debug.WriteLine("Image size is " + width + "x" + height
                + ", byte array length is " + pixelArray.Length + " => number of bytes per pixel is " + bytesPerPixel);

            int index = (int)(point.Y * width * bytesPerPixel + point.X * bytesPerPixel);

            System.Diagnostics.Debug.WriteLine("Picked index is " + index + ": "
                + pixelArray[index] + " "
                + pixelArray[index + 1] + " "
                + pixelArray[index + 2] + " "
                + pixelArray[index + 3]);

            return new Color
            {
                A = 0xFF, //pixelArray[index],
                R = pixelArray[index + 2],
                G = pixelArray[index + 1],
                B = pixelArray[index]
            };
        }

        #region GetColorAtPoint overloads

        public static Color GetColorAtPoint(IBuffer pixelBuffer, uint width, uint height, Point point)
        {
            return GetColorAtPoint(pixelBuffer.ToArray(), width, height, point);
        }

        public static Color GetColorAtPoint(WriteableBitmap bitmap, uint width, uint height, Point point)
        {
            return GetColorAtPoint(bitmap.PixelBuffer.ToArray(), width, height, point);
        }

        #endregion

        public static double BrightnessFromColor(Color color)
        {
            return (color.R + color.G + color.B) / 3;
        }

        public static Color ApplyBrightnessToColor(Color color, double brightness)
        {
            if (brightness >= 0 && brightness <= 255 && brightness != BrightnessFromColor(color))
            {
                int change = (BrightnessFromColor(color) > brightness) ? -1 : 1;

                while (BrightnessFromColor(color) != brightness)
                {
                    if ((change == 1 && color.R < 255)
                        || (change == -1 && color.R > 0))
                    {
                        color.R += (byte)change;
                    }

                    if ((change == 1 && color.G < 255)
                        || (change == -1 && color.G > 0))
                    {
                        color.G += (byte)change;
                    }

                    if ((change == 1 && color.B < 255)
                        || (change == -1 && color.B > 0))
                    {
                        color.B += (byte)change;
                    }
                }
            }

            return color;
        }

        /// <summary>
        /// Converts the given image in NV12 format into a writeable bitmap.
        /// </summary>
        /// <param name="yuvPixelArray">The image data in NV12 format.</param>
        /// <param name="width">The width of the image in pixels.</param>
        /// <param name="height">The height of the image in pixels.</param>
        /// <returns>A WriteableBitmap instance containing the image.</returns>
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

        /// <summary>
        /// Converts the given image in NV12 format into a bitmap image.
        /// </summary>
        /// <param name="yuvPixelArray">The image data in NV12 format.</param>
        /// <param name="width">The width of the image in pixels.</param>
        /// <param name="height">The height of the image in pixels.</param>
        /// <returns>A BitmapImage instance containing the image.</returns>
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
        /// Stores the given image in NV12 format into a file.
        /// </summary>
        /// <param name="yuvPixelArray">The image data in NV12 format.</param>
        /// <param name="width">The width of the image in pixels.</param>
        /// <param name="height">The height of the image in pixels.</param>
        /// <param name="fileName">The desired filename.</param>
        public async static Task NV12PixelArrayToWriteableBitmapFileAsync(byte[] yuvPixelArray, int width, int height, string fileName)
        {
            byte[] rgbPixelArray = ImageProcessingUtils.NV12PixelArrayToRGBPixelArray(yuvPixelArray, width, height);
            var file = await Windows.Storage.ApplicationData.Current.LocalFolder.CreateFileAsync(
                fileName, CreationCollisionOption.GenerateUniqueName);

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
        /// Converts the given image in YUV2 format into a writeable bitmap.
        /// </summary>
        /// <param name="yuvPixelArray">The image data in YUV2 format.</param>
        /// <param name="width">The width of the image in pixels.</param>
        /// <param name="height">The height of the image in pixels.</param>
        /// <returns>A WriteableBitmap instance containing the image.</returns>
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

        /// <summary>
        /// Converts the YUV2 image data into a bitmap image.
        /// </summary>
        /// <param name="yuvPixelArray">The image data in YUV2 format.</param>
        /// <param name="width">The width of the image in pixels.</param>
        /// <param name="height">The height of the image in pixels.</param>
        /// <returns>A BitmapImage instance containing the image.</returns>
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
        /// Stores the given image in YUV2 format into a file.
        /// </summary>
        /// <param name="yuvPixelArray">The image data in YUV2 format.</param>
        /// <param name="width">The width of the image in pixels.</param>
        /// <param name="height">The height of the image in pixels.</param>
        /// <param name="fileName">The desired file name.</param>
        public async static Task YUY2PixelArrayToWriteableBitmapFileAsync(byte[] yuvPixelArray, int width, int height, string fileName)
        {
            byte[] rgbPixelArray = ImageProcessingUtils.YUY2PixelArrayToRGBPixelArray(yuvPixelArray, width, height);
            var file = await Windows.Storage.ApplicationData.Current.LocalFolder.CreateFileAsync(
                fileName, CreationCollisionOption.GenerateUniqueName);

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
        /// Scales the image in the given memory stream.
        /// </summary>
        /// <param name="originalStream">The original image stream to scale.</param>
        /// <param name="originalResolutionWidth">The original width.</param>
        /// <param name="originalResolutionHeight">The original height.</param>
        /// <param name="scaledStream">Stream where the scaled image is stored.</param>
        /// <param name="scaleWidth">The target width.</param>
        /// <param name="scaleHeight">The target height.</param>
        /// <returns></returns>
        public static async Task ScaleImageStreamAsync(MemoryStream originalStream,
                                                       int originalResolutionWidth,
                                                       int originalResolutionHeight,
                                                       MemoryStream scaledStream,
                                                       int scaleWidth,
                                                       int scaleHeight)
        {
            System.Diagnostics.Debug.WriteLine("ScaleImageStreamAsync() ->");

            // Create a bitmap containing the full resolution image
            var bitmap = new WriteableBitmap(originalResolutionWidth, originalResolutionHeight);
            originalStream.Seek(0, SeekOrigin.Begin);
            await bitmap.SetSourceAsync(originalStream.AsRandomAccessStream());

            /* Construct a JPEG encoder with the newly created
             * InMemoryRandomAccessStream as target
             */
            IRandomAccessStream previewResolutionStream = new InMemoryRandomAccessStream();
            previewResolutionStream.Size = 0;
            BitmapEncoder encoder = await BitmapEncoder.CreateAsync(
                BitmapEncoder.JpegEncoderId, previewResolutionStream);

            // Copy the full resolution image data into a byte array
            Stream pixelStream = bitmap.PixelBuffer.AsStream();
            var pixelArray = new byte[pixelStream.Length];
            await pixelStream.ReadAsync(pixelArray, 0, pixelArray.Length);

            // Set the scaling properties
            encoder.BitmapTransform.ScaledWidth = (uint)scaleWidth;
            encoder.BitmapTransform.ScaledHeight = (uint)scaleHeight;
            encoder.BitmapTransform.InterpolationMode = BitmapInterpolationMode.Fant;
            encoder.IsThumbnailGenerated = true;

            // Set the image data and the image format setttings to the encoder
            encoder.SetPixelData(BitmapPixelFormat.Bgra8, BitmapAlphaMode.Ignore,
                (uint)originalResolutionWidth, (uint)originalResolutionHeight,
                96.0, 96.0, pixelArray);

            await encoder.FlushAsync();
            previewResolutionStream.Seek(0);
            await previewResolutionStream.AsStream().CopyToAsync(scaledStream);

            System.Diagnostics.Debug.WriteLine("<- ScaleImageStreamAsync()");
        }

        /// <summary>
        /// Encodes the image data in the given file stream to JPEG and writes
        /// it to the given destination stream. This method should be used if
        /// the image data is not JPEG encoded. Note that the method has no
        /// error handling.
        /// </summary>
        /// <param name="fileStream">The file stream with the image data.</param>
        /// <param name="destinationStream">Where the JPEG data is written to.</param>
        /// <returns></returns>
        public static async Task FileStreamToJpegStreamAsync(IRandomAccessStream fileStream,
                                                             IRandomAccessStream destinationStream)
        {
            var image = new BitmapImage();
            image.SetSource(fileStream);
            int width = image.PixelWidth;
            int height = image.PixelHeight;
            var bitmap = new WriteableBitmap(width, height);
            bitmap.SetSource(fileStream);
            BitmapEncoder encoder = await BitmapEncoder.CreateAsync(BitmapEncoder.JpegEncoderId, destinationStream);
            Stream outStream = bitmap.PixelBuffer.AsStream();
            var pixels = new byte[outStream.Length];
            await outStream.ReadAsync(pixels, 0, pixels.Length);
            encoder.SetPixelData(BitmapPixelFormat.Bgra8, BitmapAlphaMode.Ignore, (uint)width, (uint)height, 96.0, 96.0, pixels);
            await encoder.FlushAsync();
        }

        private static byte Clip(byte value, byte min = 0, byte max = 255)
        {
            if (value < min)
            {
                return min;
            }

            if (value > max)
            {
                return max;
            }

            return value;
        }

        private static int Clip(int value, int min = 0, int max = 255)
        {
            if (value < min)
            {
                return min;
            }

            if (value > max)
            {
                return max;
            }

            return value;
        }
    }
}
