/**
 * @openigtlink/core — typed TypeScript wire codec for OpenIGTLink.
 *
 * The two main entry points:
 *
 *   import { Transform, parseMessage } from "@openigtlink/core";
 *   import { crc64, unpackHeader } from "@openigtlink/core/runtime";
 */

export * from "./runtime/index.js";
// Generated message classes live in ./messages/ and are exported
// from messages/index.js once Phase 3 codegen runs.
