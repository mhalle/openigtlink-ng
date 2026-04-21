/**
 * @openigtlink/core — typed TypeScript wire codec for OpenIGTLink.
 *
 * Three concentric layers:
 *
 * - **Wire codec** (pure, transport-independent): `unpackEnvelope`,
 *   `unpackMessage`, `unpackHeader`, `packEnvelope`, `packHeader`,
 *   `RawBody`. Sufficient on its own for MQTT payloads, file
 *   replays, browser-bundled consumers, and any other caller that
 *   already holds bytes in memory.
 *
 * - **Messages + registry**: the built-in typed message classes
 *   (`Transform`, `Status`, ...) and the public
 *   `registerMessageType` / `lookupMessageClass` API. Built-ins
 *   and extensions share a single dispatch path.
 *
 * - **Transports**: see `@openigtlink/core/net` (Node TCP and
 *   WebSocket clients) and `@openigtlink/core/net/ws`
 *   (browser-safe WebSocket client).
 *
 * Example (pure codec, no transport):
 *
 *     import { unpackEnvelope, packEnvelope } from "@openigtlink/core";
 *     import "@openigtlink/core/messages";   // side effect: register built-ins
 *
 *     const env = unpackEnvelope(wireBytes);
 *     if (env.header.typeId === "TRANSFORM") { ... }
 *     const roundTrip = packEnvelope(env);   // === wireBytes
 *
 * Extending the protocol:
 *
 *     import { registerMessageType } from "@openigtlink/core";
 *
 *     class TrackedFrame {
 *       static TYPE_ID = "TRACKEDFRAME";
 *       static unpack(body: Uint8Array): TrackedFrame { ... }
 *       pack(): Uint8Array { ... }
 *     }
 *     registerMessageType(TrackedFrame);
 */

export * from "./runtime/index.js";
export {
  packEnvelope,
  unpackEnvelope,
  unpackMessage,
  type UnpackOptions,
} from "./codec.js";
export { RawBody, type Envelope } from "./net/envelope.js";
// Generated message classes live in ./messages/ and are exported
// from messages/index.js once Phase 3 codegen runs.
