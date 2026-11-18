import {Request, Response} from "./node_modules/@zmkfirmware/zmk-studio-ts-client/src/studio.ts";
import {get_encoder, get_decoder} from "./node_modules/@zmkfirmware/zmk-studio-ts-client/src/framing.ts";
import protobuf from "protobufjs";
import schema from "../proto/modules.proto";

const root = protobuf.parse(schema).root;
const request = root.lookupType("mekabu.Request");
const response = root.lookupType("mekabu.Response");
const names = ["off", "pmw3610", "stick", "encoder", "key", "rzt", "tpd", "lpps"];

export function moduleSession(transport) {
  const signal = transport.abortController.signal;
  const tx = new TransformStream(get_encoder());
  const rx = new TransformStream(get_decoder());
  const pipes = Promise.allSettled([
    tx.readable.pipeTo(transport.writable, {signal, preventAbort: true}),
    transport.readable.pipeTo(rx.writable, {signal}),
  ]);
  const reader = rx.readable.getReader();
  const writer = tx.writable.getWriter();
  let sequence = 0;
  let index;
  async function invoke(custom) {
    const requestId = ++sequence;
    const timer = setTimeout(() => transport.abortController.abort(
      new Error("通信がタイムアウトしました。本体に再接続してください。")), 6000);
    try {
      await writer.write(Request.encode({requestId, custom}).finish());
      for (;;) {
        const {value, done} = await reader.read();
        if (done) throw new Error("接続が切れました。");
        const reply = Response.decode(value).requestResponse;
        if (reply?.requestId === requestId) return reply.custom;
      }
    } finally {
      clearTimeout(timer);
    }
  }
  return {
    async profile(side, mode) {
      if (index === undefined) {
        const list = await invoke({listCustomSubsystems: {}});
        index = list?.listCustomSubsystems?.subsystems.find(s => s.identifier === "mekabu__modules")?.index;
        if (index === undefined) throw new Error("MeKaBu Studio 対応の左側 UF2 を確認してください。");
      }
      const id = names.indexOf(mode);
      if (mode !== undefined && id < 0) throw new Error("未対応のモジュールです。");
      const operation = mode === undefined ? {get: true} : {set: id};
      const payload = request.encode({right: side === "right", ...operation}).finish();
      const result = (await invoke({call: {subsystemIndex: index, payload}}))?.call;
      if (result?.subsystemIndex !== index) throw new Error("本体からの応答を確認できませんでした。");
      const {saved, active, error} = response.decode(result.payload);
      if (side === "right" && [-110, -116, -11].includes(error)) {
        throw new Error("右側の応答を確認できません。電源と左右の接続を確認して再読込してください。");
      }
      if (error || !names[saved] || !names[active]) throw new Error("設定を確認できませんでした（" + error + "）。");
      return {side, saved: names[saved], active: names[active]};
    },
    async close() {
      transport.abortController.abort();
      await pipes;
      reader.releaseLock();
      writer.releaseLock();
    },
  };
}
