#include "PakVerifier.h"
#include "../core/EngineDiscovery.h"

#include <windows.h>
#include <bcrypt.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <format>
#include <functional>
#include <map>
#include <set>
#include <vector>

namespace fs = std::filesystem;

namespace Crypto {

constexpr uint32_t PAK_MAGIC = 0x5A6F12E1;
constexpr uint8_t UTOC_MAGIC[16] = {
    0x2D,0x3D,0x3D,0x2D, 0x2D,0x3D,0x3D,0x2D,
    0x2D,0x3D,0x3D,0x2D, 0x2D,0x3D,0x3D,0x2D
};
constexpr size_t AES_BLOCK = 16;

static void Log(const std::string& line) {
    EngineDiscovery::WriteLogLine((line + "\n").c_str());
}

static const char* PakVersionName(int version) {
    switch (version) {
    case 1: return "PakFile_Version_Initial";
    case 2: return "PakFile_Version_NoTimestamps";
    case 3: return "PakFile_Version_CompressionEncryption";
    case 4: return "PakFile_Version_IndexEncryption";
    case 5: return "PakFile_Version_RelativeChunkOffsets";
    case 6: return "PakFile_Version_DeleteRecords";
    case 7: return "PakFile_Version_EncryptionKeyGuid";
    case 8: return "PakFile_Version_FNameBasedCompressionMethod";
    case 9: return "PakFile_Version_FrozenIndex";
    case 10: return "PakFile_Version_PathHashIndex";
    case 11: return "PakFile_Version_Fnv64BugFix";
    default: return "Unknown";
    }
}

static const char* TocVersionName(int version) {
    switch (version) {
    case 1: return "Initial";
    case 2: return "DirectoryIndex";
    case 3: return "PartitionSize";
    case 4: return "PerfectHash";
    case 5: return "PerfectHashWithOverflow";
    case 6: return "OnDemandMetaData";
    default: return "Unknown";
    }
}

static const char* ChunkTypeName(uint8_t type) {
    switch (type) {
    case 0: return "Invalid";
    case 1: return "ExportBundleData";
    case 2: return "BulkData";
    case 3: return "OptionalBulkData";
    case 4: return "MemoryMappedBulkData";
    case 5: return "ScriptObjects";
    case 6: return "ContainerHeader";
    case 7: return "ExternalFile";
    case 8: return "ShaderCodeLibrary";
    case 9: return "ShaderCode";
    case 10: return "PackageStoreEntry";
    case 11: return "DerivedData";
    case 12: return "EditorDerivedData";
    case 13: return "PackageResource";
    default: return "Unknown";
    }
}

static std::string HexUpper(const uint8_t* data, size_t len) {
    static const char digits[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(digits[data[i] >> 4]);
        out.push_back(digits[data[i] & 0x0F]);
    }
    return out;
}

static std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

static std::string FileNameOf(const std::string& path) {
    return fs::path(path).filename().string();
}

static std::string StemKey(const std::string& path) {
    std::string stem = ToLowerAscii(fs::path(path).stem().string());
    const size_t pos = stem.rfind("_s");
    if (pos != std::string::npos && pos + 2 < stem.size()) {
        bool all_digits = true;
        for (size_t i = pos + 2; i < stem.size(); ++i) {
            if (stem[i] < '0' || stem[i] > '9') { all_digits = false; break; }
        }
        if (all_digits) stem.resize(pos);
    }
    return stem;
}

static std::string ExtensionOf(const std::string& name) {
    const size_t slash = name.find_last_of("/\\");
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos) return {};
    if (slash != std::string::npos && dot < slash) return {};
    return ToLowerAscii(name.substr(dot));
}

static std::string JoinMount(std::string mount, std::string rel) {
    if (rel.empty() || rel.front() != '/') rel.insert(rel.begin(), '/');
    while (!mount.empty() && mount.back() == '/') mount.pop_back();
    return mount + rel;
}

static void ApplyPakVariant(std::vector<uint8_t>& data, int version) {
    if (version != 101) return;
    for (auto& byte : data) byte ^= 0x1D;
}

static void FastSHA1(const uint8_t* data, size_t len, uint8_t out[20]) {
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    auto rol = [](uint32_t val, uint32_t bits) -> uint32_t {
        return (val << bits) | (val >> (32 - bits));
    };

    const uint64_t bit_len = static_cast<uint64_t>(len) * 8;
    const size_t padded_len = (len + 8 + 64) / 64 * 64;
    std::vector<uint8_t> pdata(padded_len, 0);
    if (len) memcpy(pdata.data(), data, len);
    pdata[len] = 0x80;
    for (int i = 0; i < 8; ++i) {
        pdata[padded_len - 1 - i] = static_cast<uint8_t>((bit_len >> (i * 8)) & 0xFF);
    }

    for (size_t chunk = 0; chunk < padded_len; chunk += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(pdata[chunk + i * 4]) << 24) |
                   (static_cast<uint32_t>(pdata[chunk + i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(pdata[chunk + i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(pdata[chunk + i * 4 + 3]));
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            const uint32_t temp = rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol(b, 30); b = a; a = temp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    const uint32_t hashes[5] = { h0, h1, h2, h3, h4 };
    for (int i = 0; i < 5; ++i) {
        out[i * 4 + 0] = static_cast<uint8_t>((hashes[i] >> 24) & 0xFF);
        out[i * 4 + 1] = static_cast<uint8_t>((hashes[i] >> 16) & 0xFF);
        out[i * 4 + 2] = static_cast<uint8_t>((hashes[i] >> 8) & 0xFF);
        out[i * 4 + 3] = static_cast<uint8_t>(hashes[i] & 0xFF);
    }
}

static std::string Sha1Hex(const uint8_t* data, size_t len) {
    uint8_t hash[20] = {};
    FastSHA1(data, len, hash);
    return HexUpper(hash, sizeof(hash));
}

class AesEcb {
public:
    ~AesEcb() { close(); }

    bool init(const uint8_t* key32) {
        close();
        if (BCryptOpenAlgorithmProvider(&alg_, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0)
            return false;
        if (BCryptSetProperty(alg_, BCRYPT_CHAINING_MODE,
                              reinterpret_cast<PUCHAR>(const_cast<LPWSTR>(BCRYPT_CHAIN_MODE_ECB)),
                              sizeof(BCRYPT_CHAIN_MODE_ECB), 0) != 0) {
            return false;
        }
        DWORD object_len = 0, result = 0;
        if (BCryptGetProperty(alg_, BCRYPT_OBJECT_LENGTH,
                              reinterpret_cast<PUCHAR>(&object_len),
                              sizeof(object_len), &result, 0) != 0) {
            return false;
        }
        key_object_.resize(object_len);
        if (BCryptGenerateSymmetricKey(alg_, &key_, key_object_.data(), object_len,
                                       const_cast<PUCHAR>(key32), 32, 0) != 0) {
            return false;
        }
        return true;
    }

    bool decrypt(const uint8_t* input, size_t len, std::vector<uint8_t>& output) {
        if (!key_ || len == 0 || (len % AES_BLOCK) != 0) return false;
        output.resize(len);
        ULONG written = 0;
        const NTSTATUS status = BCryptDecrypt(
            key_, const_cast<PUCHAR>(input), static_cast<ULONG>(len),
            nullptr, nullptr, 0, output.data(), static_cast<ULONG>(len),
            &written, 0);
        return status == 0 && written == len;
    }

private:
    void close() {
        if (key_) { BCryptDestroyKey(key_); key_ = nullptr; }
        if (alg_) { BCryptCloseAlgorithmProvider(alg_, 0); alg_ = nullptr; }
    }

    BCRYPT_ALG_HANDLE alg_ = nullptr;
    BCRYPT_KEY_HANDLE key_ = nullptr;
    std::vector<uint8_t> key_object_;
};

class AesProvider {
public:
    AesProvider() {
        if (BCryptOpenAlgorithmProvider(&alg_, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return;
        if (BCryptSetProperty(alg_, BCRYPT_CHAINING_MODE,
                              reinterpret_cast<PUCHAR>(const_cast<LPWSTR>(BCRYPT_CHAIN_MODE_ECB)),
                              sizeof(BCRYPT_CHAIN_MODE_ECB), 0) != 0) {
            return;
        }
        ready_ = true;
    }

    ~AesProvider() {
        if (alg_) BCryptCloseAlgorithmProvider(alg_, 0);
    }

    bool ok() const { return ready_; }

    bool decrypt(const uint8_t* key32, const uint8_t* input, size_t len,
                 std::vector<uint8_t>& output) {
        if (!ready_ || !key32 || len == 0 || (len % AES_BLOCK) != 0) return false;
        BCRYPT_KEY_HANDLE key = nullptr;
        if (BCryptGenerateSymmetricKey(alg_, &key, nullptr, 0,
                                       const_cast<PUCHAR>(key32), 32, 0) != 0)
            return false;
        output.resize(len);
        ULONG written = 0;
        const NTSTATUS status = BCryptDecrypt(
            key, const_cast<PUCHAR>(input), static_cast<ULONG>(len),
            nullptr, nullptr, 0, output.data(), static_cast<ULONG>(len),
            &written, 0);
        BCryptDestroyKey(key);
        return status == 0 && written == len;
    }

private:
    BCRYPT_ALG_HANDLE alg_ = nullptr;
    bool ready_ = false;
};

static AesProvider& SharedAesProvider() {
    static AesProvider provider;
    return provider;
}

class ByteReader {
public:
    ByteReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    bool ok() const { return ok_; }
    size_t tell() const { return pos_; }
    size_t remaining() const { return size_ - pos_; }

    bool need(size_t count) {
        if (!ok_ || count > size_ - pos_) { ok_ = false; return false; }
        return true;
    }

    uint8_t u8() {
        if (!need(1)) return 0;
        return data_[pos_++];
    }

    uint32_t u32() {
        if (!need(4)) return 0;
        uint32_t value = 0;
        memcpy(&value, data_ + pos_, 4);
        pos_ += 4;
        return value;
    }

    int32_t i32() { return static_cast<int32_t>(u32()); }

    uint64_t u64() {
        if (!need(8)) return 0;
        uint64_t value = 0;
        memcpy(&value, data_ + pos_, 8);
        pos_ += 8;
        return value;
    }

    int64_t i64() { return static_cast<int64_t>(u64()); }

    bool raw(void* out, size_t count) {
        if (!need(count)) return false;
        memcpy(out, data_ + pos_, count);
        pos_ += count;
        return true;
    }

    bool skip(size_t count) {
        if (!need(count)) return false;
        pos_ += count;
        return true;
    }

    bool fstring(std::string& out, size_t max_chars = 1u << 20) {
        const int32_t length = i32();
        if (!ok_) return false;
        out.clear();
        if (length == 0) return true;
        if (length > 0) {
            const size_t count = static_cast<size_t>(length);
            if (count > max_chars * 4) { ok_ = false; return false; }
            if (!need(count)) return false;
            const char* text = reinterpret_cast<const char*>(data_ + pos_);
            size_t used = 0;
            while (used < count && text[used]) ++used;
            out.assign(text, used);
            pos_ += count;
            return true;
        }
        const size_t count = static_cast<size_t>(-static_cast<int64_t>(length));
        if (count > max_chars) { ok_ = false; return false; }
        if (!need(count * 2)) return false;
        std::wstring wide(count, L'\0');
        memcpy(wide.data(), data_ + pos_, count * 2);
        pos_ += count * 2;
        const size_t terminator = wide.find(L'\0');
        if (terminator != std::wstring::npos) wide.resize(terminator);
        if (!wide.empty()) {
            const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                                 static_cast<int>(wide.size()),
                                                 nullptr, 0, nullptr, nullptr);
            if (bytes > 0) {
                out.resize(static_cast<size_t>(bytes));
                WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                    static_cast<int>(wide.size()),
                                    out.data(), bytes, nullptr, nullptr);
            }
        }
        return true;
    }

private:
    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t pos_ = 0;
    bool ok_ = true;
};

static std::vector<uint8_t> ReadExact(const std::string& path, uint64_t offset, uint64_t size) {
    if (size == 0 || size > (128ull << 20)) return {};
    HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER position; position.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(file, position, nullptr, FILE_BEGIN)) {
        CloseHandle(file);
        return {};
    }
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    DWORD read = 0;
    const BOOL ok = ReadFile(file, buffer.data(), static_cast<DWORD>(size), &read, nullptr);
    CloseHandle(file);
    if (!ok || read != size) return {};
    return buffer;
}

static std::vector<uint8_t> ReadFooter(const std::string& path, uint64_t file_size, size_t& read) {
    const size_t want = static_cast<size_t>(std::min<uint64_t>(file_size, 8192));
    if (want == 0) { read = 0; return {}; }
    auto buffer = ReadExact(path, file_size - want, want);
    read = buffer.size();
    return buffer;
}

PakInfo PakVerifier::ParsePakFile(const std::string& pak_path) {
    PakInfo info;
    info.pak_path = pak_path;

    std::error_code ec;
    const uint64_t file_size = fs::file_size(pak_path, ec);
    if (ec) return info;
    info.file_size = file_size;

    size_t read = 0;
    auto tail = ReadFooter(pak_path, file_size, read);
    if (read < 44) return info;

    int64_t magic_absolute = -1;
    int magic_index = -1;
    for (int i = static_cast<int>(read) - 44; i >= 0; --i) {
        uint32_t magic = 0;
        memcpy(&magic, tail.data() + i, 4);
        if (magic != PAK_MAGIC) continue;
        int32_t version = 0;
        memcpy(&version, tail.data() + i + 4, 4);
        if (version < 1 || version > 1000) continue;
        int64_t index_offset = 0, index_size = 0;
        memcpy(&index_offset, tail.data() + i + 8, 8);
        memcpy(&index_size, tail.data() + i + 16, 8);
        if (index_offset < 0 || index_size < 0) continue;
        if (static_cast<uint64_t>(index_offset) + static_cast<uint64_t>(index_size) > file_size)
            continue;
        magic_absolute = static_cast<int64_t>(file_size) - static_cast<int64_t>(read) + i;
        magic_index = i;
        break;
    }
    if (magic_absolute < 0) return info;

    int32_t version = 0;
    memcpy(&version, tail.data() + magic_index + 4, 4);
    uint64_t index_offset = 0, index_size = 0;
    memcpy(&index_offset, tail.data() + magic_index + 8, 8);
    memcpy(&index_size, tail.data() + magic_index + 16, 8);

    PakFooterInfo& footer = info.footer;
    footer.valid = true;
    footer.magic = PAK_MAGIC;
    footer.version = version;
    footer.version_name = PakVersionName(version);
    footer.index_offset = index_offset;
    footer.index_size = index_size;
    if (magic_index + 24 + 20 <= static_cast<int>(read)) {
        memcpy(footer.stored_index_hash, tail.data() + magic_index + 24, 20);
        for (uint8_t byte : footer.stored_index_hash) {
            if (byte) { footer.has_stored_index_hash = true; break; }
        }
    }
    footer.encrypted_index = (magic_index >= 1) && (tail[magic_index - 1] != 0);
    if (version >= 7 && magic_index >= 17) {
        memcpy(footer.encryption_key_guid, tail.data() + magic_index - 17, 16);
    }
    const int names_offset = magic_index + 44 + ((version >= 9 && version < 10) ? 1 : 0);
    if (version >= 8) {
        for (int i = 0; i < 5; ++i) {
            const int slot = names_offset + i * 32;
            if (slot + 32 > static_cast<int>(read)) break;
            const char* name = reinterpret_cast<const char*>(tail.data() + slot);
            if (name[0] == 0) continue;
            size_t length = 0;
            while (length < 32 && name[length]) ++length;
            footer.compression_methods.emplace_back(name, length);
        }
    }
    const int64_t footer_start = magic_absolute - (version >= 7 ? 17 : 1);
    const int64_t footer_end = version >= 8
        ? magic_absolute + 204 + ((version >= 9 && version < 10) ? 1 : 0)
        : magic_absolute + 44;
    footer.offset = static_cast<uint64_t>(footer_start);
    footer.size = static_cast<uint32_t>(footer_end - footer_start);
    footer.ends_at_eof = static_cast<uint64_t>(footer_end) == file_size;

    info.found_pak = true;
    info.is_utoc = false;
    info.is_ucas = false;
    info.pak_version = version;
    info.index_offset = index_offset;
    info.index_size = index_size;
    info.is_encrypted = version >= 4 ? footer.encrypted_index : false;
    info.has_relative_chunk_offsets = version >= 5;
    info.is_frozen_index = version == 9;
    info.has_index_hash = footer.has_stored_index_hash;
    memcpy(info.encryption_key_guid, footer.encryption_key_guid, 16);
    memcpy(info.index_hash, footer.stored_index_hash, 20);
    info.compression_methods = footer.compression_methods;
    return info;
}

PakInfo PakVerifier::ParseUtocFile(const std::string& utoc_path) {
    PakInfo info;
    info.pak_path = utoc_path;
    info.is_utoc = true;

    std::error_code ec;
    const uint64_t file_size = fs::file_size(utoc_path, ec);
    if (ec) return info;
    info.file_size = file_size;

    auto header = ReadExact(utoc_path, 0, 144);
    if (header.size() < 144 || memcmp(header.data(), UTOC_MAGIC, 16) != 0) return info;

    auto read32 = [&](size_t offset) -> uint32_t {
        uint32_t value = 0; memcpy(&value, header.data() + offset, 4); return value;
    };
    auto read64 = [&](size_t offset) -> uint64_t {
        uint64_t value = 0; memcpy(&value, header.data() + offset, 8); return value;
    };

    IoStoreTocInfo& toc = info.toc;
    toc.valid = true;
    toc.version = header[16];
    toc.version_name = TocVersionName(toc.version);
    toc.header_size = read32(20);
    toc.entry_count = read32(24);
    toc.block_count = read32(28);
    toc.block_entry_size = read32(32);
    toc.method_count = read32(36);
    toc.method_length = read32(40);
    toc.block_size = read32(44);
    toc.dir_size = read32(48);
    toc.partition_count = read32(52);
    toc.container_id = read64(56);
    memcpy(info.encryption_key_guid, header.data() + 64, 16);
    memcpy(toc.encryption_key_guid, header.data() + 64, 16);
    toc.flags = header[80];
    toc.seed_count = read32(84);
    toc.partition_size = read64(88);
    toc.without_hash_count = read32(96);

    if (toc.version < 4) toc.seed_count = 0;
    if (toc.version < 5) toc.without_hash_count = 0;
    if (toc.version < 3) {
        toc.partition_count = 1;
        toc.partition_size = 0xFFFFFFFFFFFFFFFFull;
    }
    if (toc.header_size < 144 || toc.header_size > file_size) return info;
    if (toc.entry_count > 20'000'000 || toc.block_count > 200'000'000) return info;
    if (toc.block_entry_size > 64 || toc.method_count > 64 || toc.method_length > 256) return info;

    const uint64_t entries_offset = toc.header_size;
    const uint64_t offsets_offset = entries_offset + static_cast<uint64_t>(toc.entry_count) * 12;
    const uint64_t seeds_offset = offsets_offset + static_cast<uint64_t>(toc.entry_count) * 10;
    const uint64_t nohash_offset = seeds_offset + static_cast<uint64_t>(toc.seed_count) * 4;
    const uint64_t blocks_offset = nohash_offset + static_cast<uint64_t>(toc.without_hash_count) * 4;
    const uint64_t names_offset = blocks_offset + static_cast<uint64_t>(toc.block_count) * toc.block_entry_size;
    if (names_offset > file_size) return info;

    if (toc.method_count && toc.method_length) {
        auto names = ReadExact(utoc_path, names_offset,
                               static_cast<uint64_t>(toc.method_count) * toc.method_length);
        if (names.size() != static_cast<uint64_t>(toc.method_count) * toc.method_length) return info;
        for (uint32_t i = 0; i < toc.method_count; ++i) {
            const char* slot = reinterpret_cast<const char*>(names.data() + i * toc.method_length);
            size_t length = 0;
            while (length < toc.method_length && slot[length]) ++length;
            toc.methods.emplace_back(slot, length);
        }
    }

    uint64_t signature_bytes = 0;
    if (toc.flags & 4) {
        auto hash_size_data = ReadExact(utoc_path, names_offset + static_cast<uint64_t>(toc.method_count) * toc.method_length, 4);
        if (hash_size_data.size() != 4) return info;
        uint32_t hash_size = 0;
        memcpy(&hash_size, hash_size_data.data(), 4);
        signature_bytes = 4ull + 2ull * hash_size + static_cast<uint64_t>(toc.block_count) * 20;
    }
    toc.signature_bytes = static_cast<uint32_t>(signature_bytes);

    const uint64_t dir_offset = names_offset + static_cast<uint64_t>(toc.method_count) * toc.method_length + signature_bytes;
    if (dir_offset > file_size) return info;
    if (dir_offset + toc.dir_size > file_size) return info;
    toc.dir_offset = dir_offset;
    toc.meta_bytes = file_size - (dir_offset + toc.dir_size);
    const uint64_t expected_meta = static_cast<uint64_t>(toc.entry_count) * 33 +
        ((toc.flags & 16) ? static_cast<uint64_t>(toc.entry_count) * 32 + static_cast<uint64_t>(toc.block_count) * 32 : 0);
    toc.meta_consistent = toc.meta_bytes == expected_meta;

    info.found_pak = true;
    info.is_encrypted = (toc.flags & 2) != 0;
    info.index_offset = toc.dir_offset;
    info.index_size = toc.dir_size;
    info.pak_version = 0;
    info.compression_methods = toc.methods;
    return info;
}

PakInfo PakVerifier::ParseUcasFile(const std::string& ucas_path) {
    PakInfo info;
    info.pak_path = ucas_path;
    info.is_ucas = true;
    std::error_code ec;
    info.file_size = fs::file_size(ucas_path, ec);
    info.found_pak = !ec;
    return info;
}

std::vector<std::string> PakVerifier::FindPakFiles(const std::string& exe_path) {
    std::string path = exe_path;
    if (path.empty()) {
        char buffer[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        path = buffer;
    }

    const fs::path exe_dir = fs::path(path).parent_path();
    fs::path game_root = exe_dir;
    for (int i = 0; i < 3 && game_root.has_parent_path(); ++i) game_root = game_root.parent_path();

    const std::vector<fs::path> search_paths = {
        game_root / "Content" / "Paks",
        exe_dir.parent_path().parent_path() / "Content" / "Paks",
        exe_dir.parent_path() / "Content" / "Paks",
        exe_dir / "Content" / "Paks",
        game_root / "Content",
    };

    std::set<std::string> unique;
    for (const auto& search_path : search_paths) {
        std::error_code ec;
        if (!fs::exists(search_path, ec)) continue;
        for (const auto& entry : fs::directory_iterator(search_path, ec)) {
            if (ec || !entry.is_regular_file()) continue;
            std::string ext = ToLowerAscii(entry.path().extension().string());
            if (ext == ".pak" || ext == ".utoc" || ext == ".ucas") {
                unique.insert(entry.path().string());
            }
        }
    }
    return std::vector<std::string>(unique.begin(), unique.end());
}

PakInfo PakVerifier::CheckEncryption(const std::string& exe_path) {
    PakInfo result;
    auto files = FindPakFiles(exe_path);
    if (files.empty()) return result;

    std::vector<PakInfo> parsed;
    parsed.reserve(files.size());
    for (const auto& file : files) {
        const std::string ext = ToLowerAscii(fs::path(file).extension().string());
        PakInfo info = ext == ".utoc" ? ParseUtocFile(file)
                     : ext == ".ucas" ? ParseUcasFile(file)
                     : ParsePakFile(file);
        if (info.found_pak) parsed.push_back(std::move(info));
    }
    if (parsed.empty()) return result;

    std::map<std::string, size_t> utoc_by_stem;
    std::map<std::string, size_t> ucas_by_stem;
    for (size_t i = 0; i < parsed.size(); ++i) {
        if (parsed[i].is_utoc) utoc_by_stem[StemKey(parsed[i].pak_path)] = i;
        else if (parsed[i].is_ucas) ucas_by_stem[StemKey(parsed[i].pak_path)] = i;
    }
    for (auto& info : parsed) {
        if (info.is_utoc) {
            const auto it = ucas_by_stem.find(StemKey(info.pak_path));
            if (it != ucas_by_stem.end()) info.paired_path = parsed[it->second].pak_path;
        } else if (info.is_ucas) {
            const auto it = utoc_by_stem.find(StemKey(info.pak_path));
            if (it != utoc_by_stem.end()) {
                info.paired_path = parsed[it->second].pak_path;
                info.is_encrypted = parsed[it->second].is_encrypted;
            }
        }
    }

    int total_paks = 0, encrypted_paks = 0;
    int total_utocs = 0, encrypted_utocs = 0;
    int total_ucas = 0, encrypted_ucas = 0;
    for (const auto& info : parsed) {
        if (info.is_utoc) {
            ++total_utocs;
            if (info.is_encrypted) ++encrypted_utocs;
        } else if (info.is_ucas) {
            ++total_ucas;
            if (info.is_encrypted) ++encrypted_ucas;
        } else {
            ++total_paks;
            if (info.is_encrypted) ++encrypted_paks;
        }
    }

    const PakInfo* primary = nullptr;
    for (const auto& info : parsed) {
        if (!info.is_utoc && !info.is_ucas && info.is_encrypted) { primary = &info; break; }
    }
    if (!primary) {
        for (const auto& info : parsed) {
            if (info.is_utoc && info.is_encrypted) { primary = &info; break; }
        }
    }
    if (!primary) {
        for (const auto& info : parsed) {
            if (!info.is_utoc && !info.is_ucas) { primary = &info; break; }
        }
    }
    if (!primary) {
        for (const auto& info : parsed) {
            if (info.is_utoc) { primary = &info; break; }
        }
    }
    if (!primary) primary = &parsed.front();

    result = *primary;
    result.found_pak = true;
    result.total_pak_files = total_paks;
    result.encrypted_pak_files = encrypted_paks;
    result.total_utoc_files = total_utocs;
    result.encrypted_utoc_files = encrypted_utocs;
    result.total_ucas_files = total_ucas;
    result.encrypted_ucas_files = encrypted_ucas;

    for (const auto& info : parsed) {
        if (info.is_ucas) continue;
        if (info.is_utoc) {
            Log(std::format("[AESKeyFinder]   Found archive: {} (IoStore TOC v{}, encrypted={}, dir=0x{:X}/{}, entries={}, flags=0x{:02X})",
                            FileNameOf(info.pak_path), info.toc.version,
                            info.is_encrypted ? "YES" : "NO",
                            info.toc.dir_offset, info.toc.dir_size,
                            info.toc.entry_count, info.toc.flags));
        } else {
            Log(std::format("[AESKeyFinder]   Found archive: {} (PAK v{}, encrypted={}, index=0x{:X}/{}, hash={})",
                            FileNameOf(info.pak_path), info.pak_version,
                            info.is_encrypted ? "YES" : "NO",
                            info.index_offset, info.index_size,
                            info.has_index_hash ? "YES" : "NO"));
        }
    }
    return result;
}

PakVerifyContext PakVerifier::BuildVerifyContext(const PakInfo& info, const std::string& exe_path) {
    PakVerifyContext ctx;
    ctx.primary_info = info;

    auto files = FindPakFiles(exe_path);
    for (const auto& file : files) {
        const std::string ext = ToLowerAscii(fs::path(file).extension().string());
        PakInfo pi = ext == ".utoc" ? ParseUtocFile(file)
                   : ext == ".ucas" ? ParseUcasFile(file)
                   : ParsePakFile(file);
        if (pi.found_pak) ctx.all_discovered_paks.push_back(std::move(pi));
    }

    bool has_encrypted_pak = false;
    for (const auto& pi : ctx.all_discovered_paks) {
        if (!pi.is_utoc && !pi.is_ucas && pi.is_encrypted &&
            pi.index_size >= 16 && pi.has_index_hash) {
            has_encrypted_pak = true;
            break;
        }
    }

    size_t total_bytes = 0;
    for (const auto& pi : ctx.all_discovered_paks) {
        if (!pi.is_encrypted || pi.is_ucas) continue;

        ArchiveIndexTest test;
        test.info = pi;
        test.is_utoc = pi.is_utoc;
        if (pi.is_utoc) {
            if (has_encrypted_pak) continue;
            if (pi.toc.dir_size < 16) continue;
            test.encrypted_index = ReadExact(pi.pak_path, pi.toc.dir_offset, pi.toc.dir_size);
        } else {
            if (pi.index_size < 16 || !pi.has_index_hash) continue;
            test.encrypted_index = ReadExact(pi.pak_path, pi.index_offset, pi.index_size);
        }
        if (test.encrypted_index.size() < 16) continue;
        test.valid_read_bytes = test.encrypted_index.size();
        if (total_bytes + test.valid_read_bytes > (128ull << 20)) break;
        total_bytes += test.valid_read_bytes;
        ctx.archives.push_back(std::move(test));
    }

    std::stable_sort(ctx.archives.begin(), ctx.archives.end(),
                     [](const ArchiveIndexTest& a, const ArchiveIndexTest& b) {
                         if (a.is_utoc != b.is_utoc) return !a.is_utoc;
                         return a.valid_read_bytes > b.valid_read_bytes;
                     });
    if (ctx.archives.size() > 32) ctx.archives.resize(32);
    ctx.ready = !ctx.archives.empty();
    return ctx;
}

bool PakVerifier::DecryptAES256ECB(const uint8_t* ciphertext, size_t len,
                                   const uint8_t* key32, uint8_t* plaintext) {
    if (!ciphertext || !key32 || !plaintext || len == 0 || (len & 15)) return false;
    AesEcb aes;
    if (!aes.init(key32)) return false;
    std::vector<uint8_t> output;
    if (!aes.decrypt(ciphertext, len, output)) return false;
    memcpy(plaintext, output.data(), output.size());
    return true;
}

static bool QuickCheckIndexHeader(const uint8_t* decrypted, size_t length, int pak_version) {
    if (!decrypted || length < 16) return false;

    uint8_t buffer[64] = {};
    const size_t test_length = (std::min)(length, sizeof(buffer));
    memcpy(buffer, decrypted, test_length);
    if (pak_version == 101) {
        for (size_t i = 0; i < test_length; ++i) buffer[i] ^= 0x1D;
    }

    const int32_t text_length = *reinterpret_cast<const int32_t*>(buffer);
    if (text_length > 0 && text_length <= 256) {
        const size_t count = static_cast<size_t>(text_length);
        if (4 + count <= test_length && buffer[4 + count - 1] != 0) return false;
        const size_t chars = (std::min)(count - 1, test_length - 4);
        if (chars == 0) return false;
        for (size_t i = 0; i < chars; ++i) {
            const uint8_t c = buffer[4 + i];
            if (c < 0x20 || c > 0x7E) return false;
        }
        const size_t after = 4 + count;
        if (after + 4 <= test_length) {
            int32_t entries = 0;
            memcpy(&entries, buffer + after, 4);
            if (entries < 0 || entries > 50000000) return false;
        }
        return true;
    }
    if (text_length < 0 && text_length >= -256) {
        const size_t chars = static_cast<size_t>(-text_length);
        const size_t bytes = chars * 2;
        if (4 + bytes <= test_length && (buffer[4 + bytes - 2] != 0 || buffer[4 + bytes - 1] != 0))
            return false;
        const size_t check = (std::min)(chars - 1, (test_length - 4) / 2);
        if (check == 0) return false;
        for (size_t i = 0; i < check; ++i) {
            if (buffer[4 + i * 2 + 1] != 0) return false;
            const uint8_t c = buffer[4 + i * 2];
            if (c < 0x20 || c > 0x7E) return false;
        }
        return true;
    }
    if (text_length == 0 && test_length >= 8) {
        int32_t entries = 0;
        memcpy(&entries, buffer + 4, 4);
        return entries > 0 && entries < 50000000;
    }
    return false;
}

static bool ValidateIoStoreDirectory(const std::vector<uint8_t>& plain,
                                     const std::string& archive_name,
                                     DecryptedIndexSummary& summary) {
    ByteReader reader(plain.data(), plain.size());
    std::string mount;
    if (!reader.fstring(mount, 65535)) return false;
    const uint32_t directory_count = reader.u32();
    if (!reader.ok() || directory_count > 4'000'000) return false;
    if (!reader.skip(static_cast<size_t>(directory_count) * 16)) return false;
    const uint32_t file_count = reader.u32();
    if (!reader.ok() || file_count > 8'000'000) return false;
    if (!reader.skip(static_cast<size_t>(file_count) * 12)) return false;
    const uint32_t string_count = reader.u32();
    if (!reader.ok() || string_count > 8'000'000) return false;
    for (uint32_t i = 0; i < string_count; ++i) {
        std::string ignored;
        if (!reader.fstring(ignored, 65535)) return false;
    }
    if (!reader.ok()) return false;
    summary.archive_name = archive_name;
    summary.mount_point = mount;
    summary.num_entries = static_cast<int32_t>(file_count);
    summary.is_iostore = true;
    return true;
}

static void SummarizePakIndex(const std::vector<uint8_t>& plain,
                              const std::string& archive_name,
                              DecryptedIndexSummary& summary) {
    ByteReader reader(plain.data(), plain.size());
    std::string mount;
    if (!reader.fstring(mount, 65535)) return;
    summary.archive_name = archive_name;
    summary.mount_point = mount;
    summary.num_entries = reader.i32();
    summary.path_hash_seed = reader.u64();
    summary.has_path_hash_index = reader.u32() != 0;
    if (summary.has_path_hash_index) {
        reader.i64(); reader.i64(); reader.skip(20);
    }
    summary.has_full_directory_index = reader.u32() != 0;
    summary.is_iostore = false;
}

bool PakVerifier::VerifyKey(const uint8_t* key32, const PakVerifyContext& ctx,
                            DecryptedIndexSummary& out_summary) {
    if (!key32 || !ctx.ready || ctx.archives.empty()) return false;

    AesProvider& provider = SharedAesProvider();
    if (!provider.ok()) return false;

    const ArchiveIndexTest* preview = nullptr;
    for (const auto& test : ctx.archives) {
        if (test.encrypted_index.size() >= 16) { preview = &test; break; }
    }
    if (!preview) return false;

    const size_t preview_length = (std::min)(static_cast<size_t>(64),
                                            preview->encrypted_index.size() & ~static_cast<size_t>(15));
    std::vector<uint8_t> preview_plain;
    if (!provider.decrypt(key32, preview->encrypted_index.data(), preview_length, preview_plain))
        return false;
    if (!QuickCheckIndexHeader(preview_plain.data(), preview_plain.size(), preview->info.pak_version))
        return false;

    for (const auto& test : ctx.archives) {
        std::vector<uint8_t> plain;
        if (!provider.decrypt(key32, test.encrypted_index.data(), test.encrypted_index.size(), plain))
            continue;
        if (test.is_utoc) {
            DecryptedIndexSummary summary;
            if (ValidateIoStoreDirectory(plain, FileNameOf(test.info.pak_path), summary)) {
                out_summary = std::move(summary);
                return true;
            }
            continue;
        }
        if (!test.info.has_index_hash) continue;
        const std::string computed = Sha1Hex(plain.data(), plain.size());
        const std::string stored = HexUpper(test.info.index_hash, 20);
        if (computed != stored) continue;
        std::vector<uint8_t> parsed = plain;
        ApplyPakVariant(parsed, test.info.pak_version);
        SummarizePakIndex(parsed, FileNameOf(test.info.pak_path), out_summary);
        return true;
    }
    return false;
}

static std::string PakMethodName(const PakFooterInfo& footer, int index) {
    if (index == 0) return "None";
    const size_t slot = static_cast<size_t>(index - 1);
    if (slot < footer.compression_methods.size()) return footer.compression_methods[slot];
    return std::format("method#{}", index);
}

static bool DecodePakEntry(const std::vector<uint8_t>& encoded, int64_t location,
                           const PakFooterInfo& footer, PakSampleEntry& out) {
    if (location < 0 || static_cast<size_t>(location) >= encoded.size()) return false;
    ByteReader reader(encoded.data() + location, encoded.size() - static_cast<size_t>(location));
    const uint32_t value = reader.u32();
    uint32_t block_size = 0;
    if ((value & 0x3F) == 0x3F) block_size = reader.u32();
    else block_size = (value & 0x3F) << 11;

    const int32_t method = static_cast<int32_t>((value >> 23) & 0x3F);
    const bool encrypted = (value & (1u << 22)) != 0;
    const uint32_t block_count = (value >> 6) & 0xFFFF;
    const int64_t offset = (value & (1u << 31)) ? reader.u32() : reader.i64();
    const int64_t uncompressed = (value & (1u << 30)) ? reader.u32() : reader.i64();
    int64_t size = uncompressed;
    if (method != 0) size = (value & (1u << 29)) ? reader.u32() : reader.i64();
    if (!reader.ok()) return false;

    if (block_count == 1) block_size = static_cast<uint32_t>(uncompressed);
    else if (block_count == 0) block_size = 0;

    out.encoded_offset = location;
    out.offset = offset;
    out.size = size;
    out.uncompressed_size = uncompressed;
    out.compression_method_index = method;
    out.compression_method = PakMethodName(footer, method);
    out.encrypted = encrypted;
    out.compression_block_count = block_count;
    out.compression_block_size = block_size;
    return true;
}

static void AddFailure(ReportTotals& totals, const std::string& file, const std::string& message) {
    const std::string entry = file + ": " + message;
    if (std::find(totals.failures.begin(), totals.failures.end(), entry) == totals.failures.end())
        totals.failures.push_back(entry);
}

static bool VerifyPakIndexBlob(const std::string& path, const PakFooterInfo& footer,
                               const IndexReport& metadata, AesEcb* aes, IndexReport& out) {
    out = metadata;
    auto raw = ReadExact(path, metadata.offset, metadata.size);
    if (raw.size() != static_cast<size_t>(metadata.size)) {
        out.error = "short read";
        return false;
    }
    std::vector<uint8_t> plain;
    if (footer.encrypted_index) {
        if (!aes || !aes->decrypt(raw.data(), raw.size(), plain)) {
            out.error = "decrypt failed";
            return false;
        }
    } else {
        plain = std::move(raw);
    }
    out.computed_sha1 = Sha1Hex(plain.data(), plain.size());
    out.hash_verified = out.has_hash && out.computed_sha1 == out.stored_sha1;
    out.serialized_size = static_cast<int64_t>(plain.size());
    out.padding_bytes = 0;
    return true;
}

static bool ParsePakDirectoryIndex(const std::vector<uint8_t>& plain,
                                   const std::string& mount_point,
                                   const PakFooterInfo& footer,
                                   const std::vector<uint8_t>& encoded,
                                   const ReportOptions& options,
                                   DirectoryReport& out) {
    ByteReader reader(plain.data(), plain.size());
    const int32_t directory_count = reader.i32();
    if (!reader.ok() || directory_count < 0 || directory_count > 4'000'000) {
        out.error = "invalid directory count";
        return false;
    }
    out.directory_count = directory_count;
    std::map<std::string, int> extensions;
    std::vector<std::pair<std::string, int64_t>> candidates;

    for (int32_t d = 0; d < directory_count; ++d) {
        std::string directory;
        if (!reader.fstring(directory, 65535)) { out.error = "directory string"; return false; }
        const int32_t file_count = reader.i32();
        if (!reader.ok() || file_count < 0 || file_count > 4'000'000) {
            out.error = "invalid file count";
            return false;
        }
        out.file_count += file_count;
        for (int32_t f = 0; f < file_count; ++f) {
            std::string name;
            if (!reader.fstring(name, 65535)) { out.error = "file string"; return false; }
            const int32_t location = reader.i32();
            if (!reader.ok()) { out.error = "entry location"; return false; }
            const std::string full = JoinMount(mount_point, directory + name);
            const std::string ext = ExtensionOf(name);
            if (!ext.empty()) ++extensions[ext];
            if (static_cast<int>(out.sample_paths.size()) < options.max_samples)
                out.sample_paths.push_back(full);
            if (static_cast<int>(candidates.size()) < options.max_entries &&
                location >= 0 && static_cast<size_t>(location) < encoded.size()) {
                candidates.emplace_back(full, location);
            }
        }
    }
    out.serialized_size = static_cast<int64_t>(reader.tell());
    out.padding_bytes = static_cast<int64_t>(plain.size()) - out.serialized_size;
    out.extension_counts = std::move(extensions);
    for (const auto& [path, location] : candidates) {
        PakSampleEntry entry;
        if (DecodePakEntry(encoded, location, footer, entry)) {
            entry.path = path;
            out.sample_entries.push_back(std::move(entry));
        } else {
            PakSampleEntry failed;
            failed.path = path;
            failed.error = "decode failed";
            out.sample_entries.push_back(std::move(failed));
        }
    }
    return true;
}

static bool BuildPakReport(const std::string& path, const PakInfo& info,
                           AesEcb* aes,
                           const ReportOptions& options, ReportTotals& totals,
                           ArchiveReport& report) {
    report.kind = "pak";
    report.footer = info.footer;
    report.entry_count = 0;
    report.encrypted = info.is_encrypted;
    if (info.footer.has_stored_index_hash) ++totals.pak_primary_total;

    auto raw = ReadExact(path, info.footer.index_offset, info.footer.index_size);
    if (raw.size() != static_cast<size_t>(info.footer.index_size)) {
        report.error = "primary index short read";
        return false;
    }
    std::vector<uint8_t> plain;
    if (info.footer.encrypted_index) {
        if (!aes || !aes->decrypt(raw.data(), raw.size(), plain)) {
            report.error = "primary index decrypt failed";
            return false;
        }
    } else {
        plain = std::move(raw);
    }

    IndexReport& primary = report.primary_index;
    primary.present = true;
    primary.offset = static_cast<int64_t>(info.footer.index_offset);
    primary.size = static_cast<int64_t>(info.footer.index_size);
    primary.has_hash = info.footer.has_stored_index_hash;
    primary.encrypted = info.footer.encrypted_index;
    primary.stored_sha1 = HexUpper(info.footer.stored_index_hash, 20);
    primary.computed_sha1 = Sha1Hex(plain.data(), plain.size());
    primary.hash_verified = primary.has_hash && primary.computed_sha1 == primary.stored_sha1;

    if (primary.hash_verified) ++totals.pak_primary_verified;
    if (primary.has_hash && !primary.hash_verified) {
        primary.error = "primary index hash mismatch";
        AddFailure(totals, FileNameOf(path), primary.error);
        return false;
    }

    std::vector<uint8_t> parse_plain = plain;
    ApplyPakVariant(parse_plain, info.footer.version);
    ByteReader reader(parse_plain.data(), parse_plain.size());
    if (!reader.fstring(report.mount_point, 65535)) {
        report.error = "primary index mount point";
        return false;
    }
    report.entry_count = reader.i32();
    report.path_hash_seed = reader.u64();
    if (!reader.ok() || report.entry_count < 0) {
        report.error = "primary index header";
        return false;
    }

    report.has_path_hash_index = reader.u32() != 0;
    if (report.has_path_hash_index) {
        IndexReport meta;
        meta.present = true;
        meta.offset = reader.i64();
        meta.size = reader.i64();
        uint8_t hash[20] = {};
        if (!reader.raw(hash, 20)) {
            report.error = "index metadata";
            return false;
        }
        meta.stored_sha1 = HexUpper(hash, 20);
        meta.has_hash = false;
        for (uint8_t byte : hash) if (byte) { meta.has_hash = true; break; }
        meta.encrypted = info.footer.encrypted_index;
        report.path_hash_index = meta;
        IndexReport verified;
        VerifyPakIndexBlob(path, info.footer, meta, aes, verified);
        report.path_hash_index = verified;
        if (meta.has_hash) {
            ++totals.pak_secondary_total;
            if (verified.hash_verified) ++totals.pak_secondary_verified;
            else AddFailure(totals, FileNameOf(path), "path hash index hash mismatch");
        }
    }

    report.has_full_directory_index = reader.u32() != 0;
    IndexReport directory_meta;
    if (report.has_full_directory_index) {
        directory_meta.present = true;
        directory_meta.offset = reader.i64();
        directory_meta.size = reader.i64();
        uint8_t hash[20] = {};
        if (!reader.raw(hash, 20)) {
            report.error = "index metadata";
            return false;
        }
        directory_meta.stored_sha1 = HexUpper(hash, 20);
        directory_meta.has_hash = false;
        for (uint8_t byte : hash) if (byte) { directory_meta.has_hash = true; break; }
        directory_meta.encrypted = info.footer.encrypted_index;
        report.full_directory_index = directory_meta;
    }

    const uint32_t encoded_size = reader.u32();
    std::vector<uint8_t> encoded;
    if (encoded_size) {
        if (!reader.need(encoded_size)) { report.error = "encoded entries"; return false; }
        encoded.resize(encoded_size);
        if (!reader.raw(encoded.data(), encoded_size)) {
            report.error = "encoded entries";
            return false;
        }
    }
    report.encoded_entries_size = encoded_size;
    report.unencoded_entries = reader.i32();
    if (report.unencoded_entries < 0) { report.error = "unencoded count"; return false; }
    for (int32_t i = 0; i < report.unencoded_entries; ++i) {
        reader.i64(); reader.i64(); reader.i64();
        const int32_t method = reader.i32();
        if (!reader.skip(20) || !reader.ok()) {
            report.error = "unencoded entry";
            return false;
        }
        if (method != 0) {
            const int32_t blocks = reader.i32();
            if (blocks < 0 || !reader.skip(static_cast<size_t>(blocks) * 16)) {
                report.error = "unencoded entry";
                return false;
            }
        }
        reader.u8();
        reader.u32();
        if (!reader.ok()) {
            report.error = "unencoded entry";
            return false;
        }
    }
    primary.serialized_size = static_cast<int64_t>(reader.tell());
    primary.padding_bytes = static_cast<int64_t>(plain.size()) - primary.serialized_size;

    if (report.has_full_directory_index) {
        DirectoryReport directory;
        directory.present = true;
        directory.encrypted = info.footer.encrypted_index;
        directory.offset = directory_meta.offset;
        directory.size = directory_meta.size;
        directory.stored_sha1 = directory_meta.stored_sha1;

        auto dir_raw = ReadExact(path, directory_meta.offset, directory_meta.size);
        if (dir_raw.size() != static_cast<size_t>(directory_meta.size)) {
            directory.error = "short read";
        } else {
            std::vector<uint8_t> dir_plain;
            bool decrypted = false;
            if (info.footer.encrypted_index) {
                if (aes && aes->decrypt(dir_raw.data(), dir_raw.size(), dir_plain)) decrypted = true;
            } else {
                dir_plain = std::move(dir_raw);
                decrypted = true;
            }
            if (!decrypted) {
                directory.error = "decrypt failed";
            } else {
                directory.decrypted = true;
                directory.computed_sha1 = Sha1Hex(dir_plain.data(), dir_plain.size());
                directory.hash_verified = directory_meta.has_hash &&
                    directory.computed_sha1 == directory.stored_sha1;
                if (directory.hash_verified || !directory_meta.has_hash) {
                    std::vector<uint8_t> dir_parsed = dir_plain;
                    ApplyPakVariant(dir_parsed, info.footer.version);
                    if (!ParsePakDirectoryIndex(dir_parsed, report.mount_point, info.footer,
                                                encoded, options, directory) &&
                        directory.error.empty()) {
                        directory.error = "directory index parse failed";
                    }
                } else {
                    directory.error = "directory index hash mismatch";
                }
            }
        }
        report.full_directory_index.hash_verified = directory.hash_verified;
        report.full_directory_index.computed_sha1 = directory.computed_sha1;
        report.full_directory_index.serialized_size = directory.serialized_size;
        report.full_directory_index.padding_bytes = directory.padding_bytes;
        report.full_directory_index.error = directory.error;

        if (directory_meta.has_hash) {
            ++totals.pak_secondary_total;
            if (directory.hash_verified) ++totals.pak_secondary_verified;
            else AddFailure(totals, FileNameOf(path), "full directory index hash mismatch");
        }
        if (!directory.error.empty() &&
            (directory.hash_verified || !directory_meta.has_hash)) {
            AddFailure(totals, FileNameOf(path), directory.error);
        }

        report.directory = std::move(directory);
        totals.pak_entries += report.entry_count;
        totals.pak_indexed_files += report.directory.file_count;
        totals.sample_entries_decoded += static_cast<int>(report.directory.sample_entries.size());
    } else {
        totals.pak_entries += report.entry_count;
    }

    Log(std::format("[AESKeyFinder]   Report: {} mount=\"{}\" entries={} primaryHash={} dirFiles={} dirHash={}",
                    FileNameOf(path), report.mount_point, report.entry_count,
                    primary.hash_verified ? "OK" : "no",
                    report.directory.file_count,
                    report.directory.hash_verified ? "OK" : "no"));
    return true;
}

static void CollectChunkTypes(const std::string& path, IoStoreTocInfo& toc) {
    if (toc.entry_count == 0) return;
    const uint64_t count = static_cast<uint64_t>(toc.entry_count) * 12;
    if (toc.header_size > (128ull << 20) || count > (64ull << 20)) return;
    auto ids = ReadExact(path, toc.header_size, count);
    if (ids.size() != count) return;
    for (uint32_t i = 0; i < toc.entry_count; ++i) {
        const uint8_t type = ids[static_cast<size_t>(i) * 12 + 11];
        ++toc.chunk_type_counts[ChunkTypeName(type)];
    }
}

static bool ParseIoStoreDirectoryIndex(const std::vector<uint8_t>& plain,
                                       const ReportOptions& options,
                                       DirectoryReport& out) {
    ByteReader reader(plain.data(), plain.size());
    if (!reader.fstring(out.mount_point, 65535)) { out.error = "mount point"; return false; }

    struct DirEntry { uint32_t name, first_child, next_sibling, first_file; };
    struct FileEntry { uint32_t name, next_file, user_data; };

    const uint32_t directory_count = reader.u32();
    if (!reader.ok() || directory_count > 4'000'000) { out.error = "directory count"; return false; }
    std::vector<DirEntry> directories(directory_count);
    for (auto& directory : directories) {
        directory.name = reader.u32();
        directory.first_child = reader.u32();
        directory.next_sibling = reader.u32();
        directory.first_file = reader.u32();
    }
    const uint32_t file_count = reader.u32();
    if (!reader.ok() || file_count > 8'000'000) { out.error = "file count"; return false; }
    std::vector<FileEntry> files(file_count);
    for (auto& file : files) {
        file.name = reader.u32();
        file.next_file = reader.u32();
        file.user_data = reader.u32();
    }
    const uint32_t string_count = reader.u32();
    if (!reader.ok() || string_count > 8'000'000) { out.error = "string count"; return false; }
    std::vector<std::string> strings(string_count);
    for (auto& text : strings) {
        if (!reader.fstring(text, 65535)) { out.error = "string table"; return false; }
    }
    if (!reader.ok()) { out.error = "truncated"; return false; }

    out.directory_count = static_cast<int32_t>(directory_count);
    out.file_count = static_cast<int32_t>(file_count);
    out.string_count = static_cast<int32_t>(string_count);

    std::vector<uint8_t> visited(directory_count, 0);
    std::function<void(uint32_t, const std::string&, int)> walk =
        [&](uint32_t directory, const std::string& prefix, int depth) {
            if (depth > 128 || directory >= directories.size() || visited[directory]) return;
            visited[directory] = 1;
            const DirEntry& dir = directories[directory];
            uint32_t file_index = dir.first_file;
            size_t guard = 0;
            while (file_index != 0xFFFFFFFFu && file_index < files.size() && guard++ <= files.size()) {
                const FileEntry& file = files[file_index];
                const std::string& name = file.name < strings.size() ? strings[file.name] : std::string("?");
                const std::string full = JoinMount(out.mount_point, prefix + name);
                const std::string ext = ExtensionOf(name);
                if (!ext.empty()) ++out.extension_counts[ext];
                ++out.walked_file_count;
                if (static_cast<int>(out.sample_paths.size()) < options.max_samples) {
                    out.sample_paths.push_back(full);
                    out.sample_files.emplace_back(full, file.user_data);
                }
                file_index = file.next_file;
            }
            uint32_t child = dir.first_child;
            guard = 0;
            while (child != 0xFFFFFFFFu && child < directories.size() && guard++ <= directories.size()) {
                const std::string& name = directories[child].name < strings.size()
                    ? strings[directories[child].name] : std::string("?");
                walk(child, prefix + name + "/", depth + 1);
                child = directories[child].next_sibling;
            }
        };
    if (!directories.empty()) walk(0, "", 0);

    out.serialized_size = static_cast<int64_t>(reader.tell());
    out.padding_bytes = static_cast<int64_t>(plain.size()) - out.serialized_size;
    return true;
}

static bool BuildUtocReport(const std::string& path, const PakInfo& info,
                            AesEcb* aes,
                            const ReportOptions& options, ReportTotals& totals,
                            ArchiveReport& report) {
    report.kind = "iostore_toc";
    report.toc = info.toc;
    report.encrypted = info.is_encrypted;
    CollectChunkTypes(path, report.toc);
    totals.iostore_chunks += report.toc.entry_count;

    DirectoryReport directory;
    directory.present = (report.toc.flags & 8) && report.toc.dir_size > 0;
    directory.encrypted = (report.toc.flags & 2) != 0;
    directory.offset = report.toc.dir_offset;
    directory.size = report.toc.dir_size;
    if (directory.present) {
        ++totals.utoc_directory_total;
        auto raw = ReadExact(path, report.toc.dir_offset, report.toc.dir_size);
        if (raw.size() != report.toc.dir_size) {
            directory.error = "short read";
        } else {
            std::vector<uint8_t> plain;
            bool decrypted = false;
            if (directory.encrypted) {
                if (aes && aes->decrypt(raw.data(), raw.size(), plain)) decrypted = true;
            } else {
                plain = std::move(raw);
                decrypted = true;
            }
            if (!decrypted) {
                directory.error = "decrypt failed";
            } else {
                directory.decrypted = true;
                if (ParseIoStoreDirectoryIndex(plain, options, directory)) {
                    ++totals.utoc_directory_parsed;
                } else if (directory.error.empty()) {
                    directory.error = "parse failed";
                }
            }
        }
        totals.iostore_indexed_files += directory.file_count;
        Log(std::format("[AESKeyFinder]   Report: {} TOCv{} flags=0x{:02X} chunks={} mount=\"{}\" files={} dir={}",
                        FileNameOf(path), report.toc.version, report.toc.flags,
                        report.toc.entry_count, directory.mount_point,
                        directory.file_count,
                        directory.decrypted ? (directory.error.empty() ? "OK" : directory.error) : "encrypted/no key"));
    } else {
        Log(std::format("[AESKeyFinder]   Report: {} TOCv{} flags=0x{:02X} chunks={} (no directory index)",
                        FileNameOf(path), report.toc.version, report.toc.flags,
                        report.toc.entry_count));
    }
    report.utoc_directory = std::move(directory);
    if (report.utoc_directory.present && !report.utoc_directory.error.empty())
        AddFailure(totals, FileNameOf(path), report.utoc_directory.error);
    return true;
}

bool PakVerifier::BuildArchiveReports(const uint8_t* key32,
                                      const std::vector<PakInfo>& archives,
                                      const ReportOptions& options,
                                      std::vector<ArchiveReport>& out_reports,
                                      ReportTotals& out_totals) {
    out_reports.clear();
    out_totals = ReportTotals{};
    if (archives.empty()) {
        out_totals.all_checks_passed = true;
        return false;
    }

    const auto start = std::chrono::steady_clock::now();
    AesEcb aes;
    const bool aes_ready = key32 && aes.init(key32);

    std::map<std::string, const PakInfo*> utoc_by_stem;
    std::map<std::string, const PakInfo*> ucas_by_stem;
    for (const auto& info : archives) {
        if (info.is_utoc) utoc_by_stem[StemKey(info.pak_path)] = &info;
        else if (info.is_ucas) ucas_by_stem[StemKey(info.pak_path)] = &info;
    }

    Log(std::format("[AESKeyFinder] Building archive report for {} containers...", archives.size()));

    for (const auto& info : archives) {
        ArchiveReport report;
        report.file_name = FileNameOf(info.pak_path);
        report.file_size = info.file_size;
        report.encrypted = info.is_encrypted;

        if (info.is_ucas) {
            report.kind = "iostore_container";
            const auto paired = utoc_by_stem.find(StemKey(info.pak_path));
            if (paired != utoc_by_stem.end()) {
                report.paired_path = paired->second->pak_path;
                report.chunk_data_encrypted = paired->second->is_encrypted;
            } else {
                report.paired_path = info.paired_path;
                report.chunk_data_encrypted = info.is_encrypted;
            }
            ++out_totals.ucas;
            out_totals.ucas_bytes += info.file_size;
            if (report.chunk_data_encrypted) ++out_totals.encrypted_ucas;
        } else if (info.is_utoc) {
            if (const auto it = ucas_by_stem.find(StemKey(info.pak_path)); it != ucas_by_stem.end())
                report.paired_path = it->second->pak_path;
            report.chunk_data_encrypted = info.is_encrypted;
            ++out_totals.utocs;
            out_totals.utoc_bytes += info.file_size;
            if (info.is_encrypted) ++out_totals.encrypted_utocs;
            BuildUtocReport(info.pak_path, info, aes_ready ? &aes : nullptr,
                            options, out_totals, report);
        } else {
            if (const auto it = utoc_by_stem.find(StemKey(info.pak_path)); it != utoc_by_stem.end())
                report.paired_path = it->second->pak_path;
            ++out_totals.paks;
            out_totals.pak_bytes += info.file_size;
            if (info.is_encrypted) ++out_totals.encrypted_paks;
            BuildPakReport(info.pak_path, info, aes_ready ? &aes : nullptr,
                           options, out_totals, report);
        }
        if (!report.error.empty())
            AddFailure(out_totals, report.file_name, report.error);
        out_reports.push_back(std::move(report));
    }

    out_totals.containers_total = static_cast<int>(out_reports.size());
    out_totals.iostore_non_file_chunks =
        std::max<int64_t>(0, out_totals.iostore_chunks - out_totals.iostore_indexed_files);
    out_totals.all_checks_passed = out_totals.failures.empty();

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    Log(std::format("[AESKeyFinder] Archive report built in {} ms: {} pak ({} entries), {} utoc ({} chunks, {} files), {} ucas; failures={}",
                    elapsed, out_totals.paks, out_totals.pak_entries,
                    out_totals.utocs, out_totals.iostore_chunks, out_totals.iostore_indexed_files,
                    out_totals.ucas, out_totals.failures.size()));
    return true;
}

}
