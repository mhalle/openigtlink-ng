/**
 * Protocol error hierarchy.
 *
 * Every codec failure surfaces as a subclass of {@link ProtocolError}
 * so callers can either `catch (e: ProtocolError)` broadly or
 * discriminate on the specific subclass. Matches the C++
 * `ProtocolError` hierarchy in `core-cpp/include/oigtl/runtime/error.hpp`
 * and the Python `oigtl.runtime.exceptions` module.
 */

export class ProtocolError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "ProtocolError";
    // Preserve prototype chain across transpile targets.
    Object.setPrototypeOf(this, new.target.prototype);
  }
}

/** Raised when the 58-byte header cannot be parsed. */
export class HeaderParseError extends ProtocolError {
  constructor(message: string) {
    super(message);
    this.name = "HeaderParseError";
    Object.setPrototypeOf(this, HeaderParseError.prototype);
  }
}

/** Raised when the header CRC does not match the body. */
export class CrcMismatchError extends ProtocolError {
  readonly expected: bigint;
  readonly actual: bigint;
  constructor(expected: bigint, actual: bigint) {
    super(
      `CRC mismatch: header declared 0x${expected.toString(16)}, ` +
        `body computes 0x${actual.toString(16)}`,
    );
    this.name = "CrcMismatchError";
    this.expected = expected;
    this.actual = actual;
    Object.setPrototypeOf(this, CrcMismatchError.prototype);
  }
}

/** Raised when a typed message body cannot be decoded per its schema. */
export class BodyDecodeError extends ProtocolError {
  constructor(message: string) {
    super(message);
    this.name = "BodyDecodeError";
    Object.setPrototypeOf(this, BodyDecodeError.prototype);
  }
}

/** Raised when a message encodes to more bytes than the schema allows. */
export class BodyEncodeError extends ProtocolError {
  constructor(message: string) {
    super(message);
    this.name = "BodyEncodeError";
    Object.setPrototypeOf(this, BodyEncodeError.prototype);
  }
}

/** Raised when the wire type_id has no registered typed class. */
export class UnknownTypeError extends ProtocolError {
  readonly typeId: string;
  constructor(typeId: string) {
    super(`no typed class registered for type_id ${JSON.stringify(typeId)}`);
    this.name = "UnknownTypeError";
    this.typeId = typeId;
    Object.setPrototypeOf(this, UnknownTypeError.prototype);
  }
}
