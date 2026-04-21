/**
 * Tests for `@openigtlink/core`'s registry API.
 *
 * The registry is the pivot that makes built-in types and
 * user-supplied extension types indistinguishable to the codec.
 * These tests exercise both halves: confirming the built-ins are
 * visible through the public API, and proving that third-party
 * registrations follow exactly the same decode path.
 *
 * Parallels `core-py/tests/test_registry.py`.
 */

import assert from "node:assert/strict";
import { afterEach, describe, it } from "node:test";

import "../src/messages/index.js";   // side effect: register built-ins
import {
  RegistryConflictError,
  lookupMessageClass,
  packEnvelope,
  packHeader,
  registerMessageType,
  registeredTypes,
  unpackEnvelope,
  unregisterMessageType,
} from "../src/index.js";

// ---------------------------------------------------------------------------
// A minimal extension class that satisfies the MessageCtor contract.
// ---------------------------------------------------------------------------

class FakeBody {
  static readonly TYPE_ID = "EXT_TEST";   // ≤ 12 ASCII chars

  constructor(public readonly value: number) {}

  static unpack(body: Uint8Array): FakeBody {
    if (body.length !== 4) {
      throw new Error(`expected 4 bytes, got ${body.length}`);
    }
    const view = new DataView(body.buffer, body.byteOffset, body.byteLength);
    return new FakeBody(view.getUint32(0, true));
  }

  pack(): Uint8Array {
    const out = new Uint8Array(4);
    const view = new DataView(out.buffer);
    view.setUint32(0, this.value, true);
    return out;
  }
}

class OtherFakeBody {
  static readonly TYPE_ID = FakeBody.TYPE_ID;
  static unpack(_body: Uint8Array): OtherFakeBody {
    return new OtherFakeBody();
  }
  pack(): Uint8Array {
    return new Uint8Array(0);
  }
}

// Always clean up extension registrations to keep tests order-independent.
afterEach(() => {
  unregisterMessageType("EXT_TEST");
});

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function wrapBody(typeId: string, body: Uint8Array): Uint8Array {
  const header = packHeader({
    version: 2,
    typeId,
    deviceName: "dev",
    timestamp: 0n,
    body,
  });
  const out = new Uint8Array(header.length + body.length);
  out.set(header, 0);
  out.set(body, header.length);
  return out;
}

// ---------------------------------------------------------------------------
// Built-ins are visible through the public API
// ---------------------------------------------------------------------------

describe("built-in registration", () => {
  it("registered types include the canonical built-ins", () => {
    const types = registeredTypes();
    for (const expected of [
      "TRANSFORM", "STATUS", "IMAGE", "COMMAND",
      "POSITION", "POLYDATA", "STRING",
    ]) {
      assert.ok(
        types.includes(expected),
        `expected ${expected} in registeredTypes()`,
      );
    }
  });

  it("lookupMessageClass returns a registered built-in", () => {
    const ctor = lookupMessageClass("TRANSFORM");
    assert.ok(ctor !== undefined);
    assert.equal(ctor.TYPE_ID, "TRANSFORM");
  });

  it("lookupMessageClass returns undefined for unknown types", () => {
    assert.equal(lookupMessageClass("NOT_REAL"), undefined);
  });
});

// ---------------------------------------------------------------------------
// Extension registration — the main event
// ---------------------------------------------------------------------------

describe("extension registration", () => {
  it("registered extension decodes through unpackEnvelope", () => {
    registerMessageType(FakeBody);

    const payload = new FakeBody(0xcafebabe).pack();
    const wire = wrapBody("EXT_TEST", payload);
    const env = unpackEnvelope(wire);

    assert.ok(env.body instanceof FakeBody);
    assert.equal((env.body as FakeBody).value, 0xcafebabe);
    assert.equal(env.header.typeId, "EXT_TEST");
  });

  it("registered extension round-trips through packEnvelope", () => {
    registerMessageType(FakeBody);

    const wire = wrapBody("EXT_TEST", new FakeBody(42).pack());
    const env = unpackEnvelope(wire);

    assert.deepEqual(packEnvelope(env), wire);
  });
});

// ---------------------------------------------------------------------------
// Collision detection and override
// ---------------------------------------------------------------------------

describe("collision detection", () => {
  it("duplicate registration throws RegistryConflictError", () => {
    registerMessageType(FakeBody);
    assert.throws(
      () => registerMessageType(OtherFakeBody),
      RegistryConflictError,
    );
  });

  it("re-registering the same class is idempotent", () => {
    registerMessageType(FakeBody);
    registerMessageType(FakeBody);   // no throw
    assert.equal(lookupMessageClass("EXT_TEST"), FakeBody);
  });

  it("override: true replaces the existing entry", () => {
    registerMessageType(FakeBody);
    registerMessageType(OtherFakeBody, { override: true });
    assert.equal(lookupMessageClass("EXT_TEST"), OtherFakeBody);
  });

  it("built-in collisions are protected without override", () => {
    class Imposter {
      static readonly TYPE_ID = "TRANSFORM";
      static unpack(_body: Uint8Array): Imposter {
        return new Imposter();
      }
      pack(): Uint8Array {
        return new Uint8Array();
      }
    }
    assert.throws(() => registerMessageType(Imposter), RegistryConflictError);
  });
});

// ---------------------------------------------------------------------------
// unregisterMessageType
// ---------------------------------------------------------------------------

describe("unregisterMessageType", () => {
  it("returns the previously-registered class", () => {
    registerMessageType(FakeBody);
    const prior = unregisterMessageType("EXT_TEST");
    assert.equal(prior, FakeBody);
    assert.equal(lookupMessageClass("EXT_TEST"), undefined);
  });

  it("returns undefined for a never-registered type_id", () => {
    assert.equal(unregisterMessageType("NEVER_REGISTERED"), undefined);
  });
});
