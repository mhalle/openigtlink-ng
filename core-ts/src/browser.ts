/**
 * Pre-bundled browser entry point for ``@openigtlink/core``.
 *
 * Imports this file (or the generated ``dist/browser.mjs``) to
 * get the WebSocket transport plus every generated message class
 * in a single ESM module, suitable for ``<script type="module">``
 * tag use without a bundler.
 *
 * Researchers using a bundler (Vite, Webpack, esbuild, Rollup)
 * should import directly from subpaths instead — it tree-shakes
 * better:
 *
 *     import { WsClient } from "@openigtlink/core/net/ws";
 *     import { Transform, Status } from "@openigtlink/core/messages";
 *
 * This aggregate file exists specifically for the no-bundler
 * case (drop a .mjs next to an HTML file, serve from a static
 * host, done).
 */

// Side-effect import: populates the dispatch registry with every
// generated message class so `WsClient.receive(Transform)` etc.
// can decode bodies automatically.
import "./messages/index.js";

export * from "./net/ws/index.js";
export * from "./messages/index.js";
