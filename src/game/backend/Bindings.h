#pragma once

#include <unordered_map>
#include <map>
#include <vector>
#include <functional>

#include "gse/Types.h"
#include "gse/value/Types.h"
#include "gse/Value.h"

#include "gse/Bindings.h"

namespace gse {
class GSE;
class GCWrappable;

namespace context {
class GlobalContext;
}

}

namespace game {
namespace backend {

class System;
class Game;
class State;

class Bindings : public gse::Bindings {
public:
	Bindings( State* state );
	~Bindings();

	void AddToContext( gc::Space* const gc_space, gse::context::Context* ctx, gse::ExecutionPointer& ep ) override;

	void RunMainScript();
	void RunMain();

	gc::Space* const GetGCSpace() const;
	gse::context::Context* const GetContext() const;

	typedef std::function< void( GSE_CALLABLE, gse::value::object_properties_t& args ) > f_args_t;
	gse::Value* const Trigger( gse::GCWrappable* object, const std::string& event, const f_args_t& f_args );

	State* GetState() const;
	Game* GetGame( GSE_CALLABLE ) const;

private:

	std::vector< gse::Value* > m_main_callables = {};

	const gse::si_t m_si_internal = { "" };
	const gse::value::function_arguments_t m_no_arguments = {};

	State* m_state = nullptr;

	const std::string m_entry_script;

	gse::GSE* m_gse = nullptr;
	gse::context::GlobalContext* m_gse_context = nullptr;
};

}
}
