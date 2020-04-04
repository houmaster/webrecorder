#pragma once

#include "common.h"
#include <functional>
#include <set>
#include <deque>
#include <filesystem>
#include <thread>
#include <mutex>
#include <condition_variable>

class LossyCompressor;

class ArchiveReader final {
public:
  ArchiveReader() = default;
  ArchiveReader(const ArchiveReader&) = delete;
  ArchiveReader& operator=(const ArchiveReader&) = delete;
  ~ArchiveReader();

  bool open(const std::filesystem::path& filename);
  void close();
  time_t get_modification_time(const std::string& filename) const;
  ByteVector read(const std::string& filename) const;
  void for_each_file(const std::function<void(std::string)>& callback) const;

private:
  void* acquire_context() const;
  void return_context(void* context) const;

  std::filesystem::path m_filename;
  mutable std::mutex m_mutex;
  mutable std::vector<void*> m_unzip_contexts;
};

//-------------------------------------------------------------------------

class ArchiveWriter final {
public:
  ArchiveWriter();
  ArchiveWriter(const ArchiveWriter&) = delete;
  ArchiveWriter& operator=(const ArchiveWriter&) = delete;
  ~ArchiveWriter();

  void set_lossy_compressor(std::unique_ptr<LossyCompressor> lossy_compressor);
  bool open(std::filesystem::path filename);
  void move_on_close(std::filesystem::path filename, bool overwrite);
  bool close();
  bool write(const std::string& filename, ByteView data,
    time_t modification_time = 0, bool allow_lossy_compression = false);
  void async_write(const std::string& filename, ByteView data,
    time_t modification_time, bool allow_lossy_compression,
    std::function<void(bool)>&& on_complete);
  bool contains(const std::string& filename) const;
  void async_read(const std::string& filename,
    std::function<void(ByteVector, time_t)>&& on_complete);

private:
  bool update_filenames(const std::string& filename);
  bool reopen(bool for_reading);
  void do_close();
  bool do_write(const std::string& filename, ByteView data,
    time_t modification_time, bool allow_lossy_compression);
  std::pair<ByteVector, time_t> do_read(const std::string& filename);

  void insert_task(std::function<void()>&& task);
  void start_thread();
  void finish_thread();
  void thread_func();

  std::filesystem::path m_filename;
  std::filesystem::path m_move_on_close;
  bool m_overwrite{ };
  std::set<std::string> m_filenames;

  std::mutex m_zip_mutex;
  std::unique_ptr<LossyCompressor> m_lossy_compressor;
  void* m_zip{ };
  bool m_reading{ };

  std::mutex m_tasks_mutex;
  std::condition_variable m_tasks_signal;
  std::deque<std::function<void()>> m_tasks;
  bool m_finish_thread{ };
  std::thread m_thread;
};
