/**
 * Accept-time policy for inbound connections (Node-only).
 *
 * Mirrors the role of `core-py`'s `oigtl.net.policy.PeerPolicy`,
 * scaled down to what the TS port needs today: a peer-IP allowlist
 * for TCP, a peer-IP **and** Origin allowlist for WebSocket. Both
 * are optional and default to "accept anything", matching the
 * pre-policy behaviour.
 *
 * Checks run at accept time, before any IGTL byte is read. A
 * blocked peer is rejected by closing the underlying socket (TCP)
 * or by failing the WebSocket upgrade with HTTP 403 (WS), so the
 * allowlists also serve as a pre-parse DoS mitigation layer.
 *
 * Built on Node's standard `net.BlockList` — reach in and use it
 * directly if the spec language here is too narrow. We expose a
 * compact wrapper rather than the raw class so the server option
 * surface stays declarative (`allow: ["127.0.0.1", ...]`) and
 * doesn't require an imperative builder for the simple cases.
 */

import { BlockList, isIPv4, isIPv6 } from "node:net";

// ---------------------------------------------------------------------------
// IP allowlist
// ---------------------------------------------------------------------------

/**
 * Result of compiling a list of CIDR / range / address specs into
 * a callable predicate.
 */
export interface PeerPolicy {
  /**
   * Is `ip` allowed? Empty/unset policies allow everything; a
   * configured policy denies everything not explicitly listed.
   * Returns `false` for malformed inputs (defensive — never throw
   * during accept).
   */
  allows(ip: string): boolean;
}

const ALLOW_ALL: PeerPolicy = { allows: () => true };

/**
 * Compile peer-IP allow specs.
 *
 * Each spec is one of:
 *
 *   - A single literal address: `"127.0.0.1"`, `"::1"`
 *   - A CIDR: `"10.0.0.0/8"`, `"fd00::/8"`
 *   - An inclusive range: `"10.0.0.1-10.0.0.99"`
 *
 * Hostnames are NOT resolved here — pass IPs only. If your
 * deployment has DNS-based access lists, do the lookup at boot
 * and pass the resolved addresses.
 *
 * Throws `TypeError` on a malformed spec, eagerly at server
 * construction. The returned predicate never throws.
 *
 * Pass `undefined` or `[]` to opt out (returns the always-allow
 * policy without allocating a `BlockList`).
 */
export function buildPeerPolicy(
  specs: readonly string[] | undefined,
): PeerPolicy {
  if (specs === undefined || specs.length === 0) {
    return ALLOW_ALL;
  }
  const list = new BlockList();
  for (const spec of specs) {
    addSpec(list, spec);
  }
  return {
    allows(ip: string): boolean {
      const stripped = stripZone(ip);
      const family = isIPv4(stripped)
        ? "ipv4"
        : isIPv6(stripped)
        ? "ipv6"
        : null;
      if (family === null) return false;
      try {
        return list.check(stripped, family);
      } catch {
        return false;
      }
    },
  };
}

function addSpec(list: BlockList, raw: string): void {
  const spec = raw.trim();
  if (spec.length === 0) {
    throw new TypeError("empty allow spec");
  }

  const slash = spec.indexOf("/");
  if (slash >= 0) {
    const addr = spec.slice(0, slash);
    const prefixStr = spec.slice(slash + 1);
    const prefix = Number(prefixStr);
    if (
      !Number.isInteger(prefix) ||
      prefix < 0 ||
      prefix > (isIPv6(addr) ? 128 : 32)
    ) {
      throw new TypeError(`invalid CIDR prefix in '${spec}'`);
    }
    list.addSubnet(addr, prefix, familyOf(addr, spec));
    return;
  }

  const dash = spec.indexOf("-");
  if (dash >= 0) {
    const first = spec.slice(0, dash).trim();
    const last = spec.slice(dash + 1).trim();
    const fam = familyOf(first, spec);
    if (familyOf(last, spec) !== fam) {
      throw new TypeError(`mixed-family range in '${spec}'`);
    }
    list.addRange(first, last, fam);
    return;
  }

  list.addAddress(spec, familyOf(spec, spec));
}

function familyOf(addr: string, contextForError: string): "ipv4" | "ipv6" {
  if (isIPv4(addr)) return "ipv4";
  if (isIPv6(addr)) return "ipv6";
  throw new TypeError(
    `'${addr}' in '${contextForError}' is not a valid IPv4/IPv6 address`,
  );
}

function stripZone(ip: string): string {
  // Node's socket.remoteAddress occasionally carries a zone id
  // (e.g. "fe80::1%en0") which BlockList.check() rejects. Strip
  // it for matching — the policy is a network-prefix decision,
  // not interface-scoped.
  const i = ip.indexOf("%");
  return i >= 0 ? ip.slice(0, i) : ip;
}

// ---------------------------------------------------------------------------
// Origin allowlist (WebSocket only)
// ---------------------------------------------------------------------------

/**
 * Predicate for the `Origin` request header presented during the
 * WebSocket upgrade.
 */
export interface OriginPolicy {
  /**
   * Is `origin` allowed? An undefined/missing header is allowed
   * only when the policy itself is the always-allow default
   * (because non-browser clients legitimately omit `Origin`); a
   * configured policy that doesn't include `"*"` rejects them.
   */
  allows(origin: string | undefined): boolean;
}

const ORIGIN_ALLOW_ALL: OriginPolicy = { allows: () => true };

/**
 * Compile an Origin allowlist for {@link WsServer}.
 *
 * Each spec is matched **exactly** against the `Origin` header
 * value (e.g. `"https://app.example.com"`); ports must match
 * literally. The literal `"*"` is recognised as "allow any
 * origin **including** non-browser clients with no header at all"
 * — equivalent to the unconfigured default but explicit.
 *
 * Pass `undefined` or `[]` to opt out (allow-all default).
 */
export function buildOriginPolicy(
  specs: readonly string[] | undefined,
): OriginPolicy {
  if (specs === undefined || specs.length === 0) {
    return ORIGIN_ALLOW_ALL;
  }
  const wildcard = specs.includes("*");
  const exact = new Set(specs.filter((s) => s !== "*"));
  return {
    allows(origin: string | undefined): boolean {
      if (wildcard) return true;
      if (origin === undefined) return false;
      return exact.has(origin);
    },
  };
}
