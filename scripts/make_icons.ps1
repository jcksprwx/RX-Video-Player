Add-Type -AssemblyName System.Drawing

$code = @"
using System;
using System.IO;
using System.Drawing;
using System.Drawing.Imaging;
using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;

public class IconMaker
{
    public static void Process(string srcJpg, string outPng512, string outIco)
    {
        float cx = 512.0f;
        float cy = 514.5f;
        float radius = 395.5f;

        int masterSize = 512;
        float targetRadius = 238.0f;
        float scale = targetRadius / radius;
        float targetCx = masterSize / 2.0f;
        float targetCy = masterSize / 2.0f;

        using (Bitmap src = new Bitmap(srcJpg))
        using (Bitmap master = new Bitmap(masterSize, masterSize, PixelFormat.Format32bppArgb))
        {
            BitmapData srcData = src.LockBits(new Rectangle(0, 0, src.Width, src.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppRgb);
            BitmapData dstData = master.LockBits(new Rectangle(0, 0, masterSize, masterSize), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);

            int srcBytesCount = srcData.Stride * src.Height;
            byte[] srcBytes = new byte[srcBytesCount];
            Marshal.Copy(srcData.Scan0, srcBytes, 0, srcBytesCount);

            int dstBytesCount = dstData.Stride * masterSize;
            byte[] dstBytes = new byte[dstBytesCount];

            int srcStride = srcData.Stride;
            int dstStride = dstData.Stride;

            for (int y = 0; y < masterSize; y++)
            {
                int dstRowOffset = y * dstStride;

                for (int x = 0; x < masterSize; x++)
                {
                    int inCount = 0;
                    int sumR = 0, sumG = 0, sumB = 0;

                    for (int sy = 0; sy < 4; sy++)
                    {
                        float subY = y + (sy + 0.5f) / 4.0f;
                        float dy = subY - targetCy;

                        for (int sx = 0; sx < 4; sx++)
                        {
                            float subX = x + (sx + 0.5f) / 4.0f;
                            float dx = subX - targetCx;

                            float dist = (float)Math.Sqrt(dx * dx + dy * dy);
                            if (dist <= targetRadius)
                            {
                                inCount++;

                                float srcX = cx + dx / scale;
                                float srcY = cy + dy / scale;

                                int isrcX = (int)Math.Max(0, Math.Min(src.Width - 1, (float)Math.Round(srcX)));
                                int isrcY = (int)Math.Max(0, Math.Min(src.Height - 1, (float)Math.Round(srcY)));

                                int srcIdx = (isrcY * srcStride) + (isrcX * 4);
                                sumB += srcBytes[srcIdx + 0];
                                sumG += srcBytes[srcIdx + 1];
                                sumR += srcBytes[srcIdx + 2];
                            }
                        }
                    }

                    int dstIdx = dstRowOffset + (x * 4);
                    if (inCount == 0)
                    {
                        dstBytes[dstIdx + 0] = 0;
                        dstBytes[dstIdx + 1] = 0;
                        dstBytes[dstIdx + 2] = 0;
                        dstBytes[dstIdx + 3] = 0;
                    }
                    else
                    {
                        byte alpha = (byte)((inCount * 255) / 16);
                        dstBytes[dstIdx + 0] = (byte)(sumB / inCount);
                        dstBytes[dstIdx + 1] = (byte)(sumG / inCount);
                        dstBytes[dstIdx + 2] = (byte)(sumR / inCount);
                        dstBytes[dstIdx + 3] = alpha;
                    }
                }
            }

            Marshal.Copy(dstBytes, 0, dstData.Scan0, dstBytesCount);
            src.UnlockBits(srcData);
            master.UnlockBits(dstData);

            Directory.CreateDirectory(Path.GetDirectoryName(outPng512));
            master.Save(outPng512, ImageFormat.Png);
            Console.WriteLine("Saved master PNG: " + outPng512);

            int[] sizes = new int[] { 256, 128, 64, 48, 32, 16 };
            byte[][] pngBlobs = new byte[sizes.Length][];

            for (int i = 0; i < sizes.Length; i++)
            {
                int s = sizes[i];
                using (Bitmap resized = new Bitmap(s, s, PixelFormat.Format32bppArgb))
                {
                    using (Graphics g = Graphics.FromImage(resized))
                    {
                        g.Clear(Color.Transparent);
                        g.SmoothingMode = SmoothingMode.HighQuality;
                        g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                        g.DrawImage(master, new Rectangle(0, 0, s, s));
                    }
                    using (MemoryStream ms = new MemoryStream())
                    {
                        resized.Save(ms, ImageFormat.Png);
                        pngBlobs[i] = ms.ToArray();
                    }
                }
            }

            using (FileStream fs = new FileStream(outIco, FileMode.Create, FileAccess.Write))
            using (BinaryWriter bw = new BinaryWriter(fs))
            {
                bw.Write((short)0);
                bw.Write((short)1);
                bw.Write((short)sizes.Length);

                int offset = 6 + (sizes.Length * 16);

                for (int i = 0; i < sizes.Length; i++)
                {
                    int s = sizes[i];
                    bw.Write((byte)(s >= 256 ? 0 : s));
                    bw.Write((byte)(s >= 256 ? 0 : s));
                    bw.Write((byte)0);
                    bw.Write((byte)0);
                    bw.Write((short)1);
                    bw.Write((short)32);
                    bw.Write((int)pngBlobs[i].Length);
                    bw.Write((int)offset);

                    offset += pngBlobs[i].Length;
                }

                for (int i = 0; i < sizes.Length; i++)
                {
                    bw.Write(pngBlobs[i]);
                }
            }
            Console.WriteLine("Saved ICO with 6 resolutions: " + outIco);
        }
    }
}
"@

Add-Type -TypeDefinition $code -ReferencedAssemblies "System.Drawing"

$src = "C:\Users\Ritur\.gemini\antigravity\brain\1f6f6d61-11cb-4395-9f59-1e3cfcd55f6f\.user_uploaded\media_1793469030117_a3a5572f.jpg"
$outPng = "D:\Project-Code\RX Media Player\assets\icons\app_icon.png"
$outIco = "D:\Project-Code\RX Media Player\assets\icons\app_icon.ico"
$brainPng = "C:\Users\Ritur\.gemini\antigravity\brain\1f6f6d61-11cb-4395-9f59-1e3cfcd55f6f\app_icon_preview.png"

[IconMaker]::Process($src, $outPng, $outIco)
Copy-Item $outPng $brainPng -Force
Get-Item $outPng, $outIco, $brainPng | Select-Object Name, Length, LastWriteTime
