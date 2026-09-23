// sim/stm32f1/ssd1306.cs —— SSD1306 128x64 OLED 的 Renode 器件模型(教学级)。
//
// 用法(顺序敏感,见 .claude/notes 调研记录 2026-09-22):
//   1. include @sim/stm32f1/ssd1306.cs   ← 必须在 mach create 之前,Roslyn 现场编译
//   2. machine LoadPlatformDescriptionFromString "oled: Antmicro.Renode.Peripherals.I2C.SSD1306 @ i2c1 0x3C"
//   3. showAnalyzer i2c1.oled            ← GUI 模式实时窗口;headless 见 PngPath
//
// 两个未成文约束:
//   - 命名空间必须以 Antmicro.Renode 开头,否则 TypeManager 不索引,.repl 报 E04;
//   - 1.17 已删除 machine LoadPlugin,别走旧教程的 dll 路线。
//
// 模型实现两件套:
//   II2CPeripheral —— STM32F1_I2C 控制器在 STOP 时把整个事务的 payload(不含地址
//                     字节)递进来:首字节控制字节,0x00=命令流 / 0x40=显存流;
//                     命令按 datasheet 表派发,数据写入 GDDRAM(8 页 x 128 列)。
//   IVideo         —— FinishTransmission 时若显存有变,重组 RGBA 帧发 FrameRendered;
//                     另支持把帧落成 PNG(headless/CI 无窗口也能"看见"画面)。
using System;
using System.IO;

using Antmicro.Renode.Backends.Display;
using Antmicro.Renode.Core;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.I2C;
using Antmicro.Renode.Peripherals.Video;
using Antmicro.Renode.Utilities;

using ELFSharp.ELF;

namespace Antmicro.Renode.Peripherals.I2C
{
    public class SSD1306 : II2CPeripheral, IVideo
    {
        public SSD1306(IMachine machine)
        {
            gddram = new byte[GddramSize];
            frame = new byte[Width * Height * 4];
            Reset();
        }

        public event Action<byte[]> FrameRendered;

        public event Action<int, int, PixelFormat, Endianess> ConfigurationChanged;

        // 帧落盘路径(留空则不落盘)。headless 跑法(--disable-xwt)下"看画面"就靠它。
        public string PngPath { get; set; }

        // PNG 放大倍数:128x64 原尺寸太小,默认放大 4 倍成 512x256 的块状像素。
        public int PngScale { get; set; } = 4;

        public void Write(byte[] data)
        {
            if(data.Length == 0)
            {
                return;
            }

            // 首字节控制字节:Co(bit7)=0 → 后续字节同流;D/C#(bit6) 决定命令/数据。
            var control = data[0];
            var continuous = (control & 0x80) == 0;
            var isData = (control & 0x40) != 0;

            var index = 1;
            while(index < data.Length)
            {
                if(!continuous)
                {
                    // Co=1:每个字节都缀自己的控制字节(单字节命令风格)。
                    control = data[index++];
                    isData = (control & 0x40) != 0;
                    if(index >= data.Length)
                    {
                        break;
                    }
                }
                if(isData)
                {
                    WriteGddram(data[index++]);
                }
                else
                {
                    index = WriteCommand(data, index);
                }
            }
        }

        public byte[] Read(int count = 1)
        {
            // SSD1306 的读(状态寄存器/显存回读)本库固件不用,按全零应答。
            return new byte[count];
        }

        public void FinishTransmission()
        {
            if(needsRender)
            {
                RenderFrame("i2c update");
            }
        }

        public void Reset()
        {
            Array.Clear(gddram, 0, gddram.Length);
            addressingMode = AddressingMode.Page;  // 上电默认页寻址(datasheet 10h)
            columnStart = 0;
            columnEnd = Width - 1;
            pageStart = 0;
            pageEnd = Pages - 1;
            column = 0;
            page = 0;
            lowerColumnNibble = 0;
            upperColumnNibble = 0;
            segmentRemapped = false;
            comRemapped = false;
            inverted = false;
            displayOn = false;
            needsRender = false;
            RenderFrame("reset");
        }

        // 从 data[offset] 起解析一条命令(含参数),返回下一字节下标。
        private int WriteCommand(byte[] data, int offset)
        {
            var command = data[offset];
            switch(command)
            {
                // 0x00-0x0F:列地址低 4 位;0x10-0x1F:列地址高 4 位(仅页寻址生效)
                case byte lower when lower <= 0x0F:
                    lowerColumnNibble = lower & 0x0F;
                    SetPageModeColumn();
                    return offset + 1;
                case byte upper when upper >= 0x10 && upper <= 0x1F:
                    upperColumnNibble = (upper & 0x0F) << 4;
                    SetPageModeColumn();
                    return offset + 1;
                case 0x20:  // 设置显存寻址模式
                    if(!HasArg(data, offset, 1))
                    {
                        return data.Length;
                    }
                    switch(data[offset + 1])
                    {
                        case 0x00: addressingMode = AddressingMode.Horizontal; break;
                        case 0x01: addressingMode = AddressingMode.Vertical; break;
                        case 0x02: addressingMode = AddressingMode.Page; break;
                        default:
                            this.Log(LogLevel.Warning, "unsupported addressing mode 0x{0:X2}", data[offset + 1]);
                            break;
                    }
                    return offset + 2;
                case 0x21:  // 列地址范围(水平/垂直寻址)
                    if(!HasArg(data, offset, 2))
                    {
                        return data.Length;
                    }
                    columnStart = Math.Min((int)data[offset + 1], Width - 1);
                    columnEnd = Math.Min((int)data[offset + 2], Width - 1);
                    column = columnStart;
                    return offset + 3;
                case 0x22:  // 页地址范围(水平/垂直寻址)
                    if(!HasArg(data, offset, 2))
                    {
                        return data.Length;
                    }
                    pageStart = Math.Min((int)data[offset + 1], Pages - 1);
                    pageEnd = Math.Min((int)data[offset + 2], Pages - 1);
                    page = pageStart;
                    return offset + 3;
                // 0x40-0x7F:显示起始行(仅滚屏用,本模型不滚屏,吃掉即可)
                case byte startLine when startLine >= 0x40 && startLine <= 0x7F:
                    return offset + 1;
                case 0x81:  // 对比度(单色模型无灰度,吃掉)
                case 0xA8:  // 多路复用比
                case 0xD3:  // 显示偏移
                case 0xD5:  // 时钟分频/振荡
                case 0xD9:  // 预充电周期
                case 0xDA:  // COM 引脚配置
                case 0xDB:  // VCOMH 电压
                case 0x8D:  // 电荷泵(真机要点亮必须开;模型恒供电,同样吃掉)
                    return HasArg(data, offset, 1) ? offset + 2 : data.Length;
                case 0xA0:
                    segmentRemapped = false;
                    needsRender = true;
                    return offset + 1;
                case 0xA1:
                    segmentRemapped = true;
                    needsRender = true;
                    return offset + 1;
                case 0xA4:  // 整屏点亮跟随 GDDRAM
                case 0xA5:  // 整屏强制点亮(忽略)
                case 0xA6:
                    inverted = false;
                    needsRender = true;
                    return offset + 1;
                case 0xA7:
                    inverted = true;
                    needsRender = true;
                    return offset + 1;
                case 0xAE:
                    displayOn = false;
                    needsRender = true;
                    return offset + 1;
                case 0xAF:
                    displayOn = true;
                    needsRender = true;
                    return offset + 1;
                // 0xB0-0xB7:页寻址模式下设置页指针
                case byte pageStartNew when pageStartNew >= 0xB0 && pageStartNew <= 0xB7:
                    page = pageStartNew & 0x07;
                    return offset + 1;
                case 0xC0:
                    comRemapped = false;
                    needsRender = true;
                    return offset + 1;
                case 0xC8:
                    comRemapped = true;
                    needsRender = true;
                    return offset + 1;
                case 0xE3:  // NOP
                    return offset + 1;
                default:
                    this.Log(LogLevel.Warning, "unhandled command 0x{0:X2}, skipping one byte", command);
                    return offset + 1;
            }
        }

        private void WriteGddram(byte value)
        {
            if(page >= Pages || column >= Width)
            {
                return;  // 指针在 0x21/0x22 约定范围外:按硬件静默丢弃
            }
            gddram[page * Width + column] = value;
            needsRender = true;
            switch(addressingMode)
            {
                case AddressingMode.Horizontal:
                    if(++column > columnEnd)
                    {
                        column = columnStart;
                        if(++page > pageEnd)
                        {
                            page = pageStart;
                        }
                    }
                    break;
                case AddressingMode.Vertical:
                    if(++page > pageEnd)
                    {
                        page = pageStart;
                        if(++column > columnEnd)
                        {
                            column = columnStart;
                        }
                    }
                    break;
                case AddressingMode.Page:
                    if(++column > columnEnd)
                    {
                        column = columnStart;  // 页模式只回绕列,翻页靠 0xB0-0xB7
                    }
                    break;
            }
        }

        // GDDRAM(页,列,位) → 屏幕 (x,y)。A1/C8 重映射按 datasheet 1. 段/COM 映射表。
        private void RenderFrame(string reason)
        {
            needsRender = false;
            var lit = 0;
            for(var p = 0; p < Pages; p++)
            {
                for(var x = 0; x < Width; x++)
                {
                    var bits = gddram[p * Width + x];
                    for(var b = 0; b < 8; b++)
                    {
                        var on = ((bits >> b) & 1) != 0;
                        if(inverted)
                        {
                            on = !on;
                        }
                        var visible = on && displayOn;
                        var sx = segmentRemapped ? Width - 1 - x : x;
                        var sy = comRemapped ? Height - 1 - (p * 8 + b) : p * 8 + b;
                        var o = (sy * Width + sx) * 4;
                        var level = (byte)(visible ? 0xFF : 0x00);
                        frame[o] = level;
                        frame[o + 1] = level;
                        frame[o + 2] = level;
                        frame[o + 3] = 0xFF;
                        if(visible)
                        {
                            lit++;
                        }
                    }
                }
            }
            this.Log(LogLevel.Info, "frame ({0}): {1} lit pixels, display {2}", reason, lit, displayOn ? "on" : "off");
            ConfigurationChanged?.Invoke(Width, Height, PixelFormat.RGBA8888, Endianess.LittleEndian);
            FrameRendered?.Invoke(frame);
            if(!string.IsNullOrEmpty(PngPath))
            {
                DumpPng();
            }
        }

        // 就地放大逐像素拷贝(最近邻):OLED 的"大果粒"观感就是这么来的。
        private void DumpPng()
        {
            var scale = Math.Max(1, PngScale);
            var w = Width * scale;
            var h = Height * scale;
            var enlarged = new byte[w * h * 4];
            for(var y = 0; y < h; y++)
            {
                var srcRow = (y / scale) * Width;
                var dstRow = y * w;
                for(var x = 0; x < w; x++)
                {
                    var src = (srcRow + x / scale) * 4;
                    var dst = (dstRow + x) * 4;
                    enlarged[dst] = frame[src];
                    enlarged[dst + 1] = frame[src + 1];
                    enlarged[dst + 2] = frame[src + 2];
                    enlarged[dst + 3] = frame[src + 3];
                }
            }
            var image = new RawImageData(enlarged, w, h);
            using(var png = image.ToPng())
            using(var file = new FileStream(PngPath, FileMode.Create, FileAccess.Write))
            {
                png.CopyTo(file);
            }
        }

        private void SetPageModeColumn()
        {
            if(addressingMode == AddressingMode.Page)
            {
                column = Math.Min(upperColumnNibble | lowerColumnNibble, Width - 1);
            }
        }

        private bool HasArg(byte[] data, int offset, int count)
        {
            if(offset + count >= data.Length)
            {
                this.Log(LogLevel.Warning, "command 0x{0:X2} truncated (needs {1} arg bytes)", data[offset], count);
                return false;
            }
            return true;
        }

        private readonly byte[] gddram;
        private readonly byte[] frame;

        private AddressingMode addressingMode;
        private int columnStart;
        private int columnEnd;
        private int pageStart;
        private int pageEnd;
        private int column;
        private int page;
        private int lowerColumnNibble;
        private int upperColumnNibble;
        private bool segmentRemapped;
        private bool comRemapped;
        private bool inverted;
        private bool displayOn;
        private bool needsRender;

        private const int Width = 128;
        private const int Height = 64;
        private const int Pages = 8;
        private const int GddramSize = Width * Pages;

        private enum AddressingMode
        {
            Horizontal = 0x00,
            Vertical = 0x01,
            Page = 0x02
        }
    }
}
