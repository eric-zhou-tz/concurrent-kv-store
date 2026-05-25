#include "persistence/binary_io.h"
#include "persistence/wal.h"

#include <cstdint>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <utility>

namespace kv {
namespace persistence {

namespace {

using LengthType = std::uint32_t;
using ChecksumType = std::uint32_t;
using SizeType = binary_io::SizeType;
using OpType = std::uint8_t;

// Bound individual records so corrupt lengths cannot force unbounded memory
// allocation during replay.
constexpr std::size_t kMaxRecordLength = 64U * 1024U * 1024U;

enum class WalOp : OpType {
  Set = 1,
  Delete = 2,
};

struct ParsedRecord {
  WalOp op = WalOp::Set;
  std::string key;
  std::string value;
};

template <typename T>
void append_primitive(std::string& bytes, T value) {
  const char* raw = reinterpret_cast<const char*>(&value);
  bytes.append(raw, sizeof(T));
}

std::uint32_t crc32(const std::string& bytes) {
  std::uint32_t crc = 0xFFFFFFFFU;
  for (const unsigned char byte : bytes) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
      const std::uint32_t mask = 0U - (crc & 1U);
      crc = (crc >> 1U) ^ (0xEDB88320U & mask);
    }
  }
  return ~crc;
}

LengthType checked_record_length(std::size_t record_length) {
  // Keep writer and reader limits aligned: if we will not replay it, do not
  // write it.
  if (record_length > kMaxRecordLength) {
    throw std::runtime_error("WAL record is too large");
  }
  return static_cast<LengthType>(record_length);
}

bool read_exact(std::ifstream& input, char* data, std::size_t size) {
  input.read(data, static_cast<std::streamsize>(size));
  return static_cast<bool>(input);
}

template <typename T>
bool read_framing_primitive(std::ifstream& input,
                            std::uint64_t& cursor,
                            T& value,
                            WalReplayResult& result) {
  input.read(reinterpret_cast<char*>(&value), sizeof(value));
  const std::streamsize bytes_read = input.gcount();
  if (bytes_read == 0 && input.eof()) {
    result.status = WalReplayStatus::CleanEof;
    result.stop_offset = cursor;
    return false;
  }

  if (bytes_read != static_cast<std::streamsize>(sizeof(value))) {
    result.status = WalReplayStatus::PartialRecord;
    result.stop_offset = cursor;
    return false;
  }

  cursor += sizeof(value);
  return true;
}

WalReplayStatus parse_record(const std::string& record,
                             ParsedRecord& parsed) {
  std::size_t offset = 0;

  // All record variants start with an opcode and a key.

  // Operation
  OpType op = 0;
  if (!binary_io::ConsumePrimitive(record, offset, op)) {
    return WalReplayStatus::PartialPayload;
  }

  if (op != static_cast<OpType>(WalOp::Set) &&
      op != static_cast<OpType>(WalOp::Delete)) {
    return WalReplayStatus::InvalidOpcode;
  }

  // Key Size
  SizeType key_size = 0;
  if (!binary_io::ConsumePrimitive(record, offset, key_size)) {
    return WalReplayStatus::PartialPayload;
  }

  // Key itself
  std::string key;
  if (!binary_io::ConsumeBytes(record, offset, key_size, key)) {
    return WalReplayStatus::PartialPayload;
  }

  if (op == static_cast<OpType>(WalOp::Set)) {
    // SET records carry exactly one value after the key. Extra trailing bytes
    // make the record malformed.
    SizeType value_size = 0;
    if (!binary_io::ConsumePrimitive(record, offset, value_size)) {
      return WalReplayStatus::PartialPayload;
    }

    std::string value;
    if (!binary_io::ConsumeBytes(record, offset, value_size, value)) {
      return WalReplayStatus::PartialPayload;
    }

    if (offset != record.size()) {
      return WalReplayStatus::InvalidPayload;
    }

    parsed.op = WalOp::Set;
    parsed.key = std::move(key);
    parsed.value = std::move(value);
    return WalReplayStatus::CleanEof;
  }

  // DELETE records end immediately after the key.
  if (offset != record.size()) {
    return WalReplayStatus::InvalidPayload;
  }

  parsed.op = WalOp::Delete;
  parsed.key = std::move(key);
  return WalReplayStatus::CleanEof;
}

void apply_record(const ParsedRecord& record,
                  std::unordered_map<std::string, std::string>& store) {
  if (record.op == WalOp::Set) {
    store[record.key] = record.value;
    return;
  }

  store.erase(record.key);
}

}  // namespace

WriteAheadLog::WriteAheadLog(std::string path)
    : path_(std::move(path)),
      output_(path_, std::ios::binary | std::ios::app) {
  if (!output_.is_open()) {
    throw std::runtime_error("failed to open WAL file: " + path_);
  }
}

void WriteAheadLog::AppendSet(const std::string& key, const std::string& value) {
  const OpType op = static_cast<OpType>(WalOp::Set);
  const SizeType key_size = binary_io::CheckedSize(key, "WAL key");
  const SizeType value_size = binary_io::CheckedSize(value, "WAL value");
  // Length covers the payload after the length field itself:
  // [op][key_size][key][value_size][value].
  const LengthType record_length =
      checked_record_length(sizeof(op) + sizeof(key_size) + key_size +
                            sizeof(value_size) + value_size);

  std::string payload;
  append_primitive(payload, op);
  append_primitive(payload, key_size);
  payload.append(key);
  append_primitive(payload, value_size);
  payload.append(value);
  const ChecksumType checksum = crc32(payload);

  binary_io::WritePrimitive(output_, record_length, "WAL primitive");
  binary_io::WritePrimitive(output_, checksum, "WAL primitive");
  binary_io::WriteBytes(output_, payload, "WAL bytes");

  output_.flush();
  if (!output_) {
    throw std::runtime_error("failed to write WAL SET record");
  }
}

void WriteAheadLog::AppendDelete(const std::string& key) {
  const OpType op = static_cast<OpType>(WalOp::Delete);
  const SizeType key_size = binary_io::CheckedSize(key, "WAL key");
  // Length covers the payload after the length field itself:
  // [op][key_size][key].
  const LengthType record_length =
      checked_record_length(sizeof(op) + sizeof(key_size) + key_size);

  std::string payload;
  append_primitive(payload, op);
  append_primitive(payload, key_size);
  payload.append(key);
  const ChecksumType checksum = crc32(payload);

  binary_io::WritePrimitive(output_, record_length, "WAL primitive");
  binary_io::WritePrimitive(output_, checksum, "WAL primitive");
  binary_io::WriteBytes(output_, payload, "WAL bytes");

  output_.flush();
  if (!output_) {
    throw std::runtime_error("failed to write WAL DELETE record");
  }
}

std::uint64_t WriteAheadLog::CurrentOffset() {
  output_.flush();
  if (!output_) {
    throw std::runtime_error("failed to flush WAL before reading offset");
  }

  output_.seekp(0, std::ios::end);
  if (!output_) {
    throw std::runtime_error("failed to seek WAL output stream");
  }

  const std::streampos position = output_.tellp();
  if (position == std::streampos(-1)) {
    throw std::runtime_error("failed to read WAL offset");
  }

  return static_cast<std::uint64_t>(position);
}

void WriteAheadLog::Clear() {
  // The WAL keeps an append stream open for normal writes. Close and reopen it
  // around truncation so future SET/DELETE records continue using the same WAL
  // object after persistence has been cleared.
  output_.close();
  output_.clear();

  {
    std::ofstream truncated(path_, std::ios::binary | std::ios::trunc);
    if (!truncated.is_open()) {
      throw std::runtime_error("failed to truncate WAL file: " + path_);
    }

    truncated.flush();
    if (!truncated) {
      throw std::runtime_error("failed to clear WAL file: " + path_);
    }
  }

  output_.open(path_, std::ios::binary | std::ios::app);
  if (!output_.is_open()) {
    throw std::runtime_error("failed to reopen WAL file: " + path_);
  }
}

void WriteAheadLog::TruncateTo(std::uint64_t offset) {
  output_.close();
  output_.clear();

  std::error_code error;
  const std::uintmax_t file_size = std::filesystem::exists(path_, error)
                                       ? std::filesystem::file_size(path_, error)
                                       : 0;
  if (error) {
    throw std::runtime_error("failed to inspect WAL file before truncation: " +
                             path_);
  }
  if (offset > file_size) {
    throw std::runtime_error("WAL truncation offset is past EOF");
  }

  std::filesystem::resize_file(path_, offset, error);
  if (error) {
    throw std::runtime_error("failed to truncate WAL file: " + path_);
  }

  output_.open(path_, std::ios::binary | std::ios::app);
  if (!output_.is_open()) {
    throw std::runtime_error("failed to reopen WAL file: " + path_);
  }
}

std::size_t WriteAheadLog::Replay(
    std::unordered_map<std::string, std::string>& store) const {
  return ReplayFrom(0, store);
}

WalReplayResult WriteAheadLog::ReplayDetailed(
    std::unordered_map<std::string, std::string>& store) const {
  return ReplayFromDetailed(0, store);
}

std::size_t WriteAheadLog::ReplayFrom(
    std::uint64_t offset,
    std::unordered_map<std::string, std::string>& store) const {
  return ReplayFromDetailed(offset, store).applied_operations;
}

WalReplayResult WriteAheadLog::ReplayFromDetailed(
    std::uint64_t offset,
    std::unordered_map<std::string, std::string>& store) const {
  if (offset > static_cast<std::uint64_t>(
                   std::numeric_limits<std::streamoff>::max())) {
    throw std::runtime_error("WAL replay offset is too large");
  }

  WalReplayResult result;
  result.start_offset = offset;
  result.last_good_offset = offset;
  result.stop_offset = offset;

  std::ifstream input(path_, std::ios::binary);
  if (!input.is_open()) {
    return result;
  }

  input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
  if (!input) {
    return result;
  }

  std::uint64_t cursor = offset;

  while (true) {
    // Each iteration validates one WAL v2 frame:
    // [uint32 payload_length][uint32 crc32(payload)][payload].
    // A record mutates the store only after the full payload parses and its
    // checksum matches.
    const std::uint64_t record_start = cursor;
    LengthType record_length = 0;
    if (!read_framing_primitive(input, cursor, record_length, result)) {
      break;
    }

    if (record_length == 0 || record_length > kMaxRecordLength) {
      result.status = WalReplayStatus::InvalidLength;
      result.stop_offset = record_start;
      break;
    }

    ChecksumType expected_checksum = 0;
    if (!read_framing_primitive(input, cursor, expected_checksum, result)) {
      result.status = WalReplayStatus::PartialRecord;
      result.stop_offset = record_start;
      break;
    }

    std::string record(record_length, '\0');
    if (!read_exact(input, record.data(), record.size())) {
      result.status = WalReplayStatus::PartialRecord;
      result.stop_offset = record_start;
      break;
    }
    cursor += record.size();

    if (crc32(record) != expected_checksum) {
      result.status = WalReplayStatus::ChecksumMismatch;
      result.stop_offset = record_start;
      break;
    }

    ParsedRecord parsed;
    const WalReplayStatus parse_status = parse_record(record, parsed);
    if (parse_status != WalReplayStatus::CleanEof) {
      result.status = parse_status;
      result.stop_offset = record_start;
      break;
    }

    apply_record(parsed, store);
    ++result.applied_operations;
    result.last_good_offset = cursor;
    result.stop_offset = cursor;
  }

  return result;
}

WalReplayResult WriteAheadLog::ReplayFromAndTruncate(
    std::uint64_t offset,
    std::unordered_map<std::string, std::string>& store) {
  WalReplayResult result = ReplayFromDetailed(offset, store);
  if (result.status != WalReplayStatus::CleanEof) {
    TruncateTo(result.last_good_offset);
    result.truncated = true;
  }
  return result;
}

}  // namespace persistence
}  // namespace kv
