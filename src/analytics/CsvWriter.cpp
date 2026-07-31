#include "analytics/CsvWriter.hpp"
#include <iomanip>

CsvWriter::CsvWriter(const std::string& path) {
    file_.open(path);
}

CsvWriter::~CsvWriter() {
    if (file_.is_open()) file_.close();
}

void CsvWriter::write_header() {
    file_ << "block_number,token_id,best_bid,best_ask,mid_price,spread,"
          << "bid_depth_1pct,ask_depth_1pct,total_volume,trade_count,vwap\n";
}

void CsvWriter::write_snapshot(const MarketSnapshot& snap) {
    file_ << snap.block_number << ","
          << snap.token_id << ","
          << snap.best_bid << ","
          << snap.best_ask << ","
          << snap.mid_price << ","
          << snap.spread << ","
          << snap.bid_depth_1pct << ","
          << snap.ask_depth_1pct << ","
          << snap.total_volume << ","
          << snap.trade_count << ","
          << std::fixed << std::setprecision(2) << snap.vwap << "\n";
}

void CsvWriter::write_snapshots(const std::vector<MarketSnapshot>& snapshots) {
    write_header();
    for (const auto& snap : snapshots) {
        write_snapshot(snap);
    }
}