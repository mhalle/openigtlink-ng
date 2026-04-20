/**
 * Transport-neutral types shared between the TCP {@link Client}
 * and the WebSocket {@link WsClient}.
 *
 * Kept in a separate file so the browser-safe `./net/ws` subpath
 * can import these without transitively pulling in Node-specific
 * modules from `./client.ts` (`node:net`, `node:timers/promises`).
 */

import type { Envelope } from "./envelope.js";

/**
 * Minimum shape a generated message class must satisfy to be
 * `send`-able: a `TYPE_ID` on the constructor and an instance
 * `pack()` returning the body bytes. Matches the generated
 * classes in `messages/` without modification.
 */
export interface SendableMessage {
  pack(): Uint8Array;
}

export interface SendableCtor {
  readonly TYPE_ID: string;
}

/**
 * A handler registered via `client.on(Message, handler)`. Receives
 * the typed envelope whose `body` is the decoded instance of the
 * message class it was registered for.
 */
export type Handler<T> = (env: Envelope<T>) => void | Promise<void>;
