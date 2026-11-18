"""Run the Studio web client's RPC tests against two connected MeKaBu instances."""
import argparse
import subprocess
import tempfile
from pathlib import Path
from board import APP_START, SETTINGS_SIZE, SETTINGS_START, board_model, port_number, read_application

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renode", required=True, type=Path)
    parser.add_argument("--firmware-dir", required=True, type=Path)
    parser.add_argument("--log", required=True, type=Path)
    args = parser.parse_args()
    renode = args.renode.resolve()
    platform, includes = board_model(renode)
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        (root / "board.repl").write_text(platform)
        (root / "blank.bin").write_bytes(b"\xff" * SETTINGS_SIZE)
        port = port_number()
        script = includes + 'emulation CreateUARTHub "split"\n'
        for side in ("left", "right"):
            uf2 = args.firmware_dir / f"MKB-ZeroShift-sim-{side}.uf2"
            (root / side).write_bytes(read_application(uf2))
            script += f'''mach create "{side}"
machine LoadPlatformDescription @{root}/board.repl
sysbus WriteDoubleWord 0x10000010 4096
sysbus WriteDoubleWord 0x10000014 256
sysbus WriteDoubleWord 0x10001200 18
sysbus WriteDoubleWord 0x10001204 18
sysbus WriteDoubleWord 0x1000120c 0xfffffffe
sysbus LoadBinary @{root}/blank.bin {SETTINGS_START:#x}
sysbus LoadBinary @{root}/{side} {APP_START:#x}
sysbus.cpu VectorTableOffset {APP_START:#x}
connector Connect sysbus.uart1 split
'''
        script += f'mach set "right"\nuart0 CreateFileBackend @{args.log.resolve()}.right-uart\n'
        script += f'''mach set "left"
emulation CreateServerSocketTerminal {port} "studio" false
connector Connect sysbus.uart0 studio
start
'''
        (root / "test.resc").write_text(script)
        with args.log.open("w") as log, subprocess.Popen(
                [renode, "--disable-xwt", "--console", "--plain", root / "test.resc"],
                stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT, text=True) as emu:
            try:
                import time
                for _ in range(300):
                    if "Machine started" in args.log.read_text(errors="replace"):
                        break
                    time.sleep(0.05)
                with subprocess.Popen(["node", Path(__file__).with_suffix(".mjs"), str(port)],
                                      stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True) as client:
                    for line in client.stdout:
                        if line.strip() in ("RESET left", "RESET right"):
                            side = line.split()[1]
                            offset = args.log.stat().st_size
                            emu.stdin.write(f'pause\nmach set "{side}"\nmachine Reset\nsysbus.cpu VectorTableOffset {APP_START:#x}\nstart\n')
                            emu.stdin.flush()
                            with args.log.open() as events:
                                events.seek(offset)
                                trace = ""
                                for _ in range(200):
                                    trace += events.read()
                                    if f"{side}/cpu: Setting initial values:" in trace:
                                        break
                                    time.sleep(0.05)
                                else:
                                    raise TimeoutError(f"{side}: reset was not observed")
                            client.stdin.write("ok\n")
                            client.stdin.flush()
                        else:
                            print(line, end="", flush=True)
                    if client.wait():
                        emu.stdin.write("pause\nmach set 0\nsysbus.cpu PC\nmach set 1\nsysbus.cpu PC\n")
                        emu.stdin.flush()
                        time.sleep(1)
                        raise RuntimeError("Studio client assertions failed")
            finally:
                emu.kill()

if __name__ == "__main__":
    main()
