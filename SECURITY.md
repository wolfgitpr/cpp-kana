# Security policy

## Supported versions

The supported line is `cpp-kana2` 2.x, the package this tree builds. Version 1 of the library is a separate
package (`cpp-kana`, with its own headers under `<cpp-kana/...>`) and is not part of this branch.

Within 2.x, the package config advertises `SameMajorVersion` compatibility: the configure step accepts a
request for 2.x only, and refuses a cross-major request rather than letting an incompatible install answer it.
That is an API compatibility statement, not a binary one; anything outside the same major version needs a
rebuild.

## Reporting a vulnerability

Please do not open a public issue for a security problem. Instead:

- open a private security advisory on the repository, or
- contact the maintainers through the repository's issue tracker, asking for a private channel before
  describing the problem.

A useful report includes the input that triggers the problem, the build configuration (compiler, platform,
shared or static library), and which surface you reached the library through (C++ API, C ABI, Python or C#).
Please avoid publishing details until a fix or a decision about one is available.

## The parsing surface

The library's whole input surface is text parsing: UTF-8 in, UTF-8 out.

- **UTF-8 is validated strictly, not repaired.** The decoder rejects overlong forms, surrogates, code points
  above U+10FFFF and truncated sequences, and reports the failure as a status value. The library does not
  substitute U+FFFD in its own output, and the encoder refuses to encode a surrogate or an out-of-range code
  point. Malformed bytes are therefore an input error, not something that reaches the table as a
  half-decoded unit.
- **No allocation is driven by a size the caller states.** Every string-taking entry point takes
  NUL-terminated UTF-8 with no length parameter, so the library cannot be told how much to allocate: buffers
  are sized from the bytes it actually has, not from a size the caller asserts. The one size a caller does
  supply is `struct_size` on the option structures, and it is refused when it is smaller than the header's
  `sizeof` rather than being trusted.
- **Allocation failure is reported, not undefined.** Out-of-memory at the C ABI is caught and returned as a
  status code, alongside a `catch (...)` for anything else, so no C++ exception unwinds across the boundary.
- **The C ABI's freeing rule is part of the contract.** Every allocation the library makes must be freed by
  the library — `cppk2_conversion_free` for a conversion (including every string inside it),
  `cppk2_string_free` for a detached string such as `cppk2_error.detail`. Calling the C runtime's `free()` on
  those pointers is heap corruption on Windows. Pointers inside a conversion stay valid only until the
  conversion is freed.
- **No mutable state, no locking.** The library holds no mutable global state, so concurrent calls are safe;
  what is not safe is sharing one conversion between threads while one of them frees it.

The rules above are stated in the headers themselves — `include/cpp-kana2/ext/c_api.h` for the C ABI and
`include/cpp-kana2/core/Utf8.h` for the UTF-8 boundary — and the test suite exercises them, including the C
smoke test that is compiled as C; CI runs that suite under AddressSanitizer and UndefinedBehaviorSanitizer
through the `sanitize` preset.
