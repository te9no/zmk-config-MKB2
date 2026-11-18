// UUIDs shared with the pinned ZMK Studio client and firmware.
const service = "00000000-0196-6107-c967-c5cfb1c2482a";
const rpc = "00000001-0196-6107-c967-c5cfb1c2482a";

export async function openBluetooth() {
  const device = await navigator.bluetooth.requestDevice({
    filters: [{namePrefix: "MeKaBu"}, {services: [service]}],
    optionalServices: [service],
  });
  const abortController = new AbortController();
  const disconnected = () => abortController.abort(new Error("Bluetooth 接続が切れました。"));
  device.addEventListener("gattserverdisconnected", disconnected);
  let characteristic;
  let stream;
  function receive(event) {
    const data = event.target.value;
    stream.enqueue(new Uint8Array(data.buffer, data.byteOffset, data.byteLength).slice());
  }
  function close() {
    device.removeEventListener("gattserverdisconnected", disconnected);
    characteristic?.removeEventListener("characteristicvaluechanged", receive);
    if (device.gatt?.connected) device.gatt.disconnect();
  }
  abortController.signal.addEventListener("abort", close, {once: true});
  try {
    const server = await device.gatt.connect();
    characteristic = await (await server.getPrimaryService(service)).getCharacteristic(rpc);
    const readable = new ReadableStream({start(controller) { stream = controller; }});
    characteristic.addEventListener("characteristicvaluechanged", receive);
    await characteristic.startNotifications();
    abortController.signal.throwIfAborted();
    const writable = new WritableStream({
      async write(bytes) {
        // The default ATT MTU permits 20 bytes of characteristic payload.
        for (let i = 0; i < bytes.length; i += 20) {
          const chunk = bytes.subarray(i, i + 20);
          await characteristic.writeValueWithResponse(chunk);
        }
      },
    });
    return {readable, writable, abortController};
  } catch (error) {
    close();
    throw error;
  }
}
