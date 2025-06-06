#include "Operator.h"

#include "common/Assert.h"

namespace gse {
namespace program {

static const std::unordered_map< operator_type_t, std::string > s_op_labels = {
	{
		OT_NOOP,
		""
	},
	{
		OT_RETURN,
		"return"
	},
	{
		OT_BREAK,
		"break"
	},
	{
		OT_CONTINUE,
		"continue"
	},
	{
		OT_THROW,
		"throw"
	},
	{
		OT_ASSIGN,
		"="
	},
	{
		OT_NOT,
		"!"
	},
	{
		OT_EQ,
		"=="
	},
	{
		OT_NE,
		"!="
	},
	{
		OT_LT,
		"<"
	},
	{
		OT_LTE,
		"<="
	},
	{
		OT_GT,
		">"
	},
	{
		OT_GTE,
		">="
	},
	{
		OT_AND,
		"&&"
	},
	{
		OT_OR,
		"||"
	},
	{
		OT_ADD,
		"+"
	},
	{
		OT_SUB,
		"-"
	},
	{
		OT_MULT,
		"*"
	},
	{
		OT_DIV,
		"/"
	},
	{
		OT_MOD,
		"%"
	},
	{
		OT_INC,
		"++"
	},
	{
		OT_DEC,
		"--"
	},
	{
		OT_INC_BY,
		"+="
	},
	{
		OT_DEC_BY,
		"-="
	},
	{
		OT_MULT_BY,
		"*="
	},
	{
		OT_DIV_BY,
		"/="
	},
	{
		OT_MOD_BY,
		"%="
	},
	{
		OT_CHILD,
		"."
	},
	{
		OT_AT,
		"["
	},
	{
		OT_PUSH,
		":+"
	},
	{
		OT_POP,
		":~",
	},
	{
		OT_ERASE,
		":-",
	},
	{
		OT_RANGE,
		"["
	},
};

Operator::Operator( const si_t& si, const operator_type_t op )
	: Element( si, ET_OPERATOR )
	, op( op ) {}

const std::string Operator::ToString() const {
	const auto it = s_op_labels.find( op );
	ASSERT( it != s_op_labels.end(), "op label not found: " + std::to_string( op ) );
	return it->second;
}
const std::string Operator::Dump( const size_t depth ) const {
	const auto it = s_op_labels.find( op );
	ASSERT( it != s_op_labels.end(), "op label not found: " + std::to_string( op ) );
	return Formatted( "Operator" + m_si.ToString() + "( " + it->second + " )", depth );
}

}
}
