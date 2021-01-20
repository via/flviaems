#include <set>

#include <sqlite3.h>

#include "Log.h"

Log::Log() {}

static std::string table_schema_from_update(const viaems::FeedUpdate& update) {
  std::string query;

  int remaining = update.size() - 1;

  query += "CREATE TABLE points (realtime_ns INTEGER,";
  for (const auto& x : update) {
    query += "\"" + x.first + "\" ";
    if (std::holds_alternative<uint32_t>(x.second)) {
      query += "INTEGER";
    } else {
      query += "REAL";
    }
    if (remaining) {
      query += ", ";
    } else {
      query += ");";
    }
    remaining -= 1;
  }
  return query;
}

static std::string table_insert_query(const std::set<std::string> keys) {
  std::string query;
  query += "INSERT INTO points (realtime_ns,";
  int remaining = keys.size() - 1;
  for (const auto& x : keys) {
    query += "\"" + x + "\" ";
    if (remaining) {
      query += ",";
    }
    remaining -= 1;
  }

  query += ") VALUES (?, ";

  for (remaining = keys.size(); remaining; remaining--) {
    query += "? ";
    if (remaining > 1) {
      query += ",";
    }
  }

  query += ");";
  return query;
}

static bool points_schema_matches_update(std::set<std::string> keys,
viaems::FeedUpdate update) {
  for (const auto &k : update) {
    if (keys.count(k.first) == 0) {
      return false;
    }
  }
  return true;
}

void Log::ensure_db_schema(const viaems::FeedUpdate &update) {
  if (points_schema_matches_update(keys, update)) {
    return;
  }

  /* TODO if table already exists, alter instead */
  char *sqlerr;
  auto create_stmt = table_schema_from_update(update);
  int res = sqlite3_exec(db, create_stmt.c_str(), NULL, 0, &sqlerr);
  if (res) {
    std::cerr << "Log: unable to initialize log schema: " << sqlerr << std::endl;
    sqlite3_free(sqlerr);
    return;
  }
  
  res = sqlite3_exec(db, "CREATE INDEX points_time ON points(realtime_ns);", NULL, 0,
      &sqlerr);
  if (res) {
    std::cerr << "Log: unable to create log index: " << sqlerr << std::endl;
    sqlite3_free(sqlerr);
    return;
  }

  for (const auto& k : update) {
    keys.insert(k.first);
  }

  std::string query = table_insert_query(keys);
  res = sqlite3_prepare_v2(db, query.c_str(), query.size(), &insert_stmt, NULL);
  if (res) {
    std::cerr << "Log: unable to prepare insert statement: " << sqlerr << std::endl;
    sqlite3_free(sqlerr);
    return;
  }
}

void Log::Update(const std::vector<viaems::FeedUpdate>& list) {
  const auto &first = list.at(0);
  ensure_db_schema(first);

  if (!in_transaction) {
    sqlite3_exec(db, "BEGIN;", NULL, 0, NULL);
    in_transaction = true;
  }


  for (const auto& update : list) {
    sqlite3_reset(insert_stmt);
    /* timestamp */
    uint64_t time_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(update.time.time_since_epoch()).count();
    sqlite3_bind_int64(insert_stmt, 1, time_ns);
    int sql_index = 2;
    for (const auto &key : keys) {
      const auto& value = update.at(key);
      if (std::holds_alternative<uint32_t>(value)) {
        sqlite3_bind_int64(insert_stmt, sql_index, std::get<uint32_t>(value));
      } else {
        sqlite3_bind_double(insert_stmt, sql_index, std::get<float>(value));
      }
      sql_index += 1;
    }

    int res = sqlite3_step(insert_stmt);
    if (res != SQLITE_DONE) {
      std::cerr << "Log: unable to insert log entry: " << sqlite3_errmsg(db) << std::endl;
      return;
    }
  }

  cur_transaction_size += list.size();
  if (cur_transaction_size > max_transaction_size) {
    sqlite3_exec(db, "COMMIT;", NULL, 0, NULL);
    std::cerr << "Wrote batch: " << cur_transaction_size << std::endl;
    in_transaction = false;
    cur_transaction_size = 0;
  }

}

void Log::SetOutputFile(std::string path) {
  int r = sqlite3_open(path.c_str(), &db);
  if (r) {
    db = nullptr;
    return;
  }
}

static std::string table_search_statement(std::vector<std::string> keys) {
  std::string query = "SELECT realtime_ns";
  for (auto k : keys) {
    query += ",\"" + k + "\"";
  }

  query += " FROM points WHERE realtime_ns > ? AND realtime_ns < ?";
  return query;
}

std::vector<viaems::FeedUpdate> Log::GetRange(std::vector<std::string> keys, std::chrono::system_clock::time_point start,
std::chrono::system_clock::time_point stop) {
  if (db == nullptr) {
    return {};
  }
  uint64_t start_ns =
  std::chrono::duration_cast<std::chrono::nanoseconds>(start.time_since_epoch()).count();

  uint64_t stop_ns =
  std::chrono::duration_cast<std::chrono::nanoseconds>(stop.time_since_epoch()).count();

  auto q = table_search_statement(keys);
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, q.c_str(), q.size(), &stmt, NULL);

  sqlite3_bind_int64(stmt, 1, start_ns);
  sqlite3_bind_int64(stmt, 2, stop_ns);

  int res;
  std::vector<viaems::FeedUpdate> retval;
  while (true) {
    res = sqlite3_step(stmt);
    if ((res == SQLITE_DONE) || (res == SQLITE_MISUSE)) {
      break;
    }

    viaems::FeedUpdate line;
    uint64_t ts = sqlite3_column_int64(stmt, 0);
    auto since_epoch = std::chrono::nanoseconds{ts};
    line.time = std::chrono::system_clock::time_point{since_epoch};

    for (int i = 1; i < sqlite3_column_count(stmt); i++) {
      auto coltype = sqlite3_column_type(stmt, i);
      switch (coltype) {
        case SQLITE_INTEGER:
          line.insert(std::make_pair(keys[i - 1],
          (uint32_t)sqlite3_column_int64(stmt, i)));
          break;
        case SQLITE_FLOAT:
          line.insert(std::make_pair(keys[i - 1],
          (float)sqlite3_column_double(stmt, i)));
          break;
        default:
        break;
      }
    }
    retval.push_back(line);
  }
  return retval;
}


