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

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Support/TargetSelect.h>
// Verify and print LLVM IR if needed
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <thread>
#include <condition_variable>
#include <chrono>
#include <mutex>

static std::string read_all(std::istream& is){ std::ostringstream ss; ss << is.rdbuf(); return ss.str(); }

int main(int argc, char** argv){
    using namespace edn;
    try{
        if(argc < 2){
            std::cerr << "usage: rustlite_jit_driver <input-file> [entry] [--debug]\n";
            std::cerr << "  input-file : path to Rustlite source file\n";
            std::cerr << "  entry      : optional entry function name (default: main)\n";
            std::cerr << "  --debug    : print source, frontend EDN, lowered core EDN, and JIT result\n";
            return 2;
        }
        std::string path = argv[1];
        std::string entry = "main";
        bool debug = false;
        for(int i=2; i<argc; ++i){
            std::string arg = argv[i];
            if(arg == "--debug" || arg == "-d") debug = true;
            else if(!arg.empty() && arg[0] != '-' && entry == "main") entry = arg;
        }
        std::ifstream f(path, std::ios::binary);
        if(!f){ std::cerr << "jit: cannot open '" << path << "'\n"; return 2; }
        auto src = read_all(f);
        if(debug){
            std::cout << "=== Rust Source ===\n" << src << "\n";
        }

        // Parse Rustlite -> EDN
        rustlite::Parser p;
        auto pres = p.parse_string(src, path);
        if(!pres.success){ std::cerr << path << ":" << pres.line << ":" << pres.column << ": parse error: " << pres.error_message << "\n"; return 1; }
    if(debug){ std::cout << "=== Frontend EDN ===\n" << pres.edn << "\n"; }

        // Parse EDN AST
        auto ast = parse(pres.edn);

        // Expand and typecheck
        auto expanded = expand_traits(rustlite::expand_rustlite(ast));
    if(debug){ std::cout << "=== Lowered Core EDN ===\n" << to_pretty_string(expanded, 2) << "\n"; }
    TypeContext tctx; TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
        if(!tcres.success){ std::cerr << "typecheck failed\n"; return 3; }

        // Emit IR
    IREmitter em(tctx); TypeCheckResult irres; auto *mod = em.emit(expanded, irres);
        if(!mod || !irres.success){ std::cerr << "ir emission failed\n"; return 4; }

        // Optional: dump IR when debugging
        if(debug){
            std::string irStr; llvm::raw_string_ostream rso(irStr); mod->print(rso, nullptr); rso.flush();
            std::cout << "=== LLVM IR ===\n" << irStr << std::endl;
        }

        // Verify IR before JIT to catch malformed control flow, missing terminators, etc.
        std::string verifyErr;
        llvm::raw_string_ostream verr(verifyErr);
        if(llvm::verifyModule(*mod, &verr)){
            verr.flush();
            std::cerr << "IR verification failed:\n" << verifyErr << std::endl;
            // Also print the IR to aid debugging if not already printed
            if(!debug){ std::string irStr; llvm::raw_string_ostream rso(irStr); mod->print(rso, nullptr); rso.flush(); std::cerr << irStr << std::endl; }
            return 8;
        }

        // JIT and execute entry
        llvm::InitializeNativeTarget(); llvm::InitializeNativeTargetAsmPrinter();
        auto jitExp = llvm::orc::LLJITBuilder().create(); if(!jitExp){ std::cerr << "Failed to create JIT\n"; return 5; }
        auto jit = std::move(*jitExp);
        jit->getMainJITDylib().addGenerator(llvm::cantFail(
            llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
                jit->getDataLayout().getGlobalPrefix())));
        if(auto err = jit->addIRModule(em.toThreadSafeModule())){ std::cerr << "Failed to add module\n"; return 6; }
        auto sym = jit->lookup(entry); if(!sym){ std::cerr << "Entry function not found: " << entry << "\n"; return 7; }
        using FnTy = int(*)(void); auto fn = reinterpret_cast<FnTy>(sym->toPtr<void*>());
        // Watchdog: 10-second timeout to avoid hangs during iteration
        std::mutex mtx; std::condition_variable cv; bool done=false; int result=0;
        std::thread worker([&]{
            int r = fn();
            {
                std::lock_guard<std::mutex> lk(mtx);
                result = r;
                done = true;
            }
            cv.notify_one();
        });
        {
            std::unique_lock<std::mutex> lk(mtx);
            if(!cv.wait_for(lk, std::chrono::seconds(10), [&]{ return done; })){
                std::cerr << "Hang detected: code example hung > 10s; aborting." << std::endl;
                if(worker.joinable()) worker.detach();
                return 124;
            }
        }
        if(worker.joinable()) worker.join();
        if(debug) std::cout << "=== JIT Result ===\n";
        std::cout << "Result: " << result << "\n";
        return 0;
    } catch(const std::exception& e){ std::cerr << "jit: exception: " << e.what() << "\n"; return 9; }
}
