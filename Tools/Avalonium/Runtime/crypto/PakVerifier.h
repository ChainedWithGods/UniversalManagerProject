#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace Crypto {

struct PakFooterInfo {
    bool valid = false;
    uint64_t offset = 0;
    uint32_t size = 0;
    bool ends_at_eof = false;
    uint32_t magic = 0;
    int32_t version = 0;
    std::string version_name;
    uint64_t index_offset = 0;
    uint64_t index_size = 0;
    bool encrypted_index = false;
    uint8_t encryption_key_guid[16] = {};
    uint8_t stored_index_hash[20] = {};
    bool has_stored_index_hash = false;
    std::vector<std::string> compression_methods;
};

struct IoStoreTocInfo {
    bool valid = false;
    int32_t version = 0;
    std::string version_name;
    uint32_t header_size = 0;
    uint32_t entry_count = 0;
    uint32_t block_count = 0;
    uint32_t block_entry_size = 0;
    uint32_t method_count = 0;
    uint32_t method_length = 0;
    uint32_t block_size = 0;
    uint32_t dir_size = 0;
    uint64_t dir_offset = 0;
    uint32_t partition_count = 0;
    uint64_t partition_size = 0;
    uint64_t container_id = 0;
    uint8_t encryption_key_guid[16] = {};
    uint8_t flags = 0;
    uint32_t seed_count = 0;
    uint32_t without_hash_count = 0;
    uint32_t signature_bytes = 0;
    uint64_t meta_bytes = 0;
    bool meta_consistent = false;
    std::vector<std::string> methods;
    std::map<std::string, int> chunk_type_counts;
};

struct PakInfo {
    bool found_pak      = false;
    bool is_encrypted   = false;
    bool is_utoc        = false;
    bool is_ucas        = false;
    int  pak_version    = 0;
    uint8_t encryption_key_guid[16] = {};
    uint8_t index_hash[20]          = {};
    bool has_index_hash             = false;
    std::string pak_path;
    std::string paired_path;
    uint64_t file_size    = 0;
    uint64_t index_offset = 0;
    uint64_t index_size   = 0;
    int total_pak_files     = 0;
    int encrypted_pak_files = 0;
    int total_utoc_files     = 0;
    int encrypted_utoc_files = 0;
    int total_ucas_files     = 0;
    int encrypted_ucas_files = 0;

    std::vector<std::string> compression_methods;
    bool has_relative_chunk_offsets = false;
    bool is_frozen_index            = false;

    PakFooterInfo footer;
    IoStoreTocInfo toc;
};

struct DecryptedIndexSummary {
    std::string archive_name;
    std::string mount_point;
    int32_t num_entries = 0;
    uint64_t path_hash_seed = 0;
    bool has_path_hash_index = false;
    bool has_full_directory_index = false;
    bool is_iostore = false;
};

struct ArchiveIndexTest {
    PakInfo info;
    std::vector<uint8_t> encrypted_index;
    size_t valid_read_bytes = 0;
    bool is_utoc = false;
};

struct PakVerifyContext {
    PakInfo primary_info;
    std::vector<ArchiveIndexTest> archives;
    std::vector<PakInfo> all_discovered_paks;
    bool ready = false;
};

struct PakSampleEntry {
    std::string path;
    int64_t encoded_offset = 0;
    int64_t offset = 0;
    int64_t size = 0;
    int64_t uncompressed_size = 0;
    int32_t compression_method_index = 0;
    std::string compression_method;
    bool encrypted = false;
    uint32_t compression_block_count = 0;
    uint32_t compression_block_size = 0;
    std::string error;
};

struct IndexReport {
    bool present = false;
    int64_t offset = 0;
    int64_t size = 0;
    int64_t serialized_size = 0;
    int64_t padding_bytes = 0;
    bool has_hash = false;
    bool encrypted = false;
    bool hash_verified = false;
    std::string stored_sha1;
    std::string computed_sha1;
    std::string error;
};

struct DirectoryReport {
    bool present = false;
    bool encrypted = false;
    bool decrypted = false;
    bool hash_verified = false;
    int64_t offset = 0;
    int64_t size = 0;
    int64_t serialized_size = 0;
    int64_t padding_bytes = 0;
    std::string stored_sha1;
    std::string computed_sha1;
    std::string error;
    std::string mount_point;
    int32_t directory_count = 0;
    int32_t file_count = 0;
    int32_t string_count = 0;
    int32_t walked_file_count = 0;
    std::map<std::string, int> extension_counts;
    std::vector<std::string> sample_paths;
    std::vector<PakSampleEntry> sample_entries;
    std::vector<std::pair<std::string, uint32_t>> sample_files;
};

struct ArchiveReport {
    std::string file_name;
    std::string kind;
    uint64_t file_size = 0;
    bool encrypted = false;
    std::string error;

    PakFooterInfo footer;
    IndexReport primary_index;
    std::string mount_point;
    int32_t entry_count = 0;
    uint64_t path_hash_seed = 0;
    bool has_path_hash_index = false;
    bool has_full_directory_index = false;
    IndexReport path_hash_index;
    IndexReport full_directory_index;
    int64_t encoded_entries_size = 0;
    int32_t unencoded_entries = 0;
    DirectoryReport directory;

    IoStoreTocInfo toc;
    DirectoryReport utoc_directory;

    std::string paired_path;
    bool chunk_data_encrypted = false;
};

struct ReportTotals {
    int containers_total = 0;
    int paks = 0;
    int utocs = 0;
    int ucas = 0;
    int encrypted_paks = 0;
    int encrypted_utocs = 0;
    int encrypted_ucas = 0;

    int64_t pak_entries = 0;
    int64_t pak_indexed_files = 0;
    int64_t iostore_chunks = 0;
    int64_t iostore_indexed_files = 0;
    int64_t iostore_non_file_chunks = 0;

    uint64_t pak_bytes = 0;
    uint64_t utoc_bytes = 0;
    uint64_t ucas_bytes = 0;

    int pak_primary_total = 0;
    int pak_primary_verified = 0;
    int pak_secondary_total = 0;
    int pak_secondary_verified = 0;
    int utoc_directory_total = 0;
    int utoc_directory_parsed = 0;
    int sample_entries_decoded = 0;

    bool all_checks_passed = false;
    std::vector<std::string> failures;
};

struct ReportOptions {
    int max_samples = 20;
    int max_entries = 8;
    int max_extensions = 32;
};

class PakVerifier {
public:
    static PakInfo CheckEncryption(const std::string& exe_path = "");
    static PakVerifyContext BuildVerifyContext(const PakInfo& info, const std::string& exe_path = "");
    static bool DecryptAES256ECB(const uint8_t* ciphertext, size_t len,
                                 const uint8_t* key32, uint8_t* plaintext);
    static bool VerifyKey(const uint8_t* key32, const PakVerifyContext& ctx,
                          DecryptedIndexSummary& out_summary);

    static bool BuildArchiveReports(const uint8_t* key32,
                                    const std::vector<PakInfo>& archives,
                                    const ReportOptions& options,
                                    std::vector<ArchiveReport>& out_reports,
                                    ReportTotals& out_totals);

    static std::vector<std::string> FindPakFiles(const std::string& exe_path);
    static PakInfo ParsePakFile(const std::string& pak_path);
    static PakInfo ParseUtocFile(const std::string& utoc_path);
    static PakInfo ParseUcasFile(const std::string& ucas_path);
};

}
