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
  std::cerr << query << std::endl;
  return query;
}

static std::string table_insert_statement(const viaems::FeedUpdate& update) {
  std::string query;
  query += "INSERT INTO points (realtime_ns,";
  int remaining = update.size() - 1;
  for (const auto& x : update) {
    query += "\"" + x.first + "\" ";
    if (remaining) {
      query += ",";
    }
    remaining -= 1;
  }
  uint64_t ns =
  std::chrono::duration_cast<std::chrono::nanoseconds>(update.time.time_since_epoch()).count();
  query += ") VALUES (" + std::to_string(ns) + ",";

  remaining = update.size() - 1;
  for (const auto& x : update) {
    query += 
    std::visit([](const auto &x) -> std::string { return std::to_string(x); }, x.second);
    if (remaining) {
      query += ",";
    }
    remaining -= 1;
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
  
  for (const auto& k : update) {
    keys.insert(k.first);
  }

  res = sqlite3_exec(db, "CREATE INDEX points_time ON points(realtime_ns);", NULL, 0,
      &sqlerr);
  if (res) {
    std::cerr << "Log: unable to create log index: " << sqlerr << std::endl;
    sqlite3_free(sqlerr);
    return;
  }
}


void Log::Update(std::vector<viaems::FeedUpdate> list) {
  const auto &first = list.at(0);
  ensure_db_schema(first);

  char *sqlerr;
  auto query = table_insert_statement(first);
  int res = sqlite3_exec(db, query.c_str(), NULL, 0, &sqlerr);
  if (res) {
    std::cerr << "Log: unable to insert log entry: " << sqlerr << std::endl;
    sqlite3_free(sqlerr);
    return;
  }
}

void Log::SetOutputFile(std::string path) {
  int r = sqlite3_open(path.c_str(), &db);
  if (r) {
    db = nullptr;
    return;
  }
}

