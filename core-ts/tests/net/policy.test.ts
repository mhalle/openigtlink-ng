/**
 * Unit tests for src/net/policy.ts.
 *
 * Covers the spec language (literal, CIDR, range), the
 * always-allow defaults, the malformed-spec rejection, and the
 * Origin allowlist's wildcard-vs-omitted-header semantics.
 *
 * Server-level enforcement (accept-time rejection, max-clients
 * cap) lives in server.test.ts and ws_server.test.ts.
 */

import assert from "node:assert/strict";
import { describe, it } from "node:test";

import {
  buildOriginPolicy,
  buildPeerPolicy,
} from "../../src/net/policy.js";

describe("buildPeerPolicy — defaults", () => {
  it("undefined opts allow everything", () => {
    const p = buildPeerPolicy(undefined);
    assert.equal(p.allows("127.0.0.1"), true);
    assert.equal(p.allows("8.8.8.8"), true);
    assert.equal(p.allows("::1"), true);
  });

  it("empty array allows everything (same as undefined)", () => {
    const p = buildPeerPolicy([]);
    assert.equal(p.allows("8.8.8.8"), true);
  });
});

describe("buildPeerPolicy — spec language", () => {
  it("matches literal IPv4 addresses", () => {
    const p = buildPeerPolicy(["127.0.0.1"]);
    assert.equal(p.allows("127.0.0.1"), true);
    assert.equal(p.allows("127.0.0.2"), false);
  });

  it("matches literal IPv6 addresses", () => {
    const p = buildPeerPolicy(["::1"]);
    assert.equal(p.allows("::1"), true);
    assert.equal(p.allows("::2"), false);
  });

  it("matches IPv4 CIDR subnets", () => {
    const p = buildPeerPolicy(["10.0.0.0/8"]);
    assert.equal(p.allows("10.0.0.1"), true);
    assert.equal(p.allows("10.255.255.255"), true);
    assert.equal(p.allows("11.0.0.0"), false);
    assert.equal(p.allows("9.255.255.255"), false);
  });

  it("matches IPv6 CIDR subnets", () => {
    const p = buildPeerPolicy(["fd00::/8"]);
    assert.equal(p.allows("fd12:3456::1"), true);
    assert.equal(p.allows("fe80::1"), false);
  });

  it("matches inclusive IPv4 ranges", () => {
    const p = buildPeerPolicy(["10.0.0.5-10.0.0.10"]);
    assert.equal(p.allows("10.0.0.4"), false);
    assert.equal(p.allows("10.0.0.5"), true);
    assert.equal(p.allows("10.0.0.7"), true);
    assert.equal(p.allows("10.0.0.10"), true);
    assert.equal(p.allows("10.0.0.11"), false);
  });

  it("composes multiple specs additively", () => {
    const p = buildPeerPolicy(["127.0.0.1", "10.0.0.0/8", "::1"]);
    assert.equal(p.allows("127.0.0.1"), true);
    assert.equal(p.allows("10.5.5.5"), true);
    assert.equal(p.allows("::1"), true);
    assert.equal(p.allows("8.8.8.8"), false);
    assert.equal(p.allows("fe80::1"), false);
  });

  it("strips IPv6 zone ids before matching", () => {
    const p = buildPeerPolicy(["fe80::/10"]);
    assert.equal(p.allows("fe80::1%en0"), true);
  });

  it("never allows malformed peer addresses", () => {
    const p = buildPeerPolicy(["10.0.0.0/8"]);
    assert.equal(p.allows(""), false);
    assert.equal(p.allows("not-an-ip"), false);
    assert.equal(p.allows("999.999.999.999"), false);
  });
});

describe("buildPeerPolicy — malformed specs throw eagerly", () => {
  it("rejects empty spec strings", () => {
    assert.throws(() => buildPeerPolicy([""]), /empty allow spec/);
    assert.throws(() => buildPeerPolicy(["   "]), /empty allow spec/);
  });

  it("rejects non-IP literals", () => {
    assert.throws(
      () => buildPeerPolicy(["example.com"]),
      /not a valid IPv4\/IPv6 address/,
    );
  });

  it("rejects nonsense CIDR prefixes", () => {
    assert.throws(
      () => buildPeerPolicy(["10.0.0.0/abc"]),
      /invalid CIDR prefix/,
    );
    assert.throws(
      () => buildPeerPolicy(["10.0.0.0/64"]),
      /invalid CIDR prefix/,
    );
    assert.throws(
      () => buildPeerPolicy(["10.0.0.0/-1"]),
      /invalid CIDR prefix/,
    );
  });

  it("rejects mixed-family ranges", () => {
    assert.throws(
      () => buildPeerPolicy(["10.0.0.1-::1"]),
      /(mixed-family|not a valid)/,
    );
  });
});

describe("buildOriginPolicy", () => {
  it("undefined opts allow everything (including missing header)", () => {
    const p = buildOriginPolicy(undefined);
    assert.equal(p.allows("https://app.example.com"), true);
    assert.equal(p.allows(undefined), true);
  });

  it("empty array allows everything (same as undefined)", () => {
    const p = buildOriginPolicy([]);
    assert.equal(p.allows("https://anywhere.example"), true);
    assert.equal(p.allows(undefined), true);
  });

  it("configured allowlist requires exact match", () => {
    const p = buildOriginPolicy(["https://app.example.com"]);
    assert.equal(p.allows("https://app.example.com"), true);
    assert.equal(p.allows("https://other.example.com"), false);
    assert.equal(p.allows("http://app.example.com"), false);   // scheme matters
    assert.equal(p.allows("https://app.example.com:443"), false); // port literal
  });

  it("configured allowlist rejects missing Origin header", () => {
    // A non-browser client (no Origin header) is rejected unless
    // "*" is on the list — that's the security-relevant default.
    const p = buildOriginPolicy(["https://app.example.com"]);
    assert.equal(p.allows(undefined), false);
  });

  it("\"*\" wildcard accepts anything including missing headers", () => {
    const p = buildOriginPolicy(["*"]);
    assert.equal(p.allows("https://anywhere.example"), true);
    assert.equal(p.allows(undefined), true);
  });

  it("wildcard mixes with exact entries (wildcard wins)", () => {
    const p = buildOriginPolicy(["*", "https://app.example.com"]);
    assert.equal(p.allows("https://elsewhere.example"), true);
  });
});
