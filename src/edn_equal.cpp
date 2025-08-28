// Structural equality + single-form parse helper implementation.
#include "edn/edn.hpp"
#include <vector>

namespace edn {

namespace { using detail::reader; using detail::parse_value; }

node_ptr parse_one(std::string_view src) {
	detail::reader r(src);
	r.skip_ws();
	auto v = detail::parse_value(r);
	r.skip_ws();
	if (!r.eof()) {
		throw parse_error("trailing content after single form");
	}
	return v;
}

static bool equal_impl(const node_ptr& a, const node_ptr& b, bool ignore_meta) {
	if (a.get() == b.get()) return true;
	if (!a || !b) return false;
	if (a->data.index() != b->data.index()) return false;

	if (!ignore_meta) {
		if (a->metadata.size() != b->metadata.size()) return false;
		for (const auto& kv : a->metadata) {
			auto it = b->metadata.find(kv.first);
			if (it == b->metadata.end()) return false;
			if (!equal_impl(kv.second, it->second, ignore_meta)) return false;
		}
	}

	struct Visitor {
		const node_ptr& a; const node_ptr& b; bool ignore_meta;
		static bool cmp(const node_ptr& x, const node_ptr& y, bool ig) { return equal_impl(x, y, ig); }
		bool operator()(std::monostate) const { return true; }
		bool operator()(bool) const { return std::get<bool>(a->data) == std::get<bool>(b->data); }
		bool operator()(int64_t) const { return std::get<int64_t>(a->data) == std::get<int64_t>(b->data); }
		bool operator()(double) const { return std::get<double>(a->data) == std::get<double>(b->data); }
		bool operator()(const std::string&) const { return std::get<std::string>(a->data) == std::get<std::string>(b->data); }
		bool operator()(const keyword&) const { return std::get<keyword>(a->data).name == std::get<keyword>(b->data).name; }
		bool operator()(const symbol&) const { return std::get<symbol>(a->data).name == std::get<symbol>(b->data).name; }
		bool operator()(const list&) const {
			const auto& le = std::get<list>(a->data).elems;
			const auto& re = std::get<list>(b->data).elems;
			if (le.size() != re.size()) return false;
			for (size_t i = 0; i < le.size(); ++i) if (!cmp(le[i], re[i], ignore_meta)) return false;
			return true;
		}
		bool operator()(const vector_t&) const {
			const auto& le = std::get<vector_t>(a->data).elems;
			const auto& re = std::get<vector_t>(b->data).elems;
			if (le.size() != re.size()) return false;
			for (size_t i = 0; i < le.size(); ++i) if (!cmp(le[i], re[i], ignore_meta)) return false;
			return true;
		}
		bool operator()(const set&) const {
			const auto& le = std::get<set>(a->data).elems;
			const auto& re = std::get<set>(b->data).elems;
			if (le.size() != re.size()) return false;
			std::vector<bool> used(re.size());
			for (const auto& e : le) {
				bool found = false;
				for (size_t j = 0; j < re.size(); ++j) {
					if (!used[j] && cmp(e, re[j], ignore_meta)) { used[j] = true; found = true; break; }
				}
				if (!found) return false;
			}
			return true;
		}
		bool operator()(const map&) const {
			const auto& lm = std::get<map>(a->data).entries;
			const auto& rm = std::get<map>(b->data).entries;
			if (lm.size() != rm.size()) return false;
			std::vector<bool> used(rm.size());
			for (const auto& kv : lm) {
				bool found = false;
				for (size_t j = 0; j < rm.size(); ++j) {
					if (!used[j] && cmp(kv.first, rm[j].first, ignore_meta) && cmp(kv.second, rm[j].second, ignore_meta)) { used[j] = true; found = true; break; }
				}
				if (!found) return false;
			}
			return true;
		}
		bool operator()(const tagged_value&) const {
			const auto& lt = std::get<tagged_value>(a->data);
			const auto& rt = std::get<tagged_value>(b->data);
			return lt.tag.name == rt.tag.name && cmp(lt.inner, rt.inner, ignore_meta);
		}
	};

	return std::visit(Visitor{a, b, ignore_meta}, a->data);
}

bool equal(const node_ptr& a, const node_ptr& b, bool ignore_metadata) { return equal_impl(a, b, ignore_metadata); }

} // namespace edn