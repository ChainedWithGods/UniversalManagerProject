# CryptoAPI: AES-256 Key Discovery & Container Verification

The CryptoAPI subsystem discovers the **AES-256 encryption key** of an Unreal Engine game at
runtime and verifies it against the game's own encrypted archives. It then writes a
JSON **container report** that covers every pak, IoStore TOC and IoStore payload
container found next to the executable.

| Component | Source | Role |
| --- | --- | --- |
| `AESKeyFinder` | [`Runtime/crypto/AESKeyFinder.{h,cpp}`](../../Runtime/crypto/AESKeyFinder.h) | Candidate scan, orchestration, executable metadata, JSON writer |
| `PakVerifier` | [`Runtime/crypto/PakVerifier.{h,cpp}`](../../Runtime/crypto/PakVerifier.h) | Pak/IoStore parsing, AES-256-ECB verification, per-container report builder |

Both live in namespace `Crypto`.

> **Why verification matters.** An AES-256 key is 32 bytes of unstructured data, the same
> shape as countless buffers in a process. I didn't trust the 32 random bytes just because
> they looked random. That's not verification; that's how idiots ship false keys. We report a
> key only when decrypting a real encrypted index with it either reproduces the stored SHA-1
> hash (pak) or produces a structurally valid IoStore directory index (`.utoc`).

---

## 1. When it runs

`AESKeyFinder::Scan()` is called once during initialization by `dllmain.cpp`:

```cpp
// Runtime/core/dllmain.cpp, InitThread
if (runtimeConfig.AutoDiscoverAESKey) {
    Crypto::AESKeyFinder::Scan("");
}
```

* Config: `[Features] AutoDiscoverAESKey` (aliases `FindAESKey`, `AutoAESKey`,
  `DiscoverAESKey`). See [RuntimeAPI/CONFIG.md](../RuntimeAPI/CONFIG.md). We turn it off after
  the first successful scan; leaving it on for every normal launch is idiotic, because the key
  doesn't change until the game patches.
* The scan is synchronous on the init thread; `ScanAsync()` exists for tooling and uses the
  same path.
* If no archive is encrypted, the key scan is skipped and the **container report is still
  written**. It records the unencrypted inventory and states that no key is required.
* If encrypted archives exist, the report is written on success and on failure. A failed
  key search still produces the archive census, verification failures and per-container
  structure.
* Results are available in-process through `AESKeyFinder::IsScanFinished()` /
  `AESKeyFinder::GetResult()`; there is no IPC command that triggers the scan.

---

## 2. Pipeline

```
Scan(exe_path)
  │
  ├─ 1. QueryVersionInfo + CheckEncryption
  │      parse every .pak/.utoc/.ucas, pair IoStore containers, build the census,
  │      choose the primary encrypted archive for the log
  │
  ├─ 2. BuildVerifyContext
  │      if encrypted paks with stored hashes exist: load their index blocks
  │      otherwise: load encrypted IoStore directory-index blocks
  │
  ├─ 3. Memory scan (only when encryption is present)
  │      main executable regions, then game modules under the game root
  │      data sections  → ScanDataSections
  │      code sections  → ScanCodeImmediates
  │
  ├─ 4. TryCandidate → VerifyKey
  │      cheap 64-byte preview gate on the first archive test, then full
  │      SHA-1 / structural verification across the loaded index tests
  │
  ├─ 5. BuildArchiveReports
  │      verify and decode every container independently:
  │        pak  → primary index, path-hash index, full-directory index,
  │               sample FPakEntry records
  │        utoc → TOC header, chunk-type census, directory-index tree
  │        ucas → size, paired TOC, chunk-encryption state
  │
  └─ 6. SaveJson
         <exe-stem>_AESKey.json next to the executable, plus AESKey.json in the CWD
```

The key search stops at the first verified candidate. The report pass runs afterwards and
does not depend on which archive supplied the key proof. I don't stare at `target_archive`
like it's the only archive that matters, it's just the first place the key checked out;
every container gets its own record either way.

---

## 3. Candidate sources

| Source tag | Found in | Pattern |
| --- | --- | --- |
| `data_raw_buffer` | data sections | raw 32-byte window at 16-byte stride |
| `hex_literal_0x` | data sections | ASCII `0x` + 64 hex chars |
| `hex_literal_64char` | data sections | ASCII 64 hex chars |
| `utf16_hex_literal` | data sections | UTF-16 `0x` + 64 wide hex chars |
| `base64_string_data` | data sections | 44-char base64 string decoding to 32 bytes |
| `mov_imm64_code` | code sections | 4 × `48 B8+r imm64` back-to-back |
| `mov_imm32_code` | code sections | 8 × `C7 … imm32` stores of 4 bytes |

Every candidate passes `AESKeyFinder::LooksLikeKey()` before BCrypt verification. See
[REFERENCE.md](REFERENCE.md#lookslikekey) for the filter rules. We run the cheap filter first;
skipping it because "BCrypt will sort it out" means Microsoft's BCrypt gets a call on every
random 32-byte window in the process, stupid trade.

---

## 4. Outputs

| File | Location |
| --- | --- |
| `<exe-stem>_AESKey.json` | next to the game executable |
| `AESKey.json` | current working directory (convenience copy) |

The report is written after verification is attempted, including when the key is not found
or not required. Log lines are tagged `[AESKeyFinder]`.

Example (successful, abridged):

```json
{
  "generated_by": { "tool": "Avalonium AESKeyFinder", "source_truth": "Vendored Unreal Engine sources (UE4.23-UE5.8)." },
  "scan": { "mode": "in-process scan", "duration_ms": 812, "candidates_tested": 131831, "report_build_ms": 64 },
  "game": {
    "name": "MyGame",
    "executable": "MyGame-Win64-Shipping.exe",
    "executable_path": "C:\\Game\\Binaries\\Win64\\MyGame-Win64-Shipping.exe",
    "executable_size": 123456789,
    "version_info": { "ProductName": "MyGame", "CompanyName": "Studio", "fixed_product_version": "5.4.4.0" }
  },
  "engine": { "family": "Unreal Engine", "version": "5.4", "evidence": "VS_FIXEDFILEINFO product version" },
  "encryption": {
    "algorithm": "AES-256", "mode": "ECB", "key_size_bits": 256, "block_size_bytes": 16,
    "key_guid": "00000000-0000-0000-0000-000000000000",
    "key_guid_meaning": "All-zero GUID with encrypted containers means the default/embedded project key.",
    "pak_index_encrypted": true,
    "iostore_directory_index_encrypted": true
  },
  "key": {
    "found": true,
    "verified": true,
    "hex": "0xAABB…",
    "base64": "qru8…",
    "unrealpak_arg": "-aes=0xAABB…",
    "detection": {
      "method": "data_raw_buffer",
      "runtime_address": "0x7FF6…",
      "module": "MyGame-Win64-Shipping.exe",
      "module_offset": "0xA3594A0",
      "section": ".rdata",
      "on_disk_search": "not performed"
    },
    "byte_statistics": {
      "sample_bytes": 32,
      "unique_byte_values": 32,
      "shannon_entropy_bits_per_byte": 5.0,
      "sample_entropy_ceiling_bits_per_byte": 5.0
    }
  },
  "verification": {
    "method": "AES-256-ECB decryption followed by SHA-1 comparison against FPakInfo::IndexHash (pak) and structural parsing of FIoDirectoryIndex (IoStore).",
    "encrypted_containers": true,
    "pak_primary_indexes": { "total": 9, "verified": 9 },
    "pak_secondary_indexes": { "total": 18, "verified": 18 },
    "iostore_directory_indexes": { "total": 9, "parsed": 9 },
    "sample_entries_decoded": 40,
    "all_checks_passed": true
  },
  "census": {
    "containers": { "total": 29, "pak": 9, "utoc": 10, "ucas": 10 },
    "encrypted": { "paks": 9, "utocs": 8, "ucas_chunk_data": 8 },
    "pak_entries": 203981,
    "pak_indexed_files": 203981,
    "iostore_chunks": 175708,
    "iostore_indexed_files": 168561,
    "iostore_non_file_chunks": 7147,
    "bytes": { "pak": 6067591552, "utoc": 34202170, "ucas": 41203347200, "total": 47305140922 }
  },
  "target_archive": {
    "file_name": "pakchunk0-Windows.pak",
    "role": "first key-verified pak index",
    "mount_point": "../../../",
    "entry_count": 36783
  },
  "archives": [
    {
      "file_name": "pakchunk0-Windows.pak",
      "kind": "pak",
      "file_size": 1658276433,
      "encrypted": true,
      "footer": { "version": 11, "version_name": "PakFile_Version_Fnv64BugFix", "encrypted_index": true, "compression_methods": ["Oodle"] },
      "primary_index": {
        "mount_point": "../../../",
        "entry_count": 36783,
        "hash_verified": true,
        "path_hash_index": { "hash_verified": true },
        "full_directory_index": { "hash_verified": true }
      },
      "directory": { "file_count": 36783, "extension_counts": { ".wem": 17593, ".bnk": 14485 } }
    }
  ],
  "format_reference": { "pak": { "footer_magic": "0x5A6F12E1" }, "iostore": { "toc_magic": "-==--==--==--==-" } },
  "notes": ["All 9 pak primary indexes, 18 pak secondary indexes, and 9 IoStore directory indexes were verified."]
}
```

The full report format, every field and the decoders are documented in
[REFERENCE.md](REFERENCE.md#part-2--json-report).

`unrealpak_arg` is ready for UnrealPak-style tooling; `c_array` drops into C/C++
sources. The report also contains the key in spaced-hex and base64 forms.

---

## 5. Report semantics

* **Pak.** Each `.pak` gets its own footer, primary index, path-hash index, full-directory
  index and directory summary. Stored SHA-1 hashes are recomputed from the decrypted bytes
  and compared; the primary index is not parsed when its hash does not match.
* **IoStore.** Each `.utoc` gets its TOC header, chunk-type census, directory-index mount
  point and file tree. TOC entries are decoded with the source-true
  `FIoChunkId (12 bytes) + FIoOffsetAndLength (10 bytes)` layout; the directory index is
  located after the perfect-hash tables, compression blocks, method names and optional
  signatures.
* **Ucas.** Each `.ucas` is an inventory entry: size, paired `.utoc`, and whether the paired
  TOC marks its chunks encrypted.
* **Samples.** Pak samples include decoded `FPakEntry` fields (offset, sizes, method,
  encrypted flag, block count) proving that the directory locations and encoded-entry table
  are understood. Sample assets for IoStore include the chunk index. The report does not
  decompress payload data.
* **Empty containers.** Empty patch paks are retained; their zero-entry primary index,
  16-byte secondary maps and valid hashes are reported instead of being skipped.
* **Failures.** `verification.failures` lists per-container hash/decrypt/parse failures.
  `all_checks_passed` is true only when the list is empty.
* **Entropy.** `byte_statistics.shannon_entropy_bits_per_byte` describes the 32 key bytes
  themselves. A 32-byte sample cannot exceed 5.0 bits/byte; the value is not a key-strength
  estimate. I read 5.0 as a sample ceiling, not a key-strength verdict; calling it "weak key"
  is measuring the wrong thing, stupid.

The runtime does not search on-disk binaries for duplicate copies of the key; the
`detection` block describes only the in-memory buffer that verified.

---

## 6. Threading & lifetime

| Object | Thread safety |
| --- | --- |
| `AESKeyFinder::Scan` | Blocking; called on the init thread. Uses locals, safe-memory reads, one shared BCrypt provider. |
| `AESKeyFinder::ScanAsync` | Spawns a detached `CreateThread` around `Scan`; exceptions are caught and logged. |
| `AESKeyFinder::IsScanFinished` | `std::atomic<bool>`. |
| `AESKeyFinder::GetResult` | Copies the stored `AESKeyResult` under `std::mutex`. |
| `PakVerifier::*` | Stateless static functions; `PakVerifyContext` is plain data. |

`VerifyKey` reuses a process-wide BCrypt AES provider (`SharedAesProvider`) and creates one
symmetric key per candidate. The report pass creates one AES key for all containers it
decrypts. `ScanAsync` returns immediately. I check `IsScanFinished()` before `GetResult()`;
calling it without that is racing the scan.

---

## 7. Failure modes & diagnostics

| Symptom in log / report | Meaning | Action |
| --- | --- | --- |
| `No pak/IoStore containers found. Aborting.` | The search roots contain no `.pak`/`.utoc`/`.ucas` | Check the game layout; the search paths derive from the EXE directory |
| `Container indexes are not encrypted; no key is required.` | Nothing to recover; inventory is still reported | Nothing |
| `Could not read encrypted index blocks; key search skipped.` | Encrypted archives exist but no index could be read | Check file access/anti-tamper; inspect `archives[].error` |
| `Scan finished in N ms. Candidates tested: X | Key found: NO` | No candidate verified | The key may be generated/encrypted at runtime, or outside the supported pak/IoStore layouts; check `verification.failures` |
| `verification.failures` contains hash mismatches | A key was found, but one container disagrees | The container may use a different key GUID; the rest of the report remains valid |
| `verification.all_checks_passed == false` with no key | Encrypted indexes could not be decrypted | Supply/rediscover the key; the census still describes the archives |

When an archive's primary index hash mismatches, its `primary_index.hash_verified` is false
and the detailed directory parse is skipped; the census still counts its footer/index.

---

## 8. Source map

| File | Contents |
| --- | --- |
| [`AESKeyFinder.h`](../../Runtime/crypto/AESKeyFinder.h) | `AESKeyResult`, `ExeVersionInfo`, `AESKeyFinder` public statics |
| [`AESKeyFinder.cpp`](../../Runtime/crypto/AESKeyFinder.cpp) | Candidate filters, entropy, formatters, version metadata, JSON writer, scanners, `Scan` |
| [`PakVerifier.h`](../../Runtime/crypto/PakVerifier.h) | `PakInfo`, `PakFooterInfo`, `IoStoreTocInfo`, report structs, `PakVerifier` |
| [`PakVerifier.cpp`](../../Runtime/crypto/PakVerifier.cpp) | Footer/TOC/index decoding, AES, `VerifyKey`, `BuildArchiveReports` |
| [`Runtime/core/dllmain.cpp`](../../Runtime/core/dllmain.cpp) | Startup hook (`AutoDiscoverAESKey`) |

---

## Cross-references

* [REFERENCE.md](REFERENCE.md): fields, functions and byte-level layouts.
* [RuntimeAPI/CONFIG.md](../RuntimeAPI/CONFIG.md): `AutoDiscoverAESKey` and aliases.
* [RuntimeAPI/README.md](../RuntimeAPI/README.md): startup sequence.
* [DiscoveryAPI/README.md](../DiscoveryAPI/README.md): pak-based asset discovery that
  consumes the key.
