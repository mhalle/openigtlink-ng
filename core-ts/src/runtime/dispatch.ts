/**
 * Public registry of message body classes, keyed by wire `type_id`.
 *
 * Built-in message classes (generated in `messages/`) and third-party
 * extension classes (PLUS `TRACKEDFRAME`, vendor-specific types,
 * etc.) share this single table — there is no private registration
 * back door. Registering an extension makes it decode through the
 * exact same codec path the built-ins use.
 *
 * Mirrors the Python `oigtl.messages.registry` API.
 */

import type { Header } from "./header.js";

/**
 * Generated message classes conform to this shape: a constructor
 * with a static `TYPE_ID` and a static `unpack(bytes)` factory.
 * They may also expose `BODY_SIZE` for fixed-body types and a
 * `pack(): Uint8Array` instance method (required for round-trip
 * encoding).
 */
export interface MessageCtor<T = unknown> {
  readonly TYPE_ID: string;
  unpack(bytes: Uint8Array): T;
}

/**
 * Raised when `registerMessageType` would silently overwrite an
 * existing entry. Pass `override: true` to force replacement.
 */
export class RegistryConflictError extends Error {
  constructor(typeId: string, existingName: string) {
    super(
      `type_id ${JSON.stringify(typeId)} is already registered to ` +
        `${existingName}; pass { override: true } to replace it, or ` +
        `pick a different type_id`,
    );
    this.name = "RegistryConflictError";
  }
}

const _registry = new Map<string, MessageCtor>();

/**
 * Register `ctor` as the body class for its declared `TYPE_ID`.
 *
 * After registration, `unpackMessage` / `unpackEnvelope` dispatch
 * wire messages whose header `typeId` matches through
 * `ctor.unpack(body)`.
 *
 * @param ctor - The body class. Must expose `TYPE_ID` (up to 12
 *   ASCII chars per the OpenIGTLink spec) and `unpack(bytes)`.
 * @param opts.override - If true, silently replace any existing
 *   entry. If false (default) and `ctor.TYPE_ID` is already
 *   registered to a different class, throws
 *   {@link RegistryConflictError}. Re-registering the same
 *   `(TYPE_ID, ctor)` pair is always idempotent.
 *
 * @throws {RegistryConflictError} `ctor.TYPE_ID` is already bound
 *   to a different class and `override` is false.
 */
export function registerMessageType(
  ctor: MessageCtor,
  opts: { override?: boolean } = {},
): void {
  const existing = _registry.get(ctor.TYPE_ID);
  if (existing !== undefined && existing !== ctor && !opts.override) {
    const existingName =
      (existing as unknown as { name?: string }).name ?? "anonymous";
    throw new RegistryConflictError(ctor.TYPE_ID, existingName);
  }
  _registry.set(ctor.TYPE_ID, ctor);
}

/**
 * Remove and return the class registered for `typeId`, if any.
 *
 * Primarily useful in tests that need to undo a registration
 * between cases. Production code should not need this.
 */
export function unregisterMessageType(typeId: string): MessageCtor | undefined {
  const prior = _registry.get(typeId);
  _registry.delete(typeId);
  return prior;
}

/**
 * Return the class registered for `typeId`, or `undefined`.
 *
 * Unlike `registerMessageType`, this never throws — callers use
 * `undefined` as the signal to fall back to a raw-body path
 * (see {@link RawBody} in `net/envelope.ts`).
 */
export function lookupMessageClass(typeId: string): MessageCtor | undefined {
  return _registry.get(typeId);
}

/** Sorted list of every currently-registered type_id. */
export function registeredTypes(): string[] {
  return [...(_registry.keys())].sort();
}

/** Number of currently-registered types (tooling / diagnostics). */
export function registrySize(): number {
  return _registry.size;
}

/** Context passed to every typed constructor for error attribution. */
export interface DispatchContext {
  header: Header;
}
