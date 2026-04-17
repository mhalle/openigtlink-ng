/**
 * Typed dispatch registry.
 *
 * Maps wire `type_id` → constructor that takes the content bytes
 * and returns a typed message instance. Populated by the generated
 * `messages/index.ts` on import.
 */

import type { Header } from "./header.js";

/**
 * Generated message classes conform to this shape: a constructor
 * with a static `TYPE_ID` and a static `unpack(bytes)` factory.
 * They may also expose `BODY_SIZE` for fixed-body types.
 */
export interface MessageCtor<T = unknown> {
  readonly TYPE_ID: string;
  unpack(bytes: Uint8Array): T;
}

const registry = new Map<string, MessageCtor>();

export function registerMessage(ctor: MessageCtor): void {
  if (registry.has(ctor.TYPE_ID)) {
    throw new Error(
      `duplicate registration for type_id ${JSON.stringify(ctor.TYPE_ID)}`,
    );
  }
  registry.set(ctor.TYPE_ID, ctor);
}

export function lookup(typeId: string): MessageCtor | undefined {
  return registry.get(typeId);
}

export function registeredTypeIds(): string[] {
  return [...registry.keys()].sort();
}

export function registrySize(): number {
  return registry.size;
}

/** Context passed to every typed constructor for error attribution. */
export interface DispatchContext {
  header: Header;
}
