// SPDX-License-Identifier: MIT

using System.Collections.Generic;
using Antmicro.Renode.Core;
using Antmicro.Renode.Peripherals.Bus;

namespace Antmicro.Renode.Peripherals.Mocks
{
    // The stick test holds both axes at their neutral 12-bit SAADC value.
    public class NRF52840_StickAdc : IDoubleWordPeripheral, IKnownSize
    {
        public NRF52840_StickAdc(IMachine machine)
        {
            bus = machine.GetSystemBus(this);
            IRQ = new GPIO();
        }

        public GPIO IRQ { get; }
        public long Size => 0x1000;

        public void Reset()
        {
            registers.Clear();
            interruptMask = 0;
            IRQ.Unset();
        }

        public uint ReadDoubleWord(long offset)
        {
            if(offset == IntEnable || offset == IntSet || offset == IntClear)
            {
                return interruptMask;
            }
            return registers.TryGetValue(offset, out var value) ? value : 0;
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            switch(offset)
            {
                case Start:
                    registers[Started] = value;
                    break;
                case Sample:
                    if(value != 0 && ReadDoubleWord(Enable) != 0)
                    {
                        WriteSamples();
                    }
                    break;
                case Stop:
                    registers[Stopped] = value;
                    break;
                case Calibrate:
                    registers[Calibrated] = value;
                    break;
                case IntEnable:
                    interruptMask = value;
                    break;
                case IntSet:
                    interruptMask |= value;
                    break;
                case IntClear:
                    interruptMask &= ~value;
                    break;
                default:
                    registers[offset] = value;
                    break;
            }
            IRQ.Set((ReadDoubleWord(End) != 0 && (interruptMask & (1u << 1)) != 0)
                 || (ReadDoubleWord(Calibrated) != 0 && (interruptMask & (1u << 4)) != 0));
        }

        private void WriteSamples()
        {
            uint address = ReadDoubleWord(ResultPointer);
            uint capacity = ReadDoubleWord(ResultMaximum);
            uint written = 0;
            for(int channel = 0; channel < 8 && written < capacity; channel++)
            {
                uint input = ReadDoubleWord(ChannelInput + 0x10 * channel);
                if(input != 0)
                {
                    bus.WriteWord(address + 2 * written, 1820);
                    written++;
                }
            }
            registers[ResultAmount] = written;
            registers[End] = 1;
        }

        private readonly IBusController bus;
        private readonly Dictionary<long, uint> registers = new();
        private uint interruptMask;

        private const long Start = 0x00;
        private const long Sample = 0x04;
        private const long Stop = 0x08;
        private const long Calibrate = 0x0C;
        private const long Started = 0x100;
        private const long End = 0x104;
        private const long Calibrated = 0x110;
        private const long Stopped = 0x114;
        private const long IntEnable = 0x300;
        private const long IntSet = 0x304;
        private const long IntClear = 0x308;
        private const long Enable = 0x500;
        private const long ChannelInput = 0x510;
        private const long ResultPointer = 0x62C;
        private const long ResultMaximum = 0x630;
        private const long ResultAmount = 0x634;
    }
}
