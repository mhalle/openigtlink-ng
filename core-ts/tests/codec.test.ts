/**
 * Tests for `@openigtlink/core`'s pure wire codec.
 *
 * Exercises the public unpack/pack surface over built-in message
 * types, the loose/strict unknown-type split, CRC verification,
 * and framing validity. Parallels `core-py/tests/test_codec.py`.
 */

import assert from "node:assert/strict";
import { describe, it } from "node:test";

import "../src/messages/index.js";   // side effect: register built-ins
import {
  HEADER_SIZE,
  packEnvelope,
  packHeader,
  unpackEnvelope,
  unpackHeader,
  unpackMessage,
  RawBody,
} from "../src/index.js";
import { Status } from "../src/messages/status.js";
import { Transform } from "../src/messages/transform.js";
import {
  CrcMismatchError,
  MalformedMessageError,
  ShortBufferError,
  UnknownMessageTypeError,
} from "../src/runtime/errors.js";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function wrap(
  bodyBytes: Uint8Array,
  opts: {
    typeId: string;
    deviceName?: string;
    timestamp?: bigint;
    version?: number;
  },
): Uint8Array {
  const header = packHeader({
    version: opts.version ?? 2,
    typeId: opts.typeId,
    deviceName: opts.deviceName ?? "dev",
    timestamp: opts.timestamp ?? 0n,
    body: bodyBytes,
  });
  const out = new Uint8Array(header.length + bodyBytes.length);
  out.set(header, 0);
  out.set(bodyBytes, header.length);
  return out;
}

function makeTransform(): Transform {
  // OpenIGTLink TRANSFORM body is a 12-element affine (the bottom
  // [0, 0, 0, 1] row is implicit).
  return new Transform({
    matrix: [1, 0, 0,
             0, 1, 0,
             0, 0, 1,
             11, 22, 33],
  });
}

function makeStatus(): Status {
  return new Status({
    code: 1,
    subcode: 0n,
    error_name: "",
    status_message: "ok",
  });
}

// ---------------------------------------------------------------------------
// Round-trip — the property that justifies everything else.
// ---------------------------------------------------------------------------

describe("codec round-trip", () => {
  it("unpackEnvelope then packEnvelope is byte-identical for TRANSFORM", () => {
    const tx = makeTransform();
    const wire = wrap(tx.pack(), { typeId: "TRANSFORM" });
    const env = unpackEnvelope(wire);
    assert.ok(env.body instanceof Transform);
    assert.equal(env.header.typeId, "TRANSFORM");
    assert.deepEqual(packEnvelope(env), wire);
  });

  it("round-trips STATUS with custom device_name and timestamp", () => {
    const st = makeStatus();
    const wire = wrap(st.pack(), {
      typeId: "STATUS",
      deviceName: "Tracker1",
      timestamp: 1_700_000_000_000_000_000n,
    });
    const env = unpackEnvelope(wire);
    assert.ok(env.body instanceof Status);
    assert.equal((env.body as Status).code, 1);
    assert.equal(env.header.deviceName, "Tracker1");
    assert.deepEqual(packEnvelope(env), wire);
  });

  it("two-step unpackHeader + unpackMessage matches one-step unpackEnvelope", () => {
    const wire = wrap(makeTransform().pack(), { typeId: "TRANSFORM" });
    const header = unpackHeader(wire.subarray(0, HEADER_SIZE));
    const body = wire.subarray(
      HEADER_SIZE,
      HEADER_SIZE + Number(header.bodySize),
    );
    const twoStep = unpackMessage(header, body);
    const oneStep = unpackEnvelope(wire);
    assert.deepEqual(twoStep.header, oneStep.header);
    // Bodies are pydantic/plain objects — compare their packed form.
    assert.deepEqual(
      (twoStep.body as Transform).pack(),
      (oneStep.body as Transform).pack(),
    );
  });
});

// ---------------------------------------------------------------------------
// Loose / strict on unknown type_ids
// ---------------------------------------------------------------------------

describe("unknown type_id dispatch", () => {
  const fabricated = new Uint8Array([0, 1, 2, 3]);

  it("strict mode throws UnknownMessageTypeError", () => {
    const wire = wrap(fabricated, { typeId: "NOSUCHTYPE" });
    assert.throws(() => unpackEnvelope(wire), UnknownMessageTypeError);
  });

  it("loose mode returns a RawBody envelope", () => {
    const wire = wrap(fabricated, { typeId: "NOSUCHTYPE" });
    const env = unpackEnvelope(wire, { loose: true });
    assert.ok(env.body instanceof RawBody);
    assert.equal((env.body as RawBody).typeId, "NOSUCHTYPE");
    assert.deepEqual((env.body as RawBody).bytes, fabricated);
  });

  it("RawBody re-packs to the original wire bytes", () => {
    const body = new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f]);
    const wire = wrap(body, { typeId: "NOSUCHTYPE" });
    const env = unpackEnvelope(wire, { loose: true });
    assert.deepEqual(packEnvelope(env), wire);
  });
});

// ---------------------------------------------------------------------------
// CRC verification
// ---------------------------------------------------------------------------

describe("CRC verification", () => {
  it("bad CRC throws when verification is on (default)", () => {
    const wire = new Uint8Array(wrap(makeTransform().pack(), {
      typeId: "TRANSFORM",
    }));
    // CRC lives in the last 8 bytes of the 58-byte header.
    const i = HEADER_SIZE - 1;
    wire[i] = (wire[i] ?? 0) ^ 0xff;
    assert.throws(() => unpackEnvelope(wire), CrcMismatchError);
  });

  it("bad CRC is silent when verifyCrc: false", () => {
    const wire = new Uint8Array(wrap(makeTransform().pack(), {
      typeId: "TRANSFORM",
    }));
    const i = HEADER_SIZE - 1;
    wire[i] = (wire[i] ?? 0) ^ 0xff;
    const env = unpackEnvelope(wire, { verifyCrc: false });
    assert.ok(env.body instanceof Transform);
  });
});

// ---------------------------------------------------------------------------
// Framing validity
// ---------------------------------------------------------------------------

describe("framing validation", () => {
  it("too-short-for-header throws ShortBufferError", () => {
    assert.throws(
      () => unpackEnvelope(new Uint8Array(HEADER_SIZE - 1)),
      ShortBufferError,
    );
  });

  it("truncated body throws ShortBufferError", () => {
    const wire = wrap(makeTransform().pack(), { typeId: "TRANSFORM" });
    assert.throws(
      () => unpackEnvelope(wire.subarray(0, wire.length - 1)),
      ShortBufferError,
    );
  });

  it("trailing bytes throw MalformedMessageError", () => {
    const wire = wrap(makeTransform().pack(), { typeId: "TRANSFORM" });
    const extended = new Uint8Array(wire.length + 1);
    extended.set(wire, 0);
    extended[wire.length] = 0x99;
    assert.throws(() => unpackEnvelope(extended), MalformedMessageError);
  });

  it("unpackMessage rejects body length mismatch", () => {
    const wire = wrap(makeTransform().pack(), { typeId: "TRANSFORM" });
    const header = unpackHeader(wire.subarray(0, HEADER_SIZE));
    const body = wire.subarray(
      HEADER_SIZE,
      HEADER_SIZE + Number(header.bodySize),
    );
    assert.throws(
      () => unpackMessage(header, body.subarray(0, body.length - 1)),
      MalformedMessageError,
    );
  });
});

// ---------------------------------------------------------------------------
// pack_envelope determinism
// ---------------------------------------------------------------------------

describe("packEnvelope determinism", () => {
  it("recomputes CRC from body even if header.crc is stale", () => {
    const wire = wrap(makeTransform().pack(), { typeId: "TRANSFORM" });
    const env = unpackEnvelope(wire);

    // Clone with a bogus CRC — pack_envelope should ignore and
    // recompute.
    const staleEnv = {
      header: { ...env.header, crc: 0xdeadbeefdeadbeefn },
      body: env.body,
    };
    const repacked = packEnvelope(staleEnv);

    // Round-trips through strict decoder.
    const env2 = unpackEnvelope(repacked);
    assert.equal(env2.header.typeId, "TRANSFORM");
    // And byte-identical to the original.
    assert.deepEqual(repacked, wire);
  });
});
