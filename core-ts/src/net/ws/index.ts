/**
 * Browser-safe WebSocket entry point.
 *
 * `@openigtlink/core/net/ws` is the subpath a browser bundle
 * imports. It exports only the WebSocket transport and the
 * transport-neutral types (Envelope, RawBody, errors) — no
 * Node-specific imports, so bundlers like Vite/Webpack/esbuild
 * can ship it to the browser without polyfills.
 *
 * Node-side consumers that also need the TCP `Client` should
 * import from `@openigtlink/core/net` instead.
 *
 * Example:
 *
 *     import { WsClient } from "@openigtlink/core/net/ws";
 *     import { Transform, Status } from "@openigtlink/core/messages";
 *
 *     const c = await WsClient.connect("ws://tracker.lab:18945/");
 *     await c.send(new Transform({ matrix: [1,0,0, 0,1,0, 0,0,1, 0,0,0] }));
 *     const reply = await c.receive(Status);
 *     console.log(reply.body.statusMessage);
 *     await c.close();
 */

export {
  WsClient,
  type WebSocketLike,
  type WebSocketLikeCtor,
  type WsClientOptions,
} from "../ws_client.js";

export type { Handler, SendableCtor, SendableMessage } from "../types.js";
export type { ClientOptions, ResolvedClientOptions } from "../options.js";
export { resolveClientOptions } from "../options.js";
export { RawBody, type Envelope } from "../envelope.js";
export {
  ConnectionClosedError,
  FramingError,
  TransportError,
  TransportTimeoutError,
} from "../errors.js";

// Pure codec + registry — lets browser consumers decode raw bytes
// (e.g. from an MQTT-over-WSS payload) without reaching through
// WsClient. Also re-exports registerMessageType so extension
// registration works without a separate import from the barrel.
export {
  packEnvelope,
  unpackEnvelope,
  unpackMessage,
  type UnpackOptions,
} from "../../codec.js";
export {
  HEADER_SIZE,
  type Header,
  packHeader,
  unpackHeader,
} from "../../runtime/header.js";
export {
  RegistryConflictError,
  lookupMessageClass,
  registerMessageType,
  registeredTypes,
  unregisterMessageType,
  type MessageCtor,
} from "../../runtime/dispatch.js";
