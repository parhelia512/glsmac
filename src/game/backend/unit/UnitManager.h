#pragma once

#include "gse/GCWrappable.h"

#include "gse/value/Object.h"

#include "Types.h"

namespace game {
namespace backend {

class Game;

namespace map {
class Map;
namespace tile {
class Tile;
}
}

namespace slot {
class Slot;
}

namespace unit {

class MoraleSet;
class Def;
class Unit;

CLASS( UnitManager, gse::GCWrappable )
public:
	UnitManager( Game* game );
	~UnitManager();

	void Clear();
	void DefineMoraleSet( MoraleSet* moraleset );
	void UndefineMoraleSet( const std::string& id );
	void DefineUnit( Def* def );
	void UndefineUnit( const std::string& id );
	void SpawnUnit( GSE_CALLABLE, Unit* unit );
	void DespawnUnit( GSE_CALLABLE, const size_t unit_id );

	MoraleSet* GetMoraleSet( const std::string& name ) const;
	Unit* GetUnit( const size_t id ) const;
	Def* GetUnitDef( const std::string& name ) const;
	const std::map< size_t, Unit* >& GetUnits() const;

	void ProcessUnprocessed( GSE_CALLABLE );
	void PushUpdates();

	WRAPDEFS_PTR( UnitManager )

	void Serialize( types::Buffer& buf ) const;
	void Unserialize( GSE_CALLABLE, types::Buffer& buf );

public:
	// TODO: limit access
	typedef std::function< void() > cb_oncomplete;
	const std::string* MoveUnitToTile( GSE_CALLABLE, Unit* unit, map::tile::Tile* dst_tile, const cb_oncomplete& on_complete );
	const std::string* AttackUnitValidate( GSE_CALLABLE, Unit* attacker, Unit* defender );
	gse::Value* const AttackUnitResolve( GSE_CALLABLE, Unit* attacker, Unit* defender );
	void AttackUnitApply( GSE_CALLABLE, Unit* attacker, Unit* defender, gse::Value* const resolutions );
	void RefreshUnit( GSE_CALLABLE, const Unit* unit );

private:
	Game* m_game = nullptr;

	std::unordered_map< std::string, MoraleSet* > m_unit_moralesets = {};
	std::unordered_map< std::string, Def* > m_unit_defs = {};
	std::map< size_t, Unit* > m_units = {};
	std::vector< Unit* > m_unprocessed_units = {};

	enum unit_update_op_t : uint8_t {
		UUO_NONE = 0,
		UUO_SPAWN = 1 << 0,
		UUO_REFRESH = 1 << 1,
		UUO_DESPAWN = 1 << 2,
	};
	struct unit_update_t {
		unit_update_op_t ops = UUO_NONE;
		const Unit* unit = nullptr;
	};
	std::unordered_map< size_t, unit_update_t > m_unit_updates = {};
	void QueueUnitUpdate( const Unit* unit, const unit_update_op_t op );

	const morale_t GetMorale( GSE_CALLABLE, const int64_t& morale );
	const health_t GetHealth( GSE_CALLABLE, const float health );

private:
	friend class Unit;
	map::Map* GetMap() const;
	slot::Slot* GetSlot( const size_t slot_num ) const;
};

}
}
}
