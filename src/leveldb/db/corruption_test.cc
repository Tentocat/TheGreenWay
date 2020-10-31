// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/db.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "leveldb/cache.h"
#include "leveldb/env.h"
#include "leveldb/table.h"
#include "leveldb/write_batch.h"
#include "db/db_impl.h"
#include "db/filename.h"
#include "db/log_format.h"
#include "db/version_set.h"
#include "util/logging.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace leveldb {

static const int kValueSize = 1000;

class CorruptionTest {
 public:
  test::ErrorEnv env_;
  std::string dbname_;
  Cache* tiny_cache_;
  Options options_;
  DB* db_;

  CorruptionTest() {
    tiny_cache_ = NewLRUCache(100);
    options_.env = &env_;
    options_.block_cache = tiny_cache_;
    dbname_ = test::TmpDir() + "/corruption_test";
    DestroyDB(dbname_, options_);

    db_ = NULL;
    options_.create_if_missing = true;
    Reopen();
    options_.create_if_missing = false;
  }

  ~CorruptionTest() {
     delete db_;
     DestroyDB(dbname_, Options());
     delete tiny_cache_;
  }

  Status TryReopen() {
    delete db_;
    db_ = NULL;
    return DB::Open(options_, dbname_, &db_);
  }

  void Reopen() {
    ASSERT_OK(TryReopen());
  }

  void RepairDB() {
    delete db_;
    db_ = NULL;
    ASSERT_OK(::leveldb::RepairDB(dbname_, options_));
  }

  void Build(int n) {
    std::string key_space, value_space;
    WriteBatch batch;
    for (int i = 0; i < n; i++) {
      //if ((i % 100) == 0) fprintf(stderr, "@ %d of %d\n", i, n);
      Slice key = Key(i, &key_space);
      batch.Clear();
      batch.Put(key, Value(i, &value_space));
      WriteOptions options;
      // Corrupt() doesn't work without this sync on windows; stat reports 0 for
      // the file size.
      if (i == n - 1) {
        options.sync = true;
      }
      ASSERT_OK(db_->Write(options, &batch));
    }
  }

  void Check(int min_expected, int max_expected) {
    int next_expected = 0;
    int missed = 0;
    int bad_keys = 0;
    int bad_values = 0;
    int correct = 0;
    std::string value_space;
    Iterator* iter = db_->NewIterator(ReadOptions());
    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
      uint64_t key;
      Slice in(iter->key());
      if (in == "" || in == "~") {
        // Ignore boundary keys.
        continue;
      }
      if (!ConsumeDecimalNumber(&in, &key) ||
          !in.empty() ||
          key < next_expected) {
        bad_keys++;
        continue;
      }
      missed += (key - next_expected);
      next_expected = key + 1;
      if (iter->value() != Value(key, &value_space)) {
        bad_values++;
      } else {
        correct++;
      }
    }
    delete iter;

    fprintf(stderr,
            "expected=%d..%d; got=%d; bad_keys=%d; bad_values=%d; missed=%d\n",
            min_expected, max_expected, correct, bad_keys, bad_values, missed);
    ASSERT_LE(min_expected, correct);
    ASSERT_GE(max_expected, correct);
  }

  void Corrupt(FileType filetype, int offset, int bytes_to_corrupt) {
    // Pick file to corrupt
    std::vector<std::string> filenames;
    ASSERT_OK(env_.GetChildren(dbname_, &filenames));
    uint64_t number;
    FileType type;
    std::string fname;
    int picked_number = -1;
    for (size_t i = 0; i < filenames.size(); i++) {
      if (ParseFileName(filenames[i], &number, &type) &&
          type == filetype &&
          int(number) > picked_number) {  // Pick latest file
        fname = dbname_ + "/" + filenames[i];
        picked_number = number;
      }
    }
    ASSERT_TRUE(!fname.empty()) << filetype;

    struct stat sbuf;
    if (stat(fname.c_str(), &sbuf) != 0) {
      const char* msg = strerror(errno);
      ASSERT_TRUE(false) << fname << ": " << msg;
    }

    if (offset < 0) {
      // Relative to end of file; make it absolute
      if (-offset > sbuf.st_size) {
        offset = 0;
      } else {
        offset = sbuf.st_size + offset;
      }
    }
    if (offset > sbuf.st_size) {
      offset = sbuf.st_size;
    }
    if (offset + bytes_to_corrupt > sbuf.st_size) {
      bytes_to_corrupt = sbuf.st_size - offset;
    }

    // Do it
    std::string contents;
    Status s = ReadFileToString(Env::Default(), fname, &contents);
    ASSERT_TRUE(s.ok()) << s.ToString();
    for (int i = 0; i < bytes_to_corrupt; i++) {
      contents[i + offset] ^= 0x80;
    }
    s = WriteStringToFile(Env::Default(), contents, fname);
    ASSERT_TRUE(s.ok()) << s.ToString();
  }

  int Property(const std::string& name) {
    std::string property;
    int result;
    if (db_->GetProperty(name, &property) &&
        sscanf(property.c_str(), "%d", &result) == 1) {
      return result;
    } else {
      return -1;
    }
  }

  // Return the ith key
  Slice Key(int i, std::string* storage) {
    char buf[100];
    snprintf(buf, sizeof(buf), "%016d", i);
    storage->assign(buf, strlen(buf));
    return Slice(*storage);
  }

  // Return th