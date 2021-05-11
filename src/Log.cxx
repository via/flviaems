#include <iostream>
#include <set>

#include <sqlite3.h>

#include "Log.h"

Log::Log() {}

static std::string points_table_schema_from_update(const viaems::LogChunk &update) {
  std::string query;

  int index = 0;

  query += "CREATE TABLE points (realtime_ns INTEGER,";
  for (const auto &x : update.keys) {
    query += "\"" + x + "\" ";
    if (std::holds_alternative<uint32_t>(update.points[0].values[index])) {
      query += "INTEGER";
    } else {
      query += "REAL";
    }
    if (index < update.keys.size() - 1) {
      query += ", ";
    } else {
      query += ");";
    }
    index += 1;
  }
  return query;
}

static std::string points_table_insert_query(const std::vector<std::string> keys) {
  std::string query;
  query += "INSERT INTO points (realtime_ns,";
  int remaining = keys.size() - 1;
  for (const auto &x : keys) {
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

static bool points_schema_matches_update(std::vector<std::string> keys,
                                         viaems::LogChunk update) {
  if (update.keys.size() != keys.size()) {
    return false;
  }
  for (int i = 0; i < keys.size(); i++) {
    if (update.keys[i] != keys[i]) {
      return false;
    }
  }
  return true;
}

static std::vector<std::string> current_points_keys(sqlite3 *db) {
  sqlite3_stmt *stmt;
  std::vector<std::string> keys;

  std::string query = "PRAGMA TABLE_INFO(points);";
  sqlite3_prepare_v2(db, query.c_str(), query.size(), &stmt, NULL);
  while (true) {
    int res = sqlite3_step(stmt);
    if ((res == SQLITE_DONE) || (res == SQLITE_MISUSE)) {
      break;
    }
    auto *col_name =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    keys.push_back(std::string{col_name});
  }
  sqlite3_finalize(stmt);
  return keys;
}

static void ensure_db_schema(sqlite3 *db, const viaems::LogChunk &update) {

  auto table_keys = current_points_keys(db);

  if (table_keys.size() == 0) {
    /* Create a new table */
    int res;
    char *sqlerr;
    auto create_stmt = points_table_schema_from_update(update);
    res = sqlite3_exec(db, create_stmt.c_str(), NULL, 0, &sqlerr);
    if (res) {
      std::cerr << "Log: unable to initialize log schema: " << sqlerr
                << std::endl;
      sqlite3_free(sqlerr);
      return;
    }

    res = sqlite3_exec(db, "CREATE INDEX points_time ON points(realtime_ns);",
                       NULL, 0, &sqlerr);
    if (res) {
      std::cerr << "Log: unable to create log index: " << sqlerr << std::endl;
      sqlite3_free(sqlerr);
    }
  } else if (false) {
    /* Alter the existing table to have the additional cols */
  }
}

void Log::WriteChunk(viaems::LogChunk &&update) {
  if (!db) {
    return;
  }

  ensure_db_schema(this->db, update);

  auto query = points_table_insert_query(update.keys);
  sqlite3_stmt *insert_stmt;
  int res =
      sqlite3_prepare_v2(db, query.c_str(), query.size(), &insert_stmt, NULL);
  if (res != SQLITE_OK) {
    std::cerr << "Log: unable to prepare insert statement: "
              << sqlite3_errmsg(db) << std::endl;
    return;
  }

  sqlite3_exec(db, "BEGIN;", NULL, 0, NULL);

  for (const auto &point : update.points) {
    sqlite3_reset(insert_stmt);
    /* timestamp */
    uint64_t time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           point.time.time_since_epoch())
                           .count();
    sqlite3_bind_int64(insert_stmt, 1, time_ns);
    int sql_index = 2;
    for (const auto &key : update.keys) {
      int value_index = sql_index - 2;
      const auto &value = point.values[value_index];
      if (std::holds_alternative<uint32_t>(value)) {
        sqlite3_bind_int64(insert_stmt, sql_index, std::get<uint32_t>(value));
      } else {
        sqlite3_bind_double(insert_stmt, sql_index, std::get<float>(value));
      }
      sql_index += 1;
    }

    int res = sqlite3_step(insert_stmt);
    if (res != SQLITE_DONE) {
      std::cerr << "Log: unable to insert log entry: " << sqlite3_errmsg(db)
                << std::endl;
      return;
    }
  }

  sqlite3_exec(db, "COMMIT;", NULL, 0, NULL);
  sqlite3_finalize(insert_stmt);
}

void Log::SetFile(std::string path) {
  int r = sqlite3_open(path.c_str(), &db);
  if (r) {
    db = nullptr;
    return;
  }

  /* Enable WAL mode */
  char *sqlerr;
  int res =
      sqlite3_exec(db, "PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL;",
                   NULL, 0, &sqlerr);
  if (res) {
    std::cerr << "Log: unable to set WAL mode: " << sqlerr << std::endl;
    sqlite3_free(sqlerr);
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

viaems::LogChunk Log::GetRange(std::vector<std::string> keys,
                               std::chrono::system_clock::time_point start,
                               std::chrono::system_clock::time_point stop) {
  if (db == nullptr) {
    return {};
  }
  uint64_t start_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          start.time_since_epoch())
                          .count();

  uint64_t stop_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         stop.time_since_epoch())
                         .count();

  auto q = table_search_statement(keys);
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, q.c_str(), q.size(), &stmt, NULL);

  sqlite3_bind_int64(stmt, 1, start_ns);
  sqlite3_bind_int64(stmt, 2, stop_ns);

  int res;
  viaems::LogChunk retval;
  retval.keys = keys;
  while (true) {
    res = sqlite3_step(stmt);
    if ((res == SQLITE_DONE) || (res == SQLITE_MISUSE)) {
      break;
    }

    viaems::LogPoint point;
    uint64_t ts = sqlite3_column_int64(stmt, 0);
    auto since_epoch = std::chrono::nanoseconds{ts};
    point.time = std::chrono::system_clock::time_point{since_epoch};

    for (int i = 1; i < sqlite3_column_count(stmt); i++) {
      auto coltype = sqlite3_column_type(stmt, i);
      switch (coltype) {
      case SQLITE_INTEGER:
        point.values.push_back((uint32_t)sqlite3_column_int64(stmt, i));
        break;
      case SQLITE_FLOAT:
        point.values.push_back((float)sqlite3_column_double(stmt, i));
        break;
      default:
        break;
      }
    }
    retval.points.emplace_back(point);
  }
  sqlite3_finalize(stmt);
  return retval;
}

std::chrono::system_clock::time_point Log::EndTime() {
  std::string query = "SELECT realtime_ns FROM points ORDER BY realtime_ns DESC LIMIT 1";

  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, query.c_str(), query.size(), &stmt, NULL);
  int res = sqlite3_step(stmt);
  if (res == SQLITE_DONE) {
    return std::chrono::system_clock::now();
  }
  auto ns = sqlite3_column_int64(stmt, 0);
  sqlite3_finalize(stmt);
  return std::chrono::system_clock::time_point{std::chrono::nanoseconds{ns}};
}


static void ensure_configs_table(sqlite3 *db) {
  std::string create_table_str = "CREATE TABLE configs (time INTEGER, config TEXT);";

  int res;
  char *sqlerr;
  res = sqlite3_exec(db, create_table_str.c_str(), NULL, 0, &sqlerr);
  if (res) {
    std::cerr << "Log: unable to create configs table" << sqlerr
      << std::endl;
    sqlite3_free(sqlerr);
    return;
  }
}

void Log::SaveConfig(viaems::Configuration conf) {
  ensure_configs_table(db);

  std::string insert_query_str = "INSERT INTO configs VALUES(?, ?);";
  sqlite3_stmt *insert_stmt;
  int res = sqlite3_prepare_v2(db, insert_query_str.c_str(),
  insert_query_str.size(), &insert_stmt, NULL);

  if (res != SQLITE_OK) {
    std::cerr << "Log: unable to prepare insert statement: "
              << sqlite3_errmsg(db) << std::endl;
    return;
  }

  sqlite3_reset(insert_stmt);
  uint64_t time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
      conf.save_time.time_since_epoch())
    .count();
  sqlite3_bind_int64(insert_stmt, 1, time_ns);

  std::string conf_dump = conf.to_json().dump();
  sqlite3_bind_text(insert_stmt, 2, conf_dump.c_str(), conf_dump.size(), SQLITE_STATIC);

  res = sqlite3_step(insert_stmt);
  if (res != SQLITE_DONE) {
    std::cerr << "Log: unable to save config: " << sqlite3_errmsg(db) << std::endl;
    return;
  }
  sqlite3_finalize(insert_stmt);
}

std::vector<viaems::Configuration> Log::LoadConfigs() {
  ensure_configs_table(db);

  std::string select_query_str = "SELECT time, config FROM configs ORDER BY time DESC LIMIT 1";
  sqlite3_stmt *select_stmt;
  int res = sqlite3_prepare_v2(db, select_query_str.c_str(),
  select_query_str.size(), &select_stmt, NULL);

  if (res != SQLITE_OK) {
    std::cerr << "Log: unable to prepare config query statement: "
              << sqlite3_errmsg(db) << std::endl;
    return {};
  }

  sqlite3_reset(select_stmt);
  res = sqlite3_step(select_stmt);
  if (res != SQLITE_ROW) {
    std::cerr << "Log: unable to get config: " << sqlite3_errmsg(db) << std::endl;
    return {};
  }

  auto ns = sqlite3_column_int64(select_stmt, 0);
  auto time_ns = std::chrono::system_clock::time_point{std::chrono::nanoseconds{ns}};
  auto resp = viaems::Configuration{.save_time = time_ns};

  auto config_json = json::parse(sqlite3_column_text(select_stmt, 1));

  resp.from_json(config_json);
  sqlite3_finalize(select_stmt);
  return {resp};
}

void ThreadedWriteLog::WriteChunk(viaems::LogChunk &&chunk) {
  std::unique_lock<std::mutex> lock(mutex);
  chunks.emplace_back(chunk);
  cv.notify_one();
}

void ThreadedWriteLog::write_loop() {
  while (true) {
    std::unique_lock<std::mutex> lock(mutex);
    if (chunks.empty()) {
      cv.wait(lock);
    }

    if (!running) {
      return;
    }

    auto first = std::move(chunks.front());
    chunks.erase(chunks.begin());

    lock.unlock();

    Log::WriteChunk(std::move(first));
  }
}
