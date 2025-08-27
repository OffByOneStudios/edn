// Basic tuple macro driver: constructs a triple and reads element 1.
#include <iostream>
#include <cassert>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"
using namespace edn; using namespace rustlite;

int main(){
	const char* src = R"((module :id "tuple_basic"
		(rfn :name "main" :ret i32 :params [] :body [
			(const %a i32 10) (const %b i32 20) (const %c i32 30)
			(tuple %t [ %a %b %c ])
			(tget %x i32 %t 1)
			(ret i32 %x)
		])
	))";
	auto ast = parse(src);
	auto expanded = expand_rustlite(ast);
	TypeContext tctx; TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
	if(!tcres.success){ std::cerr << "[tuple_basic] typecheck failed\n"; for(auto &e: tcres.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; return 1; }
	IREmitter em(tctx); TypeCheckResult ir; auto *mod = em.emit(expanded, ir); (void)mod; assert(ir.success);
	std::cout << "tuple_basic OK\n"; return 0;
}
