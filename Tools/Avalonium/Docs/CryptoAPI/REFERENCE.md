# CryptoAPI: Reference

Complete reference for namespace `Crypto`. Two translation units:
[`AESKeyFinder.cpp`](../../Runtime/crypto/AESKeyFinder.cpp) and
[`PakVerifier.cpp`](../../Runtime/crypto/PakVerifier.cpp). Public symbols are declared in
[`AESKeyFinder.h`](../../Runtime/crypto/AESKeyFinder.h) and
[`PakVerifier.h`](../../Runtime/crypto/PakVerifier.h).

The report is a single JSON object; its fields are documented below.

---

## Part 1: `AESKeyResult`

| Field | Type | Meaning |
| --- | --- | --- |
| `key_found` | `bool` | A candidate passed `VerifyKey` |
| `key_verified` | `bool` | Set with `key_found`; verification is mandatory |
| `key_bytes[32]` | `uint8_t[32]` | Raw key; used by the report pass |
| `key_hex` | `std::string` | `"0xAABB…"`, 64 hex chars |
| `key_spaced` | `std::string` | `"0xAA 0xBB …"` |
| `key_base64` | `std::string` | Standard base64 of the 32 bytes |
| `key_c_array` | `std::string` | `"{ 0xAA, 0xBB, … }"` |
| `unrealpak_arg` | `std::string` | `"-aes=0x…"` |
| `source` | `std::string` | Candidate source tag (`data_raw_buffer`, …) |
| `entropy` | `double` | Shannon entropy of the 32 key bytes (max observable 5.0) |
| `candidates_tested` | `int` | Candidates passed to `VerifyKey` |
| `scan_duration_ms` | `int64_t` | Memory-scan wall time |
| `report_build_ms` | `int64_t` | Time spent verifying/decoding every container |
| `source_address` | `uintptr_t` | Runtime address of the winning buffer |
| `source_module` | `std::string` | Module containing it |
| `source_module_offset` | `uint64_t` | RVA relative to that module |
| `source_module_offset_valid` | `bool` | Whether the address was inside the module range |
| `source_section` | `std::string` | PE section name for the RVA (e.g. `.rdata`) |
| `game_name` | `std::string` | Version-resource `ProductName`, else executable stem |
| `engine_version` | `std::string` | `"4.26"` / `"5.4"` from the fixed product version, when present |
| `version` | `ExeVersionInfo` | Parsed version resource |
| `any_encryption` | `bool` | At least one pak/UTOC is encrypted |
| `archives` | `std::vector<ArchiveReport>` | One report per pak/UTOC/ucas |
| `totals` | `ReportTotals` | Census and verification counters |

### `ExeVersionInfo`

| Field | Source |
| --- | --- |
| `product_name`, `file_description`, `company_name`, `file_version`, `product_version`, `original_filename`, `internal_name` | `\StringFileInfo\<lang><codepage>\<key>` |
| `fixed_file_version`, `fixed_product_version` | `VS_FIXEDFILEINFO` |
| `product_major`, `product_minor` | High/low words of `dwProductVersionMS` |
| `valid` | Any version data was recovered |

The scan loads `version.dll` dynamically, so no import library is required.

### `ToJson(const std::string& exe_path)`

Manual serializer backed by a small `JsonOut` writer. Strings escape `"`, `\`, control
characters and `\u00XX`; UTF-8 path bytes pass through. Output is always valid JSON, including
the no-key and no-container paths.

---

## Part 2: JSON report

### Top level

| Key | Contents |
| --- | --- |
| `generated_by` | `tool`, `generated_at_utc`, `source_truth` |
| `scan` | `mode`, `timestamp_utc`, `duration_ms`, `candidates_tested`, `report_build_ms` |
| `game` | `name`, `executable`, `executable_path`, `executable_size`, `version_info` |
| `engine` | `family`, `version`, `evidence` |
| `encryption` | algorithm/mode/size, `key_guid`, GUID meaning, pak/IoStore encryption flags |
| `key` | found/verified, all key encodings, `detection`, `byte_statistics` |
| `verification` | method, per-index counts, `sample_entries_decoded`, `all_checks_passed`, `failures` |
| `census` | container counts, encrypted counts, entry/chunk/file counts, byte totals |
| `target_archive` | First key-verified pak (else first parsed UTOC) and its role |
| `archives` | Per-container reports |
| `format_reference` | Source-derived format notes and source file names |
| `notes` | Human-readable findings |

### `key`

Successful key:

| Key | Meaning |
| --- | --- |
| `found`, `verified` | Always `true` together in a successful scan |
| `hex`, `hex_spaced`, `base64`, `c_array` | The same 32 bytes in four forms |
| `unrealpak_arg` | `-aes=0x…` |
| `detection.method` | Source tag |
| `detection.candidates_tested` | Candidate count |
| `detection.runtime_address` | Winning address (hex string) |
| `detection.module` | Module file name |
| `detection.module_offset` | RVA within the module (hex string) |
| `detection.section` | PE section name |
| `detection.on_disk_search` | `"not performed"`: detection metadata describes the in-memory buffer only |
| `byte_statistics.sample_bytes` | `32` |
| `byte_statistics.unique_byte_values` | Distinct bytes in the key |
| `byte_statistics.shannon_entropy_bits_per_byte` | Entropy over the 32-byte sample (ceiling 5.0) |
| `byte_statistics.sample_entropy_ceiling_bits_per_byte` | `5.0` |
| `byte_statistics.note` | Explains that 5.0 is a sample-size ceiling, not key strength |

No-key paths use `found: false` plus `required` and `note`. I ignore `key.hex` when `found` is
false; feeding it to UnrealPak earns the error, and it's not the report's fault.

### `verification`

| Key | Meaning |
| --- | --- |
| `method` | AES-ECB + SHA-1 (pak) and structural IoDirectoryIndex parsing (UTOC), or the unencrypted integrity variant |
| `encrypted_containers` | Any encrypted pak/UTOC |
| `pak_primary_indexes` | `{ total, verified }` over paks with a stored primary hash |
| `pak_secondary_indexes` | `{ total, verified }` over path-hash + full-directory indexes with stored hashes |
| `iostore_directory_indexes` | `{ total, parsed }` over UTOCs with a directory index |
| `sample_entries_decoded` | Decoded `FPakEntry` samples |
| `all_checks_passed` | `true` iff `failures` is empty |
| `failures` | Optional array, first 20 failure strings |

One bad container flips `all_checks_passed`; I require every container to pass, and
cherry-picking the ones that did is not a clean report, dumbass.

### `census`

| Key | Meaning |
| --- | --- |
| `containers` | `{ total, pak, utoc, ucas }` |
| `encrypted` | `{ paks, utocs, ucas_chunk_data }` |
| `pak_entries` | Sum of pak primary-index entry counts |
| `pak_indexed_files` | Sum of decoded full-directory file counts |
| `iostore_chunks` | Sum of UTOC entry counts |
| `iostore_indexed_files` | Sum of UTOC directory file counts |
| `iostore_non_file_chunks` | `iostore_chunks - iostore_indexed_files` |
| `bytes` | `{ pak, utoc, ucas, total }` |

### Archive objects

Common fields: `file_name`, `kind` (`pak`, `iostore_toc`, `iostore_container`),
`file_size`, `encrypted`, optional `error`.

**Pak**

| Key | Meaning |
| --- | --- |
| `footer` | `offset`, `size`, `ends_at_eof`, `magic`, `version`, `version_name`, `encrypted_index`, `encryption_key_guid`, `compression_methods` |
| `primary_index` | Offsets/sizes, `encrypted`, stored/computed SHA-1, `hash_verified`, `mount_point`, `entry_count`, `path_hash_seed`, index flags, encoded/unencoded entry sizes, nested `path_hash_index` and `full_directory_index` metadata |
| `directory` | `present`, `encrypted`, `decrypted`, offset/size, stored/computed SHA-1, `hash_verified`, serialized/padding sizes, `directory_count`, `file_count`, `extension_counts`, `sample_paths`, `sample_entries` |
| `paired_container` | UTOC with the same stem, when present (metadata only) |

**IoStore TOC**

| Key | Meaning |
| --- | --- |
| `toc_header` | `version`, `version_name`, `header_size`, `container_id`, `container_flags` + `container_flag_names`, `encryption_key_guid`, entry/block counts, block/name sizes, compression methods, partitions, perfect-hash counts, directory-index size/offset, signature bytes, 33-byte entry meta totals, tail consistency, `chunk_type_counts` |
| `directory_index` | Object, or `null` when the TOC has none: mount point, directory/file/string counts, walked file count, serialized/padding sizes, extension counts, sample paths and `{ path, chunk_index }` files |
| `paired_container` | `.ucas` with the same stem |

`container_id` is the 64-bit `FIoContainerId`. `FFFFFFFFFFFFFFFF` is
`FIoContainerId::InvalidId` (`uint64(-1)`), used by the global container; such reports set
`container_id_valid: false` and include a `container_id_note`. I treat `FFFFFFFFFFFFFFFF` as
the engine's own InvalidId, not a parser bug and not a failure.

**IoStore payload**

| Key | Meaning |
| --- | --- |
| `paired_toc` | Source TOC |
| `chunk_data_encrypted` | TOC `Encrypted` flag; chunk payloads use the same key |

---

## Part 3: `AESKeyFinder`

### `LooksLikeKey`

Cheap pre-filter before BCrypt verification. Returns `false` when any rule rejects:

| # | Rule |
| --- | --- |
| 1 | Not all 32 bytes identical |
| 2 | At least 14 distinct byte values |
| 3 | At most 4 zero bytes |
| 4 | Fewer than 28 printable ASCII bytes |
| 5 | Not a padded base64/UTF-8 artifact (`NUL` ×3 plus `?`/`¿` repeats) |
| 6 | Entropy at least 3.5 bits/byte |
| 7 | Longest identical-byte run shorter than 5 |

### `CalculateEntropy`

Shannon entropy in bits/byte. The 32-byte path uses `PROB_LOG2_TABLE[33]`; other lengths use
`std::log2`. For a 32-byte input the maximum observable value is `log2(32) = 5.0`.

### Formatters

```cpp
FormatHex(key, 32, true)   // "0xAABBCC…"
FormatHex(key, 32, false)  // "0xAA 0xBB …"
FormatBase64(key, 32)      // standard base64 with '=' padding
FormatCArray(key, 32)      // "{ 0xAA, 0xBB, … }"
```

### `Scan`

1. Resolve the executable path; query `ExeVersionInfo`; derive `game_name` and
   `engine_version`.
2. `PakVerifier::CheckEncryption` parses every container and fills the census.
3. `PakVerifier::BuildVerifyContext` loads verification material.
4. If encryption is present and the context is ready, walk the main executable, then game
   modules, with `VirtualQuery`.
5. Each candidate goes through `TryCandidate` → `VerifyKey`; the first verified key sets
   `key_*`, `source_*` and stops the scan.
6. `BuildArchiveReports` runs regardless of key success, then `SaveJson`.
7. The result is stored under `s_Mutex` and `s_Done` is set.

`ScanAsync` wraps `Scan` in a detached thread and logs exceptions as
`Exception caught in ScanThread!`.

### Candidate scanners

`ScanDataSections` uses 64 KiB chunks rounded down to 16 bytes and runs, in order:
`data_raw_buffer` → `hex_literal_0x` → `hex_literal_64char` → `utf16_hex_literal` →
`base64_string_data`.

`ScanCodeImmediates` uses the same chunking with two x86-64 detectors:

* `mov_imm64_code`: four consecutive `48 B8..BF imm64` loads within 80 bytes.
* `mov_imm32_code`: eight `C7 … imm32` stores within 96 bytes, handling REX/SIB ModRM forms.

Both stop at the first verified candidate.

---

## Part 4: `PakVerifier` data types

### `PakFooterInfo`

| Field | Meaning |
| --- | --- |
| `valid` | A footer was parsed |
| `offset`, `size` | Footer start/size in the file |
| `ends_at_eof` | Footer end equals file size |
| `magic`, `version`, `version_name` | `0x5A6F12E1` and the `EPakVersion` name |
| `index_offset`, `index_size` | Primary index location/size |
| `encrypted_index` | `bEncryptedIndex` |
| `encryption_key_guid[16]` | GUID (v7+) |
| `stored_index_hash[20]`, `has_stored_index_hash` | SHA-1 of the decrypted primary index |
| `compression_methods` | On-disk name slots (v8+), empty names skipped |

### `IoStoreTocInfo`

Holds `version`, `version_name`, `header_size`, `entry_count`, `block_count`,
`block_entry_size`, `method_count`, `method_length`, `block_size`, `dir_size`, `dir_offset`,
`partition_count`, `partition_size`, `container_id`, `encryption_key_guid`, `flags`,
`seed_count`, `without_hash_count`, `signature_bytes`, `meta_bytes`, `meta_consistent`,
`methods`, and `chunk_type_counts`.

### `PakInfo`

One discovered container. Identity: `pak_path`, `file_size`, `is_utoc`, `is_ucas`,
`is_encrypted`, `paired_path`. Pak: `pak_version`, `index_offset`, `index_size`,
`encryption_key_guid`, `index_hash`, `has_index_hash`, `compression_methods`,
`has_relative_chunk_offsets`, `is_frozen_index`, `footer`. UTOC: `toc`. Census counters:
`total_pak_files`, `encrypted_pak_files`, `total_utoc_files`, `encrypted_utoc_files`,
`total_ucas_files`, `encrypted_ucas_files`.

### Report types

| Type | Role |
| --- | --- |
| `IndexReport` | One index: present/offset/size/serialized/padding, encryption, stored/computed SHA-1, verification, error |
| `DirectoryReport` | Directory index: present/encrypted/decrypted, hash state, mount point, counts, extension histogram, sample paths/files/entries |
| `PakSampleEntry` | Decoded encoded-entry fields |
| `ArchiveReport` | Per-container payload (pak/utoc/ucas) |
| `ReportTotals` | Census + verification counters + failures |
| `ReportOptions` | `max_samples=20`, `max_entries=8`, `max_extensions=32` |

### `PakVerifyContext`

| Field | Meaning |
| --- | --- |
| `primary_info` | Archive chosen by `CheckEncryption` |
| `archives` | Up to 32 encrypted index tests (paks preferred; otherwise UTOCs) |
| `all_discovered_paks` | Every parsed pak/UTOC/ucas |
| `ready` | At least one index test was loaded |

`ArchiveIndexTest` holds `info`, the raw index bytes, `valid_read_bytes`, and `is_utoc`.

`DecryptedIndexSummary` is the compact verification log payload: archive name, mount point,
entry/file count, seed, index flags, `is_iostore`.

---

## Part 5: `PakVerifier` API

### `FindPakFiles`

Returns absolute paths of `.pak`, `.utoc` and `.ucas` found non-recursively under the same
roots, sorted and deduplicated:

1. `<game_root>/Content/Paks` (`game_root` = three parent levels above the EXE dir)
2. `<exe_dir>/../../Content/Paks`
3. `<exe_dir>/../Content/Paks`
4. `<exe_dir>/Content/Paks`
5. `<game_root>/Content`

Extension matching is case-insensitive. Empty `exe_path` means the current process EXE.

### `ParsePakFile`

Reads the last ≤ 8192 bytes and scans backwards for `EPakVersion` values `1..1000` whose
`IndexOffset`/`IndexSize` fit the file. It fills `PakFooterInfo` with:

* `bEncryptedIndex` at `magic-1`;
* `EncryptionKeyGuid` at `magic-17` for v7+;
* 20-byte index hash at `magic+24`;
* optional frozen flag and compression names (see Part 6);
* footer offset = `magic - 17` (v7+) or `magic - 1`, size from `GetSerializedSize`.

Primary indexes at file offset 0 (empty patch paks) are accepted; their primary,
path-hash and directory indexes are still hashed and reported. A pak without a
valid footer returns `found_pak == false`.

### `ParseUtocFile`

Reads the 144-byte `FIoStoreTocHeader`, validates the 16-byte magic, parses every field,
then computes the directory-index offset from the source-true layout:

```
header_size
+ entry_count * 12                  // FIoChunkId
+ entry_count * 10                  // FIoOffsetAndLength
+ perfect_hash_seed_count * 4       // version >= PerfectHash
+ chunks_without_perfect_hash * 4   // version >= PerfectHashWithOverflow
+ compressed_block_count * block_entry_size
+ method_count * method_length
+ signature_bytes                   // when Signed
```

`signature_bytes = 4 + 2 * hashSize + block_count * 20`. The directory index is
`dir_size` bytes at that offset. Metadata tail = `file_size - (dir_offset + dir_size)`;
it is compared with `entry_count * 33` (plus OnDemand metadata when flagged).
`is_encrypted` is flags bit `0x02` only; the GUID is reported separately.

### `ParseUcasFile`

Records size and path. Encryption is derived from the paired UTOC during report building.

### `CheckEncryption`

Parses every discovered file, pairs `.utoc` ↔ `.ucas` by stem (`_sN` partitions stripped),
counts paks/UTOCs/ucas and their encrypted variants, and chooses a primary archive:

1. first encrypted pak; else
2. first encrypted UTOC; else
3. first pak; else
4. first UTOC; else
5. first parsed archive.

Logs one line per pak/UTOC with the corrected format version, encryption state and index
location.

### `BuildVerifyContext`

Parses all files into `all_discovered_paks`. If any encrypted pak has a stored index hash,
the verification tests are the encrypted pak indexes (largest first). Otherwise encrypted
IoStore directory indexes are used, which supports IoStore-only games. Tests are capped at
32 archives / 128 MiB of index data. The first test is the preview archive. We cap it
deliberately; raising the caps because "more must be better" only makes startup slower and
fatter, stupid.

### `VerifyKey`

1. Decrypt the first ≤64 aligned bytes of the preview index and run
   `QuickCheckIndexHeader`. A failure rejects the candidate immediately.
2. For every test:
   * pak: full decrypt, SHA-1 over the **raw decrypted bytes**; on match, summarize the
     mount point/entry count. For pak variant `101`, the structural summary is parsed from
     a copy XORed with `0x1D` after the hash check.
   * UTOC: full decrypt and `ValidateIoStoreDirectory` (mount point, counts and string table
     bounds); no stored hash exists for IoStore directory indexes.
3. First success fills `DecryptedIndexSummary` and returns.

A process-wide `SharedAesProvider` avoids reopening the BCrypt provider per candidate.

### `BuildArchiveReports`

Runs after the key scan, with or without a key:

* **pak**: read/decrypt primary index, verify SHA-1, parse mount/entries/secondary metadata,
  verify path-hash and full-directory SHA-1, parse the directory map, collect extension
  counts/sample paths, decode up to `max_entries` `FPakEntry` samples. Variant `101` parsing
  uses the XOR-0x1D copy while hashes stay over the raw bytes.
* **UTOC**: count chunk types from `FIoChunkId[11]`, read/decrypt the directory index with
  the same key when `Encrypted`, parse the tree, collect counts/samples.
* **ucas**: size, paired TOC and chunk-encryption state.

The totals count primary/secondary/index verifications, decoded samples, container bytes,
entry/chunk/file counts and failure strings. `all_checks_passed` is true iff no failures.

### `DecryptAES256ECB`

One-shot AES-256-ECB decrypt through BCrypt. Requires non-null pointers, `len > 0` and
`len % 16 == 0`.

### `QuickCheckIndexHeader`

Structural gate on the first ≤64 decrypted bytes (XOR `0x1D` first for pak variant 101):

* ANSI FString `1..256`: NUL termination, printable bytes, sane following entry count.
* UTF-16 FString `-256..-1`: zero high bytes, printable low bytes, trailing NULs.
* Empty FString: accepts a following entry count in `(0, 50 000 000)`.

---

## Part 6: Byte-level layouts

### `FPakInfo`

Serialized order (source: `FPakInfo::Serialize`):

```
EncryptionKeyGuid (v7+)
bEncryptedIndex
Magic                 0x5A6F12E1
Version
IndexOffset
IndexSize
IndexHash             20-byte SHA-1 of the decrypted index
bIndexIsFrozen        v9 only
CompressionMethodName[5][32]  v8+
```

For v11 the footer is **221 bytes**, from `magic-17` through `magic+204`. The on-disk
compression name slot `k` corresponds to `FPakEntry.CompressionMethodIndex == k+1`;
index `0` means `None`.

### Pak primary index

```
FString MountPoint
int32   NumEntries
uint64  PathHashSeed
int32   bHasPathHashIndex
          int64 Offset, int64 Size, FSHAHash (20)
int32   bHasFullDirectoryIndex
          int64 Offset, int64 Size, FSHAHash (20)
TArray<uint8> EncodedPakEntries     // int32 count + bytes
int32   NumUnencodedEntries
          serialized FPakEntry records
```

The primary index SHA-1 covers the entire decrypted index, including AES padding.
Secondary indexes are encrypted with the same key and individually hashed.

`FPakEntry` serialization: `Offset`, `Size`, `UncompressedSize`, compression-method index,
20-byte hash, compression-block array when compressed, `Flags`, `CompressionBlockSize`
(timestamp only in v1).

### Pak directory index

`FDirectoryIndex` is `TMap<FString relative directory, TMap<FString file name,
FPakEntryLocation>>`. Serialization:

```
int32 directory_count
  FString directory            // ends with '/', root is "/"
  int32 file_count
    FString file_name
    int32 FPakEntryLocation
```

`FPakEntryLocation`: `0..0x7FFFFFFE` = offset into encoded entries; `0x7FFFFFFF` /
`0x80000000` = invalid; `0x80000001..0xFFFFFFFF` = unencoded list index `-(Index+1)`.

`DecodePakEntry` bitfield:

```
bit 31   offset is 32-bit
bit 30   uncompressed size is 32-bit
bit 29   compressed size is 32-bit
bits 23-28 compression method index
bit 22   encrypted
bits 6-21 compression block count
bits 5-0 compression block size (0x3F = following u32)
```

If the block count is 1, the block size equals the uncompressed size; if 0, the size is 0.

### `FIoStoreTocHeader` (144 bytes)

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 16 | `TocMagic` (`-==--==--==--==-`) |
| 16 | 1 | `Version` |
| 20 | 4 | `TocHeaderSize` |
| 24 | 4 | `TocEntryCount` |
| 28 | 4 | `TocCompressedBlockEntryCount` |
| 32 | 4 | `TocCompressedBlockEntrySize` (12) |
| 36 | 4 | `CompressionMethodNameCount` |
| 40 | 4 | `CompressionMethodNameLength` |
| 44 | 4 | `CompressionBlockSize` |
| 48 | 4 | `DirectoryIndexSize` |
| 52 | 4 | `PartitionCount` |
| 56 | 8 | `ContainerId` |
| 64 | 16 | `EncryptionKeyGuid` |
| 80 | 1 | `ContainerFlags` |
| 84 | 4 | `TocChunkPerfectHashSeedsCount` |
| 88 | 8 | `PartitionSize` |
| 96 | 4 | `TocChunksWithoutPerfectHashCount` |

Flags: `Compressed=1`, `Encrypted=2`, `Signed=4`, `Indexed=8`, `OnDemand=16`.
`EIoStoreTocVersion`: `Initial=1`, `DirectoryIndex=2`, `PartitionSize=3`, `PerfectHash=4`,
`PerfectHashWithOverflow=5`, `OnDemandMetaData=6`.

`ContainerId` is a 64-bit `FIoContainerId`. `FFFFFFFFFFFFFFFF` is `InvalidId`
(`uint64(-1)`); `global.utoc` carries it because the global container has no assigned ID.

### IoStore TOC body

```
FIoChunkId[entry_count]                12 bytes each
FIoOffsetAndLength[entry_count]        10 bytes each (5-byte offset + 5-byte length)
int32 seeds[seed_count]                version >= 4
int32 without_hash[count]              version >= 5
FIoStoreTocCompressedBlockEntry[block_count]  12 bytes each
ANSICHAR methods[method_count][method_length]
signatures                             4 + 2*hashSize + block_count*20, Signed only
directory index                        DirectoryIndexSize bytes
FIoStoreTocEntryMeta[entry_count]      33 bytes each (32-byte hash + 1 flags)
OnDemand metadata                      when the OnDemand flag is set
```

Each `FIoStoreTocEntryMeta` contributes exactly **33 bytes** (`FIoChunkHash` 32 +
`FIoStoreTocEntryMetaFlags` 1). The directory index is encrypted as one AES-256-ECB blob
when the container is `Encrypted`; the header stays readable. I count 33 bytes per meta
entry; wrong arithmetic puts the directory index somewhere in the metadata, not the game
being weird, just stupid.

A chunk's type is `FIoChunkId.Id[11]` (`ExportBundleData=1`, `BulkData=2`, …,
`ContainerHeader=6`, `ShaderCode=9`).

### IoStore directory index

`FIoDirectoryIndexResource`:

```
FString MountPoint
int32   directory_count
  uint32 Name, FirstChildEntry, NextSiblingEntry, FirstFileEntry      (16 bytes)
int32   file_count
  uint32 Name, NextFileEntry, UserData                               (12 bytes)
int32   string_count
  FString strings[]
```

Names index the string table; `UserData` is the TOC entry index. Traversal starts at root
directory `0`; `0xFFFFFFFF` terminates child/sibling/file chains. Paths are
`MountPoint + directory names + file name`.

### Encryption

Pak indexes, pak entry payloads, IoStore directory indexes and IoStore chunk payloads use
AES-256 in **ECB** mode with 16-byte blocks and the same 32-byte key. AES padding is included
in the stored index size and therefore in the stored SHA-1.

Pak variant `101` is handled specially: after decryption, structural parsing uses a copy
XORed with `0x1D`; the stored SHA-1 remains over the raw decrypted bytes.

---

## Part 7: Constants

| Constant | Value | Location |
| --- | --- | --- |
| `PAK_MAGIC` | `0x5A6F12E1` | `PakVerifier.cpp` |
| UTOC magic | `2D3D3D2D 2D3D3D2D 2D3D3D2D 2D3D3D2D` | `PakVerifier.cpp` |
| Pak footer window | 8192 bytes | `ParsePakFile` |
| Footer size v11 | 221 bytes | `ParsePakFile` |
| TOC entry size | 22 bytes (12 + 10) | `ParseUtocFile` |
| TOC entry meta size | 33 bytes | `BuildArchiveReports` |
| Max index read | 128 MiB | `ReadExact` |
| Max verification tests | 32 | `BuildVerifyContext` |
| Scan chunk size | 64 KiB, 16-aligned | `AESKeyFinder.cpp` |
| Entropy ceiling (32-byte sample) | 5.0 bits/byte | `byte_statistics` |

---

## Part 8: Log tags

| Line | Where |
| --- | --- |
| `Starting dynamic in-process archive encryption scan...` | `Scan` |
| `Found archive: …` | `CheckEncryption` |
| `Target archive: …` | `Scan` |
| `Scanning main executable data & code sections...` | `Scan` |
| `SUCCESS! Key FOUND & VERIFIED: …` | `TryCandidate` |
| `Verified against pak index: …` / `Verified against IoStore directory: …` | `TryCandidate` |
| `Scan finished in … ms. Candidates tested: … | Key found: NO` | `Scan` |
| `Building archive report for … containers...` | `BuildArchiveReports` |
| `Report: <container> …` | `BuildPakReport` / `BuildUtocReport` |
| `Archive report built in … ms: …` | `BuildArchiveReports` |
| `JSON saved: …` | `SaveJson` |
| `Exception caught in ScanThread!` | `ScanThread` |

All log output goes through `EngineDiscovery::WriteLogLine`.

---

## Cross-references

* [README.md](README.md): overview, outputs, failure modes.
* [RuntimeAPI/CONFIG.md](../RuntimeAPI/CONFIG.md): `AutoDiscoverAESKey`, logging tags.
* [SafeMemAPI/README.md](../SafeMemAPI/README.md): `SafeMem_Read` used by the scanners.
