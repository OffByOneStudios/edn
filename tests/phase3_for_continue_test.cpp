// Phase 3 for/continue feature tests
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include <iostream>
#include <cassert>

using namespace edn;

// Helper to build simple AST nodes (use internal factory)
static node_ptr sym(const std::string& s){ return detail::make_node(symbol{s}); }
static node_ptr kw(const std::string& s){ return detail::make_node(keyword{s}); }
static node_ptr vec(const std::vector<node_ptr>& e){ return detail::make_node(vector_t{e}); }
static node_ptr listn(const std::vector<node_ptr>& e){ return detail::make_node(list{e}); }
static node_ptr i64(int64_t v){ return detail::make_node(v); }
static node_ptr make_type(const std::string& s){ return sym(s); }

static node_ptr simple_fn_for_loop(){
    std::vector<node_ptr> body;
    body.push_back( listn({ sym("const"), sym("%c"), make_type("i1"), i64(1) }) );
    body.push_back( listn({ sym("for"), kw("init"), vec({ listn({ sym("const"), sym("%i"), make_type("i32"), i64(0) }) }), kw("cond"), sym("%c"), kw("step"), vec({ listn({ sym("const"), sym("%i2"), make_type("i32"), i64(0) }) }), kw("body"), vec({ listn({ sym("break") }) }) }) );
    body.push_back( listn({ sym("const"), sym("%z"), make_type("i32"), i64(0) }) );
    body.push_back( listn({ sym("ret"), make_type("i32"), sym("%z") }) );
    auto fn = listn({ sym("fn"), kw("name"), detail::make_node(std::string("main")), kw("ret"), make_type("i32"), kw("params"), vec({}), kw("body"), vec(body) });
    return listn({ sym("module"), fn });
}

static node_ptr bad_continue_outside(){
    std::vector<node_ptr> body;
    body.push_back( listn({ sym("continue") }) ); // invalid outside loop
    body.push_back( listn({ sym("const"), sym("%z"), make_type("i32"), i64(0) }) );
    body.push_back( listn({ sym("ret"), make_type("i32"), sym("%z") }) );
    auto fn = listn({ sym("fn"), kw("name"), detail::make_node(std::string("main")), kw("ret"), make_type("i32"), kw("params"), vec({}), kw("body"), vec(body) });
    return listn({ sym("module"), fn });
}

void phase3_for_continue_tests(){
    std::cout << "[phase3] for/continue tests...\n";
    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tcr;
    {
        auto modAst = simple_fn_for_loop();
        tcr = TypeCheckResult{true,{},{}}; // ensure clean result
        auto *M = emitter.emit(modAst, tcr);
        if(!tcr.success){
            std::cerr << "Unexpected errors in simple for-loop test:\n";
            for(auto &e: tcr.errors) std::cerr<<e.code<<" "<<e.message<<"\n";
        }
        assert(tcr.success);
        (void)M;
    }
    {
        auto modAst = bad_continue_outside();
        IREmitter emitter2(tctx); TypeCheckResult tcr2; emitter2.emit(modAst, tcr2);
        bool found=false; for(auto &e: tcr2.errors){ if(e.code=="E1380") found=true; }
        assert(found);
    }
    // Additional negative tests via parser forms (focus on error codes)
    auto expect_err = [&](const char* src, const std::string& code){
        auto ast=parse(src); TypeChecker tc(tctx); auto res=tc.check_module(ast); bool found=false; for(auto &e: res.errors) if(e.code==code) found=true; if(!found){ std::cerr<<"Expected code "<<code<<" not found in for/continue tests. Got:\n"; for(auto &e: res.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; }
        // Keep assert so Debug builds fail; in Release this will be compiled out but message above will appear.
        assert(found);
    };
    // Missing :init
    expect_err("(module (fn :name \"m\" :ret void :params [] :body [ (const %c i1 1) (for :cond %c :step [] :body []) (ret void 0) ]) )","E1371");
    // Missing :cond
    expect_err("(module (fn :name \"m\" :ret void :params [] :body [ (for :init [] :step [] :body []) (ret void 0) ]) )","E1372");
    // Cond wrong type (use i32 variable as cond)
    expect_err("(module (fn :name \"m\" :ret void :params [] :body [ (const %x i32 0) (for :init [] :cond %x :step [] :body []) (ret void 0) ]) )","E1373");
    // Missing :step
    expect_err("(module (fn :name \"m\" :ret void :params [] :body [ (const %c i1 1) (for :init [] :cond %c :body []) (ret void 0) ]) )","E1375");
    // Missing :body
    expect_err("(module (fn :name \"m\" :ret void :params [] :body [ (const %c i1 1) (for :init [] :cond %c :step []) (ret void 0) ]) )","E1374");
    // Cond symbol without leading % triggers E1372 (treated as missing/invalid cond)
    expect_err("(module (fn :name \"m\" :ret void :params [] :body [ (for :init [] :cond c :step [] :body []) (ret void 0) ]) )","E1372");
    // continue with unexpected operand -> E1381
    expect_err("(module (fn :name \"m\" :ret void :params [] :body [ (const %c i1 1) (for :init [] :cond %c :step [] :body [ (continue %x) ]) (ret void 0) ]) )","E1381");
    std::cout << "[phase3] for/continue tests passed\n";
}
