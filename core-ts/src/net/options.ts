/**
 * Typed options for `Client`. The simplest useful set — timeouts,
 * default device, and a pre-parse size cap. Resilience fields
 * (`autoReconnect`, offline buffer, TCP keepalive) are deferred to
 * a later phase and layer on additively.
 *
 * Duration fields are plain `number` milliseconds — TypeScript has
 * no idiomatic `timedelta`, and `setTimeout` takes ms anyway.
 * Setting a field to `undefined` (the default) means "no limit".
 */

export interface ClientOptions {
  /**
   * Device name written into the header when `send()` isn't given
   * an explicit `deviceName`. OpenIGTLink receivers often route
   * on this.
   * @default "typescript"
   */
  defaultDevice?: string;

  /**
   * Time budget for the initial TCP connect, in milliseconds.
   * `undefined` = no limit (the OS's default connect timeout still
   * applies).
   * @default 10000
   */
  connectTimeoutMs?: number;

  /**
   * Default wall-clock budget for `receive()` / `receiveAny()` when
   * the caller doesn't pass a per-call timeout, in milliseconds.
   * `undefined` = block forever.
   */
  receiveTimeoutMs?: number;

  /**
   * If non-zero, an inbound message with `bodySize` above this cap
   * is rejected before the body bytes are buffered. Pre-parse DoS
   * defence. 0 = no app-level cap (the 64-bit wire field itself
   * still bounds body_size).
   * @default 0
   */
  maxMessageSize?: number;
}

/** The effective configuration after defaults are applied. */
export interface ResolvedClientOptions {
  defaultDevice: string;
  connectTimeoutMs: number | undefined;
  receiveTimeoutMs: number | undefined;
  maxMessageSize: number;
}

export function resolveClientOptions(
  opts: ClientOptions | undefined,
): ResolvedClientOptions {
  return {
    defaultDevice: opts?.defaultDevice ?? "typescript",
    connectTimeoutMs: opts?.connectTimeoutMs ?? 10_000,
    receiveTimeoutMs: opts?.receiveTimeoutMs,
    maxMessageSize: opts?.maxMessageSize ?? 0,
  };
}
