using System;
using Antmicro.Renode.Core;
using Antmicro.Renode.Peripherals;
using Antmicro.Renode.Peripherals.Bus;
using Antmicro.Renode.Peripherals.Timers;
using Antmicro.Renode.Time;

namespace Antmicro.Renode.Peripherals.Miscellaneous
{
    public sealed class VfdBus : IDoubleWordPeripheral, IKnownSize, IGPIOReceiver
    {
        public VfdBus(IMachine machine)
        {
            release = new LimitTimer(machine.ClockSource, 1000000, this, "busy",
                limit: 25, direction: Direction.Descending,
                workMode: WorkMode.OneShot, eventEnabled: true);
            release.LimitReached += () => { if(!held) Busy.Unset(); };
        }

        public void Reset()
        {
            data = 0;
            count = 0;
            resetLow = ready = wroteLow = held = failed = false;
            Busy.Unset();
            release.Reset();
        }

        public void OnGPIO(int number, bool high)
        {
            if(number < 8)
            {
                data = high ? data | (1 << number) : data & ~(1 << number);
                return;
            }

            switch(number)
            {
                case 8: // /WR
                    if(ready)
                    {
                        if(high)
                        {
                            Capture();
                        }
                        else
                        {
                            Check(!Busy.IsSet, "/WR fell while PBUSY was high");
                            wroteLow = true;
                        }
                    }
                    break;
                case 9: // /RESET
                    if(!high)
                    {
                        resetLow = true;
                    }
                    else if(resetLow)
                    {
                        ready = true;
                    }
                    break;
                case 10: // lock PBUSY high for the timeout test
                    if(high)
                    {
                        Check(!Busy.IsSet, "sketch skipped the final PBUSY wait");
                        held = true;
                        Busy.Set();
                    }
                    break;
                case 11: // sketch signaled a timeout passed
                    if(high)
                    {
                        Check(held && count == Expected.Length, "incomplete transaction");
                        if(!failed)
                        {
                            Console.WriteLine("VFD BUS PASS");
                        }
                    }
                    break;
            }
        }

        public uint ReadDoubleWord(long offset) => 0;
        public void WriteDoubleWord(long offset, uint value) {}
        public long Size => 4;
        public GPIO Busy { get; } = new GPIO();

        private void Capture()
        {
            Check(wroteLow && !Busy.IsSet, "/WR rose without a valid low phase");
            Console.WriteLine("VFD byte {0:X2}", data);
            Check(count < Expected.Length && data == Expected[count],
                "unexpected byte at index " + count);
            ++count;
            wroteLow = false;
            Busy.Set();
            release.ResetValue();
            release.Enabled = true;
        }

        private void Check(bool condition, string message)
        {
            if(!condition)
            {
                failed = true;
                Console.WriteLine("VFD BUS FAIL: " + message);
            }
        }

        private int data;
        private int count;
        private bool resetLow, ready, wroteLow, held, failed;
        private readonly LimitTimer release;
        private static readonly byte[] Expected = {
            0x1b, 0x40, 0x0c,
            0x1f, 0x24, 4, 1, 2, 0, 0x48, 0x69,
            0x1f, 0x24, 120, 0, 3, 0,
            0x1f, 0x28, 0x66, 0x11, 2, 0, 2, 0, 1,
            0x80, 0x01, 0xff, 0x00
        };
    }
}
