#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "../parser/parser.hpp"

static std::string read_all(std::istream& is){
    std::ostringstream ss; ss << is.rdbuf(); return ss.str();
}

int main(int argc, char** argv){
    try{
        if(argc < 2){
            std::cerr << "usage: rustlitec <input-file>\n";
            return 2;
        }
        const std::string path = argv[1];
        std::ifstream f(path, std::ios::binary);
        if(!f){ std::cerr << "rustlitec: cannot open '" << path << "'\n"; return 2; }
        const auto src = read_all(f);

        rustlite::Parser p;
        auto res = p.parse_string(src, path);
        if(!res.success){
            std::cerr << path << ":" << res.line << ":" << res.column << ": parse error: " << res.error_message << "\n";
            return 1;
        }
        std::cout << res.edn << "\n";
        return 0;
    } catch(const std::exception& e){
        std::cerr << "rustlitec: exception: " << e.what() << "\n";
        return 1;
    }
}
