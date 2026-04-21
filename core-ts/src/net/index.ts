/**
 * `@openigtlink/core/net` — Node transport layer.
 *
 * The simplest useful port: a TCP client that speaks the same
 * wire protocol as `oigtl::Client` (C++) and `oigtl.net.Client`
 * (Python). Resilience (auto-reconnect, offline buffer, keepalive),
 * server-side accept loops, and a browser-friendly WebSocket
 * transport are future phases that layer on top of the framer
 * shipped here.
 *
 * Example:
 *
 *     import { Client } from "@openigtlink/core/net";
 *     import { Transform, Status } from "@openigtlink/core/messages";
 *
 *     const c = await Client.connect("tracker.lab", 18944);
 *     await c.send(new Transform({ matrix: [1,0,0, 0,1,0, 0,0,1, 0,0,0] }));
 *     const reply = await c.receive(Status);
 *     console.log(reply.body.statusMessage);
 *     await c.close();
 */

export { Client } from "./client.js";
export {
  WsClient,
  type WebSocketLike,
  type WebSocketLikeCtor,
  type WsClientOptions,
} from "./ws_client.js";
export type { Handler, SendableCtor, SendableMessage } from "./types.js";
export type { ClientOptions, ResolvedClientOptions } from "./options.js";
export { resolveClientOptions } from "./options.js";
export { RawBody, type Envelope } from "./envelope.js";
export { ByteAccumulator, V3Framer, makeV3Framer, type Incoming } from "./framer.js";
export {
  ConnectionClosedError,
  FramingError,
  TransportError,
  TransportTimeoutError,
} from "./errors.js";

// Pure codec + registry API. Imported from the net barrel so
// callers who're already reaching in for Client/WsClient have
// decode/encode and registration in the same import.
export {
  packEnvelope,
  unpackEnvelope,
  unpackMessage,
  type UnpackOptions,
} from "../codec.js";
export {
  RegistryConflictError,
  lookupMessageClass,
  registerMessageType,
  registeredTypes,
  unregisterMessageType,
  type MessageCtor,
} from "../runtime/dispatch.js";
