// Test responses for module selection only; no physical movement is modeled.
using Antmicro.Renode.Peripherals.I2C;

namespace Antmicro.Renode.Peripherals.Mocks
{
    public class MeKaBuMagnet : II2CPeripheral
    {
        public void Reset() {}
        public void FinishTransmission() {}
        public void Write(byte[] data) {}
        public byte[] Read(int count) { return new byte[count]; }
    }

    public class MeKaBuPinnacle : II2CPeripheral
    {
        private byte[] registers = new byte[32];
        private int address;
        public MeKaBuPinnacle() { Reset(); }
        public void Reset() { System.Array.Clear(registers, 0, registers.Length); registers[0] = 7; }
        public void FinishTransmission() {}
        public byte[] Read(int count)
        {
            var result = new byte[count];
            for (int i = 0; i < count; i++) result[i] = registers[(address + i) & 31];
            return result;
        }
        public void Write(byte[] data)
        {
            for (int i = 0; i < data.Length; i += 2)
            {
                address = data[i] & 31;
                if (i + 1 == data.Length) break;
                registers[address] = data[i + 1];
                if (address == 3 && (data[i + 1] & 1) != 0) registers[2] |= 8;
                if (address == 30) registers[30] = 0;
            }
        }
    }
}
