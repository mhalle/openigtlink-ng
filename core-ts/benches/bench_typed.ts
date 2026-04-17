/**
 * Microbenchmark for the typed TS codec.
 *
 * Mirrors `core-py/benches/bench_typed.py`. Reports µs/op and MB/s
 * for pack / unpack / full oracle across TRANSFORM, IMAGE, and
 * VIDEOMETA fixtures, plus synthesized VGA / XGA / FHD images.
 *
 *     node --loader ts-node/esm benches/bench_typed.ts
 *     # or after `npm run build`:
 *     node dist/benches/bench_typed.js
 */

import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";

import { fromHex } from "../src/runtime/byte_order.js";
import { parseWire, verifyWireBytes } from "../src/runtime/oracle.js";
import { packHeader } from "../src/runtime/header.js";
import "../src/messages/index.js";
import { ImageMessage, Transform, Videometa } from "../src/messages/index.js";

function loadFixtures() {
  const here = dirname(fileURLToPath(import.meta.url));
  const candidates = [
    resolve(here, "../../spec/corpus/upstream-fixtures.json"),
    resolve(here, "../../../spec/corpus/upstream-fixtures.json"),
  ];
  for (const c of candidates) {
    try {
      const parsed = JSON.parse(readFileSync(c, "utf-8"));
      return parsed.fixtures;
    } catch {}
  }
  throw new Error("upstream-fixtures.json not found");
}

function bench(fn: () => unknown, seconds = 0.5, minIters = 200): number {
  for (let i = 0; i < 10; i++) fn();
  const t0 = performance.now();
  let iters = 0;
  const deadline = t0 + seconds * 1000;
  while (performance.now() < deadline || iters < minIters) {
    fn();
    iters++;
  }
  return ((performance.now() - t0) / iters) * 1000;
}

function row(label: string, us: number, bytesSize: number): void {
  const mb = bytesSize / (us * 1e-6) / (1024 * 1024);
  console.log(
    `  ${label.padEnd(36)} ${us.toFixed(3).padStart(10)} µs/op   ${mb.toFixed(1).padStart(10)} MB/s`,
  );
}

function benchFixture<T extends { pack(): Uint8Array }>(
  label: string,
  wireHex: string,
  ctor: { unpack(b: Uint8Array): T },
): void {
  const wire = fromHex(wireHex);
  console.log(`\n${label}  (${wire.length} wire bytes)`);
  console.log(`  ${"operation".padEnd(36)} ${"time".padStart(13)}   ${"throughput".padStart(14)}`);
  console.log(`  ${"-".repeat(70)}`);

  const framing = parseWire(wire);
  const content = framing.contentBytes;

  const msg = ctor.unpack(content);
  row("typed unpack", bench(() => ctor.unpack(content)), content.length);
  row("typed pack", bench(() => msg.pack()), content.length);
  row("verifyWireBytes (full pipeline)", bench(() => verifyWireBytes(wire)), wire.length);
}

function synthImage(w: number, h: number): Uint8Array {
  const msg = new ImageMessage({
    header_version: 2,
    num_components: 1,
    scalar_type: 3,
    endian: 1,
    coord: 1,
    size: [w, h, 1],
    matrix: [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0],
    subvol_offset: [0, 0, 0],
    subvol_size: [w, h, 1],
    pixels: new Uint8Array(w * h).map((_, i) => i & 0xff),
  });
  const body = msg.pack();
  const header = packHeader({
    version: 1,
    typeId: "IMAGE",
    deviceName: "BenchCam",
    timestamp: 0n,
    body,
  });
  const wire = new Uint8Array(header.length + body.length);
  wire.set(header);
  wire.set(body, header.length);
  return wire;
}

async function main() {
  const fixtures = loadFixtures();
  console.log("oigtl TypeScript typed codec — microbenchmark");
  console.log("==============================================");
  console.log(`Node ${process.version}`);

  benchFixture("TRANSFORM", fixtures.transform.wire_hex, Transform);
  benchFixture("IMAGE (50×50 fixture)", fixtures.image.wire_hex, ImageMessage);
  benchFixture("VIDEOMETA (v3, struct array)", fixtures.videometa.wire_hex, Videometa);

  console.log("\n" + "=".repeat(70));
  console.log("Realistic image sizes");
  console.log("=".repeat(70));
  for (const [w, h, label] of [
    [640, 480, "VGA   (640×480 grayscale)"],
    [1024, 768, "XGA   (1024×768 grayscale)"],
    [1920, 1080, "FHD   (1920×1080 grayscale)"],
  ] as const) {
    const wire = synthImage(w, h);
    const bodyLen = wire.length - 58;
    const body = wire.subarray(58);
    const msg = ImageMessage.unpack(body);
    console.log(`\n${label}  (${bodyLen.toLocaleString()} body bytes)`);
    console.log(`  ${"-".repeat(70)}`);
    row("typed unpack", bench(() => ImageMessage.unpack(body), 0.3), bodyLen);
    row("typed pack", bench(() => msg.pack(), 0.3), bodyLen);
    row(
      "verifyWireBytes (full pipeline)",
      bench(() => verifyWireBytes(wire), 0.3),
      wire.length,
    );
  }
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
