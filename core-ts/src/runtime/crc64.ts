/**
 * CRC-64 ECMA-182 as used by OpenIGTLink.
 *
 * The 256-entry `kTable` is the same one the reference C library
 * ships (`igtl_util.c`) and that the Python codec carries in
 * `corpus-tools/.../codec/crc64.py`. Both round-trip every upstream
 * fixture; if you change anything here, expect oracle parity to
 * fail.
 *
 * Implementation note: JavaScript `bigint` operations are not
 * as cheap as the equivalent `uint64` arithmetic in C++, so the
 * slice-by-8 trick used in `core-cpp` buys less here (and pays
 * more in table allocation, 16 KiB of `bigint`). We use the
 * byte-at-a-time loop instead — simpler, shorter, and still
 * reasonably fast (~400-500 MB/s on modern CPUs for the bigint
 * path). If `parseMessage` latency on multi-MB bodies becomes a
 * bottleneck, revisit with slice-by-8 or a WebAssembly CRC module.
 */

const kMask64 = 0xffffffffffffffffn;

const kTable: readonly bigint[] = [
  0x0000000000000000n, 0x42f0e1eba9ea3693n, 0x85e1c3d753d46d26n, 0xc711223cfa3e5bb5n,
  0x493366450e42ecdfn, 0x0bc387aea7a8da4cn, 0xccd2a5925d9681f9n, 0x8e224479f47cb76an,
  0x9266cc8a1c85d9ben, 0xd0962d61b56fef2dn, 0x17870f5d4f51b498n, 0x5577eeb6e6bb820bn,
  0xdb55aacf12c73561n, 0x99a54b24bb2d03f2n, 0x5eb4691841135847n, 0x1c4488f3e8f96ed4n,
  0x663d78ff90e185efn, 0x24cd9914390bb37cn, 0xe3dcbb28c335e8c9n, 0xa12c5ac36adfde5an,
  0x2f0e1eba9ea36930n, 0x6dfeff5137495fa3n, 0xaaefdd6dcd770416n, 0xe81f3c86649d3285n,
  0xf45bb4758c645c51n, 0xb6ab559e258e6ac2n, 0x71ba77a2dfb03177n, 0x334a9649765a07e4n,
  0xbd68d2308226b08en, 0xff9833db2bcc861dn, 0x388911e7d1f2dda8n, 0x7a79f00c7818eb3bn,
  0xcc7af1ff21c30bden, 0x8e8a101488293d4dn, 0x499b3228721766f8n, 0x0b6bd3c3dbfd506bn,
  0x854997ba2f81e701n, 0xc7b97651866bd192n, 0x00a8546d7c558a27n, 0x4258b586d5bfbcb4n,
  0x5e1c3d753d46d260n, 0x1cecdc9e94ace4f3n, 0xdbfdfea26e92bf46n, 0x990d1f49c77889d5n,
  0x172f5b3033043ebfn, 0x55dfbadb9aee082cn, 0x92ce98e760d05399n, 0xd03e790cc93a650an,
  0xaa478900b1228e31n, 0xe8b768eb18c8b8a2n, 0x2fa64ad7e2f6e317n, 0x6d56ab3c4b1cd584n,
  0xe374ef45bf6062een, 0xa1840eae168a547dn, 0x66952c92ecb40fc8n, 0x2465cd79455e395bn,
  0x3821458aada7578fn, 0x7ad1a461044d611cn, 0xbdc0865dfe733aa9n, 0xff3067b657990c3an,
  0x711223cfa3e5bb50n, 0x33e2c2240a0f8dc3n, 0xf4f3e018f031d676n, 0xb60301f359dbe0e5n,
  0xda050215ea6c212fn, 0x98f5e3fe438617bcn, 0x5fe4c1c2b9b84c09n, 0x1d14202910527a9an,
  0x93366450e42ecdf0n, 0xd1c685bb4dc4fb63n, 0x16d7a787b7faa0d6n, 0x5427466c1e109645n,
  0x4863ce9ff6e9f891n, 0x0a932f745f03ce02n, 0xcd820d48a53d95b7n, 0x8f72eca30cd7a324n,
  0x0150a8daf8ab144en, 0x43a04931514122ddn, 0x84b16b0dab7f7968n, 0xc6418ae602954ffbn,
  0xbc387aea7a8da4c0n, 0xfec89b01d3679253n, 0x39d9b93d2959c9e6n, 0x7b2958d680b3ff75n,
  0xf50b1caf74cf481fn, 0xb7fbfd44dd257e8cn, 0x70eadf78271b2539n, 0x321a3e938ef113aan,
  0x2e5eb66066087d7en, 0x6cae578bcfe24bedn, 0xabbf75b735dc1058n, 0xe94f945c9c3626cbn,
  0x676dd025684a91a1n, 0x259d31cec1a0a732n, 0xe28c13f23b9efc87n, 0xa07cf2199274ca14n,
  0x167ff3eacbaf2af1n, 0x548f120162451c62n, 0x939e303d987b47d7n, 0xd16ed1d631917144n,
  0x5f4c95afc5edc62en, 0x1dbc74446c07f0bdn, 0xdaad56789639ab08n, 0x985db7933fd39d9bn,
  0x84193f60d72af34fn, 0xc6e9de8b7ec0c5dcn, 0x01f8fcb784fe9e69n, 0x43081d5c2d14a8fan,
  0xcd2a5925d9681f90n, 0x8fdab8ce70822903n, 0x48cb9af28abc72b6n, 0x0a3b7b1923564425n,
  0x70428b155b4eaf1en, 0x32b26afef2a4998dn, 0xf5a348c2089ac238n, 0xb753a929a170f4abn,
  0x3971ed50550c43c1n, 0x7b810cbbfce67552n, 0xbc902e8706d82ee7n, 0xfe60cf6caf321874n,
  0xe224479f47cb76a0n, 0xa0d4a674ee214033n, 0x67c58448141f1b86n, 0x253565a3bdf52d15n,
  0xab1721da49899a7fn, 0xe9e7c031e063acecn, 0x2ef6e20d1a5df759n, 0x6c0603e6b3b7c1can,
  0xf6fae5c07d3274cdn, 0xb40a042bd4d8425en, 0x731b26172ee619ebn, 0x31ebc7fc870c2f78n,
  0xbfc9838573709812n, 0xfd39626eda9aae81n, 0x3a28405220a4f534n, 0x78d8a1b9894ec3a7n,
  0x649c294a61b7ad73n, 0x266cc8a1c85d9be0n, 0xe17dea9d3263c055n, 0xa38d0b769b89f6c6n,
  0x2daf4f0f6ff541acn, 0x6f5faee4c61f773fn, 0xa84e8cd83c212c8an, 0xeabe6d3395cb1a19n,
  0x90c79d3fedd3f122n, 0xd2377cd44439c7b1n, 0x15265ee8be079c04n, 0x57d6bf0317edaa97n,
  0xd9f4fb7ae3911dfdn, 0x9b041a914a7b2b6en, 0x5c1538adb04570dbn, 0x1ee5d94619af4648n,
  0x02a151b5f156289cn, 0x4051b05e58bc1e0fn, 0x87409262a28245ban, 0xc5b073890b687329n,
  0x4b9237f0ff14c443n, 0x0962d61b56fef2d0n, 0xce73f427acc0a965n, 0x8c8315cc052a9ff6n,
  0x3a80143f5cf17f13n, 0x7870f5d4f51b4980n, 0xbf61d7e80f251235n, 0xfd913603a6cf24a6n,
  0x73b3727a52b393ccn, 0x31439391fb59a55fn, 0xf652b1ad0167feean, 0xb4a25046a88dc879n,
  0xa8e6d8b54074a6adn, 0xea16395ee99e903en, 0x2d071b6213a0cb8bn, 0x6ff7fa89ba4afd18n,
  0xe1d5bef04e364a72n, 0xa3255f1be7dc7ce1n, 0x64347d271de22754n, 0x26c49cccb40811c7n,
  0x5cbd6cc0cc10fafcn, 0x1e4d8d2b65facc6fn, 0xd95caf179fc497dan, 0x9bac4efc362ea149n,
  0x158e0a85c2521623n, 0x577eeb6e6bb820b0n, 0x906fc95291867b05n, 0xd29f28b9386c4d96n,
  0xcedba04ad0952342n, 0x8c2b41a1797f15d1n, 0x4b3a639d83414e64n, 0x09ca82762aab78f7n,
  0x87e8c60fded7cf9dn, 0xc51827e4773df90en, 0x020905d88d03a2bbn, 0x40f9e43324e99428n,
  0x2cffe7d5975e55e2n, 0x6e0f063e3eb46371n, 0xa91e2402c48a38c4n, 0xebeec5e96d600e57n,
  0x65cc8190991cb93dn, 0x273c607b30f68faen, 0xe02d4247cac8d41bn, 0xa2dda3ac6322e288n,
  0xbe992b5f8bdb8c5cn, 0xfc69cab42231bacfn, 0x3b78e888d80fe17an, 0x7988096371e5d7e9n,
  0xf7aa4d1a85996083n, 0xb55aacf12c735610n, 0x724b8ecdd64d0da5n, 0x30bb6f267fa73b36n,
  0x4ac29f2a07bfd00dn, 0x08327ec1ae55e69en, 0xcf235cfd546bbd2bn, 0x8dd3bd16fd818bb8n,
  0x03f1f96f09fd3cd2n, 0x41011884a0170a41n, 0x86103ab85a2951f4n, 0xc4e0db53f3c36767n,
  0xd8a453a01b3a09b3n, 0x9a54b24bb2d03f20n, 0x5d45907748ee6495n, 0x1fb5719ce1045206n,
  0x919735e51578e56cn, 0xd367d40ebc92d3ffn, 0x1476f63246ac884an, 0x568617d9ef46bed9n,
  0xe085162ab69d5e3cn, 0xa275f7c11f7768afn, 0x6564d5fde549331an, 0x279434164ca30589n,
  0xa9b6706fb8dfb2e3n, 0xeb46918411358470n, 0x2c57b3b8eb0bdfc5n, 0x6ea7525342e1e956n,
  0x72e3daa0aa188782n, 0x30133b4b03f2b111n, 0xf7021977f9cceaa4n, 0xb5f2f89c5026dc37n,
  0x3bd0bce5a45a6b5dn, 0x79205d0e0db05dcen, 0xbe317f32f78e067bn, 0xfcc19ed95e6430e8n,
  0x86b86ed5267cdbd3n, 0xc4488f3e8f96ed40n, 0x0359ad0275a8b6f5n, 0x41a94ce9dc428066n,
  0xcf8b0890283e370cn, 0x8d7be97b81d4019fn, 0x4a6acb477bea5a2an, 0x089a2aacd2006cb9n,
  0x14dea25f3af9026dn, 0x562e43b4931334fen, 0x913f6188692d6f4bn, 0xd3cf8063c0c759d8n,
  0x5dedc41a34bbeeb2n, 0x1f1d25f19d51d821n, 0xd80c07cd676f8394n, 0x9afce626ce85b507n,
];

// ---------------------------------------------------------------------------
// Split hi/lo uint32 tables — the fast path
// ---------------------------------------------------------------------------
//
// `bigint` arithmetic in V8 allocates on every op. The inner CRC
// loop has 3 bigint ops per byte, so byte-at-a-time bigint CRC tops
// out around 20-30 MB/s. A JIT-friendly implementation keeps the
// running CRC in two `number` locals (uint32 hi / lo), with the
// 256-entry table unpacked into two `Uint32Array`s so lookups are
// bounded-size typed indexes rather than `bigint` reads.
//
// The ECMA-182 polynomial is non-reflected (MSB-first). Per-byte
// update in 64-bit land:
//   idx = byte ^ (crc >>> 56)       // top byte XOR input byte
//   crc = (crc << 8) ^ TABLE[idx]
//
// Split crc = hi|lo (each uint32). The same step becomes:
//   idx = byte ^ (hi >>> 24)
//   newHi = ((hi << 8) | (lo >>> 24)) ^ TABLE_HI[idx]
//   newLo = (lo << 8)                ^ TABLE_LO[idx]
// Everything fits in int32 arithmetic. `>>> 0` at the end of each
// assignment keeps the state as unsigned uint32 (so `>>> 24` on hi
// behaves correctly next iteration).
//
// The TABLE_HI/TABLE_LO arrays are derived from kTable at module
// load — no duplicated constants to maintain. One-time cost, runs
// once per process.

const kTableHi = new Uint32Array(256);
const kTableLo = new Uint32Array(256);
for (let i = 0; i < 256; i++) {
  const v = kTable[i] as bigint;
  kTableHi[i] = Number((v >> 32n) & 0xffffffffn);
  kTableLo[i] = Number(v & 0xffffffffn);
}

// ---------------------------------------------------------------------------
// Slice-by-8 tables (T0..T7 paired into HI/LO Uint32Arrays)
// ---------------------------------------------------------------------------
//
// With T0 = kTable (the byte-at-a-time table), the slice-by-8
// tables satisfy:
//
//   T_{k+1}[i] = T_0[(T_k[i] >> 56) & 0xff] ^ (T_k[i] << 8)
//
// At lookup time, each of the 8 bytes in an 8-byte input chunk
// (XORed into the running CRC at the top) is reduced through its
// corresponding table; the eight 64-bit results are XORed
// together — algebraically identical to running the byte loop
// 8 times in a row.
//
// We store the 8 × 256 entries as two flat Uint32Arrays indexed
// `k * 256 + i`. Flat layout (vs. array-of-arrays) keeps the hot
// loop's bracket accesses simple, and the total 16 KiB per array
// comfortably fits in L1 on any machine we care about.

const kSlicesHi = new Uint32Array(8 * 256);
const kSlicesLo = new Uint32Array(8 * 256);
// T0 is just kTable.
kSlicesHi.set(kTableHi, 0);
kSlicesLo.set(kTableLo, 0);
for (let k = 1; k < 8; k++) {
  const prevOff = (k - 1) * 256;
  const curOff = k * 256;
  for (let i = 0; i < 256; i++) {
    const prevHi = kSlicesHi[prevOff + i] as number;
    const prevLo = kSlicesLo[prevOff + i] as number;
    // Next table entry: T_{k+1}[i] = T_0[top_byte(T_k[i])] ^ (T_k[i] << 8)
    const topByte = (prevHi >>> 24) & 0xff;
    const shiftedHi = ((prevHi << 8) | (prevLo >>> 24)) >>> 0;
    const shiftedLo = (prevLo << 8) >>> 0;
    kSlicesHi[curOff + i] = (shiftedHi ^ (kTableHi[topByte] as number)) >>> 0;
    kSlicesLo[curOff + i] = (shiftedLo ^ (kTableLo[topByte] as number)) >>> 0;
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * Compute CRC-64 ECMA-182 over `bytes`, optionally continuing from
 * a previous seed (default 0n). Returns a non-negative `bigint`.
 *
 * Chaining: `crc64(b, crc64(a))` equals `crc64(a + b)`.
 *
 * Implementation: split-uint32 + slice-by-8 for chunks of ≥8 bytes,
 * byte-at-a-time tail. Keeps the running CRC in two `number`
 * locals to avoid bigint allocation in the hot loop.
 */
export function crc64(bytes: Uint8Array, seed: bigint = 0n): bigint {
  let hi = Number((seed >> 32n) & 0xffffffffn) | 0;
  let lo = Number(seed & 0xffffffffn) | 0;

  // --- Slice-by-8 hot loop ---------------------------------------------
  // Read 8 bytes at a time, XOR into the running CRC, then reduce the
  // result through the eight paired HI/LO slice tables. Each 8-byte
  // block is algebraically equivalent to 8 byte-at-a-time iterations,
  // but issues 8 table lookups in parallel XOR instead of sequentially.
  //
  // Direct DataView reads (big-endian) avoid byte-by-byte reassembly;
  // the two uint32 halves of each 8-byte chunk are `chunkHi`/`chunkLo`.
  const len = bytes.length;
  let i = 0;
  if (len >= 8) {
    const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    const end8 = len - (len & 7);
    while (i < end8) {
      const chunkHi = view.getUint32(i, false);
      const chunkLo = view.getUint32(i + 4, false);
      const xHi = (hi ^ chunkHi) >>> 0;
      const xLo = (lo ^ chunkLo) >>> 0;
      // Slice indices 7..0 correspond to bytes byte0..byte7 of the
      // XORed chunk, read MSB first. byte0 is the top byte of xHi,
      // byte7 is the bottom byte of xLo.
      const b0 = (xHi >>> 24) & 0xff;
      const b1 = (xHi >>> 16) & 0xff;
      const b2 = (xHi >>> 8) & 0xff;
      const b3 = xHi & 0xff;
      const b4 = (xLo >>> 24) & 0xff;
      const b5 = (xLo >>> 16) & 0xff;
      const b6 = (xLo >>> 8) & 0xff;
      const b7 = xLo & 0xff;
      const newHi =
        (kSlicesHi[7 * 256 + b0] as number) ^
        (kSlicesHi[6 * 256 + b1] as number) ^
        (kSlicesHi[5 * 256 + b2] as number) ^
        (kSlicesHi[4 * 256 + b3] as number) ^
        (kSlicesHi[3 * 256 + b4] as number) ^
        (kSlicesHi[2 * 256 + b5] as number) ^
        (kSlicesHi[1 * 256 + b6] as number) ^
        (kSlicesHi[0 * 256 + b7] as number);
      const newLo =
        (kSlicesLo[7 * 256 + b0] as number) ^
        (kSlicesLo[6 * 256 + b1] as number) ^
        (kSlicesLo[5 * 256 + b2] as number) ^
        (kSlicesLo[4 * 256 + b3] as number) ^
        (kSlicesLo[3 * 256 + b4] as number) ^
        (kSlicesLo[2 * 256 + b5] as number) ^
        (kSlicesLo[1 * 256 + b6] as number) ^
        (kSlicesLo[0 * 256 + b7] as number);
      hi = newHi | 0;
      lo = newLo | 0;
      i += 8;
    }
  }

  // --- Byte-at-a-time tail ---------------------------------------------
  for (; i < len; i++) {
    const idx = ((hi >>> 24) ^ (bytes[i] as number)) & 0xff;
    const newHi = ((hi << 8) | (lo >>> 24)) ^ (kTableHi[idx] as number);
    const newLo = (lo << 8) ^ (kTableLo[idx] as number);
    hi = newHi | 0;
    lo = newLo | 0;
  }

  // Recombine into bigint. `>>> 0` converts int32 → uint32 before
  // BigInt sees the sign bit.
  return (BigInt(hi >>> 0) << 32n) | BigInt(lo >>> 0);
}
