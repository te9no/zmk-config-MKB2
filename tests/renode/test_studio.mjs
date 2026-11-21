import assert from "node:assert/strict";
import net from "node:net";
import {Duplex} from "node:stream";
import {once} from "node:events";
import {createInterface} from "node:readline/promises";
import {setTimeout as delay} from "node:timers/promises";
const sharedClient = process.env.ZEROSHIFT_SHARED_RPC;
if (!sharedClient) throw new Error("Set ZEROSHIFT_SHARED_RPC to the built ZeroShift rpc.mjs path.");
const {moduleSession} = await import(sharedClient);

const Mode = {
  off: 0,
  pmw3610: 1,
  stick: 2,
  encoder: 3,
  key: 4,
  rzt: 5,
  tpd: 6,
  lpps: 7,
  pat9125: 8,
};

const socket = net.connect(Number(process.argv[2]), "127.0.0.1");
await once(socket, "connect");
const transport = {...Duplex.toWeb(socket), abortController: new AbortController()};
const client = moduleSession(transport);
const session = {
  close: () => client.close(),
  async profile(side, mode) {
    const state = await client.profile(side, mode === undefined ? undefined : [mode]);
    assert.equal(state.model, "MeKaBu");
    assert.equal(state.slots.length, 1);
    return state.slots[0];
  },
};
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
  let previous = Mode.off;
  check(await readAfterBoot(side), previous, previous);
  for (const mode of [Mode.key, Mode.encoder, Mode.stick, Mode.pmw3610, Mode.rzt, Mode.pat9125, Mode.tpd, Mode.lpps, Mode.off]) {
    console.log(side, "set", mode);
    check(await session.profile(side, mode), mode, previous);
    check(await readAfterBoot(side === "left" ? "right" : "left"), Mode.off, Mode.off);
    await reset(side);
    const state = await readAfterBoot(side);
    console.log(side, "saved:", state.saved, "active:", state.active);
    const active = mode === Mode.pat9125 ? Mode.off : mode;
    check(state, mode, active);
    previous = active;
  }
}
check(await session.profile("left", Mode.encoder), Mode.encoder, Mode.off);
check(await session.profile("right", Mode.key), Mode.key, Mode.off);
await reset("right");
check(await readAfterBoot("right"), Mode.key, Mode.key);
check(await readAfterBoot("left"), Mode.encoder, Mode.off);
await reset("left");
check(await readAfterBoot("left"), Mode.encoder, Mode.encoder);
check(await readAfterBoot("right"), Mode.key, Mode.key);
console.log("18/18 reboot cases passed; independent left encoder and right key passed");
await session.close();
socket.destroy();
process.exit(0);
