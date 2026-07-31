#pragma once
#include <string>
#include <fstream>
#include <vector>
#include "MarketStats.hpp"

class CsvWriter {
public:
    explicit CsvWriter(const std::string& path);
    ~CsvWriter();
    
    // Write header row
    void write_header();
    
    // Write a single snapshot row
    void write_snapshot(const MarketSnapshot& snap);
    
    // Write all snapshots at once
    void write_snapshots(const std::vector<MarketSnapshot>& snapshots);
    
    bool is_open() const { return file_.is_open(); }
    
private:
    std::ofstream file_;
};