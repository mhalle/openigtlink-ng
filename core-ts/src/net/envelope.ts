/**
 * A received message and its surrounding wire header.
 *
 * The header carries everything a dispatcher needs to route on —
 * `deviceName`, `timestamp`, `typeId`. The body is the decoded
 * typed message (when the `typeId` is registered) or a
 * {@link RawBody} sentinel (when it isn't).
 *
 * Kept generic so `c.receive(Transform)` can return
 * `Envelope<Transform>` and TypeScript will type-check
 * `env.body.matrix`.
 */

import type { Header } from "../runtime/header.js";

export interface Envelope<T> {
  readonly header: Header;
  readonly body: T;
}

/**
 * Sentinel body for wire messages whose `typeId` isn't in the
 * registry. Surfaced through `Client.onUnknown` so loggers or
 * forwarders can still see the bytes without a decoder.
 */
export class RawBody {
  constructor(
    public readonly typeId: string,
    public readonly bytes: Uint8Array,
  ) {}
}
