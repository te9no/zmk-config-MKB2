import assert from "node:assert/strict";
import net from "node:net";
import {Duplex} from "node:stream";
import {once} from "node:events";
import {createInterface} from "node:readline/promises";
import {setTimeout as delay} from "node:timers/promises";
import {moduleSession} from "../../studio-web/dist/rpc.mjs";

const socket = net.connect(Number(process.argv[2]), "127.0.0.1");
await once(socket, "connect");
const transport = {...Duplex.toWeb(socket), abortController: new AbortController()};
const session = moduleSession(transport);
const monitor = createInterface({input: process.stdin, output: process.stdout});
async function reset(side) {
  await monitor.question("RESET " + side + "\n");
  await delay(1800);
}
function check(state, saved, active) {
  assert.equal(state.saved, saved);
  assert.equal(state.active, active);
}
async function readAfterBoot(side) {
  // The wired transport becomes available after its heartbeat handshake.
  for (let attempt = 0; ; attempt++) {
    try {
      return await session.profile(side);
    } catch (error) {
      if (side !== "right" || !error.message.includes("右側の応答") || attempt >= 4) throw error;
      await delay(500);
    }
  }
}
await delay(1500);
for (const side of ["right", "left"]) {
  let previous = "off";
  check(await readAfterBoot(side), previous, previous);
  for (const mode of ["key", "encoder", "stick", "pmw3610", "rzt", "tpd", "lpps", "off"]) {
    console.log(side, "set", mode);
    check(await session.profile(side, mode), mode, previous);
    check(await readAfterBoot(side === "left" ? "right" : "left"), "off", "off");
    await reset(side);
    const state = await readAfterBoot(side);
    console.log(side, "saved:", state.saved, "active:", state.active);
    check(state, mode, mode);
    previous = mode;
  }
}
check(await session.profile("left", "encoder"), "encoder", "off");
check(await session.profile("right", "key"), "key", "off");
await reset("right");
check(await readAfterBoot("right"), "key", "key");
check(await readAfterBoot("left"), "encoder", "off");
await reset("left");
check(await readAfterBoot("left"), "encoder", "encoder");
check(await readAfterBoot("right"), "key", "key");
console.log("16/16 reboot cases passed; independent left encoder and right key passed");
await session.close();
socket.destroy();
process.exit(0);
