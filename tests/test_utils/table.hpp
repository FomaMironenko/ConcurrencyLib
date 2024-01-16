#pragma once

#include <iostream>
#include <iomanip>
#include <sstream>


class StatsTable {
public:
    StatsTable(int cell_width, int precision) : cell_width(cell_width), precision(precision) {   }

    void addHeader();
    void addEntry(std::string method_name, double min_time, double avg_time, double max_time);
    void dump();
    void dumpAndFlush();

private:
    void addSeparator();
    void addBoldSeparator();

private:
    const int name_width = 20;
    int cell_width;
    int precision;
    std::stringstream content;
};
