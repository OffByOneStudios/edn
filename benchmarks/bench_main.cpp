#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/type_check.hpp"
#include <chrono>
#include <iostream>
#include <vector>
#include <cstdlib>

using Clock = std::chrono::steady_clock;

static edn::node_ptr parse(const std::string &s){
    return edn::parse(s);
}

struct RunResult { double ms_emit; size_t ir_bytes; };

static RunResult bench_case(const char* name, const std::string &program){
    edn::TypeContext tctx;
    edn::IREmitter emitter(tctx);
    edn::TypeCheckResult tc;

    auto t0 = Clock::now();
    auto mod = emitter.emit(parse(program), tc);
    auto t1 = Clock::now();
    if(!mod || !tc.success){
        std::cerr << "[bench] case '" << name << "' failed typecheck or emission\n";
        return {0.0, 0};
    }
    std::string s;
    llvm::raw_string_ostream os(s);
    mod->print(os, nullptr);
    os.flush();
    auto t2 = Clock::now();
    double ms_emit = std::chrono::duration<double, std::milli>(t1 - t0).count();
    (void)t2; // if needed for future
    return { ms_emit, s.size() };
}

int main(){
    // Ensure deterministic: disable passes unless explicitly enabled by env
#ifdef _WIN32
    _putenv_s("EDN_ENABLE_PASSES", getenv("EDN_ENABLE_PASSES") ? getenv("EDN_ENABLE_PASSES") : "0");
#else
    setenv("EDN_ENABLE_PASSES", getenv("EDN_ENABLE_PASSES") ? getenv("EDN_ENABLE_PASSES") : "0", 1);
#endif

    struct Case { const char* name; std::string prog; };
    std::vector<Case> cases;

    // Case 1: trait object dictionary call via vtable-like indirection (simplified)
    cases.push_back({
        "trait_call",
        "(module :id \"m1\"\n"
        "  (sum :name Result :variants [ (variant :name Ok :fields [ i32 ]) (variant :name Err :fields [ i32 ]) ])\n"
        "  (struct :name S :fields [ (field :name x :type i32) ])\n"
        "  (fn :name \"callee\" :ret i32 :params [ (param i32 %env) (param i32 %x) ] :body [ (add %y i32 %x %x) (ret i32 %y) ])\n"
        "  (fn :name \"main\" :ret i32 :params [] :body [\n"
        "    (const %e (ptr i32) 0)\n"
        "    (make-closure %c callee [ %e ])\n"
        "    (call-closure %r i32 %c )\n"
        "    (ret i32 %r) ])\n"
        ")"
    });

    // Case 2: sum/match with binds
    cases.push_back({
        "sum_match",
        "(module :id \"m2\"\n"
        "  (sum :name Maybe :variants [ (variant :name None) (variant :name Some :fields [ i32 ]) ])\n"
        "  (fn :name \"main\" :ret i32 :params [] :body [\n"
        "    (const %z i32 0)\n"
        "    (sum-new %m Maybe Some [ %z ])\n"
        "    (match %out i32 Maybe %m :cases [\n"
        "      (case None [ (const %a i32 1) ] :value %a)\n"
        "      (case Some :binds [ (bind %v 0) ] :body [ (add %b i32 %v %v) ] :value %b)\n"
        "    ] :default [ (const %d i32 7) ] )\n"
        "    (ret i32 %out) ])\n"
        ")"
    });

    // Case 3: small loop + arithmetic
    cases.push_back({
        "loop_arith",
        "(module :id \"m3\"\n"
        "  (fn :name \"main\" :ret i32 :params [] :body [\n"
        "    (const %i i32 0) (const %one i32 1) (const %n i32 1000)\n"
        "    (while %cond [ (lt %cond i1 %i %n) (add %i i32 %i %one) ])\n"
        "    (ret i32 %i) ])\n"
        ")"
    });

    std::cout << "name,ms_emit,ir_bytes\n";
    for(const auto &c : cases){
        auto r = bench_case(c.name, c.prog);
        std::cout << c.name << "," << r.ms_emit << "," << r.ir_bytes << "\n";
    }
    return 0;
}
