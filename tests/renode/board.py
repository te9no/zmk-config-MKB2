import hashlib
import json
import socket
import struct
from pathlib import Path

APP_START = 0x27000
SETTINGS_START = 0xEC000
SETTINGS_SIZE = 0x8000

def read_application(path):
    data = path.read_bytes()
    provenance = json.loads(path.with_suffix(".uf2.json").read_text())["artifact"]
    if provenance["name"] != path.name or provenance["sha256"] != hashlib.sha256(data).hexdigest():
        raise ValueError(f"{path}: UF2 provenance mismatch")
    block_size = 512
    payload_size = 256
    block_count, remainder = divmod(len(data), block_size)
    if not block_count or remainder or APP_START + block_count * payload_size > SETTINGS_START:
        raise ValueError(f"{path}: invalid UF2 size")

    payload = bytearray()
    for number in range(block_count):
        start = number * block_size
        block = data[start : start + block_size]
        address = APP_START + number * payload_size
        expected = (0x0A324655, 0x9E5D5157, 0x2000, address, payload_size, number, block_count, 0xADA52840)
        if struct.unpack_from("<8I", block) != expected or block[-4:] != bytes.fromhex("306fb10a"):
            raise ValueError(f"{path}: invalid nRF52840 UF2 block {number}")
        payload.extend(block[32 : 32 + payload_size])
    return payload


def port_number():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]



def board_model(renode):
    here = Path(__file__).resolve().parent
    platform = (renode.parent / "platforms/cpus/nrf52840.repl").read_text()
    platform = platform.replace("spi2: SPI.NRF52840_SPI @ sysbus 0x40023000", "spi2: SPI.NRF52840_SPI @ sysbus 0x40023000\n    easyDMA: true")
    platform += """
ficr: Memory.MappedMemory @ sysbus 0x10000000
    size: 0x1000
uicr: Memory.MappedMemory @ sysbus 0x10001000
    size: 0x1000
saadc: Mocks.NRF52840_StickAdc @ sysbus 0x40007000
    IRQ -> nvic@7
rzt: Mocks.MeKaBuMagnet @ twi0 0x0c
pinnacle: Mocks.MeKaBuPinnacle @ twi0 0x2a
"""
    return platform, f"include @{here}/NRF52840_StickAdc.cs\ninclude @{here}/MeKaBuSensors.cs\n"
