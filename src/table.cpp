#include "table.hpp"

void StatsTable::addHeader() {
    addSeparator();
    std::string name_header = "Method name";
    name_header.resize(name_width, ' ');
    content << "| "
            << name_header << " | "
            << std::setw(cell_width) << "min ms" << " | "
            << std::setw(cell_width) << "avg ms" << " | "
            << std::setw(cell_width) << "max ms" << " |\n";
    addBoldSeparator();
}

void StatsTable::addEntry(std::string method_name, double min_time, double avg_time, double max_time) {
    method_name.resize(name_width, ' ');
    content << "| "
            << method_name << " | "
            << std::right << std::setw(cell_width) << std::fixed << std::setprecision(precision) << min_time << " | " 
            << std::right << std::setw(cell_width) << std::fixed << std::setprecision(precision) << avg_time << " | "
            << std::right << std::setw(cell_width) << std::fixed << std::setprecision(precision) << max_time << " |\n";
    addSeparator();
}

void StatsTable::dump() {
    std::string str = content.str();
    std::cout << str;
}

void StatsTable::dumpAndFlush() {
    std::string str = content.str();
    std::cout << str;
    content = std::stringstream();
}


void StatsTable::addSeparator() {
    std::string cell_sep = std::string(cell_width, '-');
    std::string name_sep = std::string(name_width, '-');
    content << "+-" + name_sep + "-+-" + cell_sep + "-+-" + cell_sep + "-+-" + cell_sep + "-+\n";
}

void StatsTable::addBoldSeparator() {
    std::string cell_sep = std::string(cell_width, '=');
    std::string name_sep = std::string(name_width, '=');
    content << "+=" + name_sep + "=+=" + cell_sep + "=+=" + cell_sep + "=+=" + cell_sep + "=+\n";
}
