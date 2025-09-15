#include "include/CommandIntrospection.h"
#include "include/CommandRegistry.h"
#include <sstream>
using namespace lfc;

std::string BuildIntrospectionJson(const CommandRegistry& reg) {
    auto list = reg.list();
    std::ostringstream os; os << "{\"methods\":[";
    for (size_t i=0;i<list.size();++i) {
        if (i) os<<",";
        os << "{\"name\":\"" << list[i].name << "\",\"help\":\"";
        for (char ch: list[i].help) { if (ch=='"'||ch=='\\') os<<'\\'<<ch; else if (ch=='\n') os<<"\\n"; else os<<ch; }
        os << "\"}";
    }
    os << "]}";
    return os.str();
}
