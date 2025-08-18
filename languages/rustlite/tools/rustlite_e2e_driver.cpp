#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/traits.hpp"
#include "rustlite/expand.hpp"
#include "../parser/parser.hpp"

static std::string read_all(std::istream& is){ std::ostringstream ss; ss << is.rdbuf(); return ss.str(); }

int main(int argc, char** argv){
    using namespace edn;
    try{
        if(argc < 2){
            std::cerr << "usage: rustlite_e2e_driver <input-file> [--dump]\n";
            return 2;
        }
        std::string path = argv[1];
        bool dump=false; for(int i=2;i<argc;++i){ if(std::string(argv[i])=="--dump") dump=true; }
        std::ifstream f(path, std::ios::binary);
        if(!f){ std::cerr << "e2e: cannot open '" << path << "'\n"; return 2; }
        auto src = read_all(f);

        // 1) Parse Rustlite source to EDN
        rustlite::Parser p;
        auto pres = p.parse_string(src, path);
        if(!pres.success){
            std::cerr << path << ":" << pres.line << ":" << pres.column << ": parse error: " << pres.error_message << "\n";
            return 1;
        }
        if(dump){ std::cout << "=== Frontend EDN ===\n" << pres.edn << "\n"; }

        // 2) Parse EDN AST
        auto ast = parse(pres.edn);

        // 3) Expand Rustlite and traits
    auto expanded = expand_traits(rustlite::expand_rustlite(ast));
    if(dump){ std::cout << "=== Expanded EDN ===\n"; /* TODO: add pretty-printer if needed */ }

        // 4) Typecheck
        TypeContext tctx; TypeChecker tc(tctx);
        auto tcres = tc.check_module(expanded);
        if(!tcres.success){
            std::cerr << "typecheck failed\n";
            for(const auto& e : tcres.errors){
                std::cerr << e.code << ": " << e.message << "\n";
                for(const auto& n : e.notes){ std::cerr << "  note: " << n.message << "\n"; }
            }
            for(const auto& w : tcres.warnings){ std::cerr << "warn " << w.code << ": " << w.message << "\n"; }
            return 3;
        }

        // 5) Emit IR
        IREmitter em(tctx); TypeCheckResult irres; auto *mod = em.emit(expanded, irres);
        if(!mod || !irres.success){ std::cerr << "ir emission failed\n"; return 4; }
        if(dump){ mod->print(llvm::outs(), nullptr); std::cout << "\n"; }
        return 0;
    } catch(const std::exception& e){
        std::cerr << "e2e: exception: " << e.what() << "\n"; return 5;
    }
}
