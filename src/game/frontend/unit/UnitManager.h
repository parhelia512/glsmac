#pragma once

#include <unordered_map>
#include <vector>

#include "common/Common.h"

#include "game/backend/unit/Types.h"

#include "types/Vec2.h"
#include "types/Vec3.h"

namespace game::backend::unit {
class Def;
}

namespace game {
namespace frontend {

class Game;

namespace sprite {
class InstancedSpriteManager;
}

namespace tile {
class Tile;
}

namespace faction {
class Faction;
}

namespace ui_legacy {
class ObjectsList;
}

namespace unit {

class Unit;
class UnitDef;
class BadgeDefs;
class SlotBadges;

CLASS( UnitManager, common::Class )

	UnitManager( Game* game );
	~UnitManager();

	void Iterate();

	Unit* GetUnitById( const size_t id ) const;

	void DefineUnit( const backend::unit::Def* def );
	void UndefineUnit( const std::string& id );
	void SpawnUnit(
		const size_t unit_id,
		const std::string& unitdef_name,
		const size_t slot_index,
		const ::types::Vec2< size_t >& tile_coords,
		const ::types::Vec3& render_coords,
		const backend::unit::movement_t movement,
		const backend::unit::morale_t morale,
		const std::string& morale_string,
		const backend::unit::health_t health
	);
	void DespawnUnit( const size_t unit_id );
	void RefreshUnit( Unit* unit );
	void MoveUnit( Unit* unit, tile::Tile* dst_tile, const size_t animation_id );
	void MoveUnit_deprecated( Unit* unit, tile::Tile* dst_tile, const types::Vec3& dst_render_coords );

	Unit* GetSelectedUnit() const;
	void SelectUnit( Unit* unit, const bool actually_select_unit );
	void DeselectUnit();
	Unit* GetPreviouslyDeselectedUnit() const;
	const bool SelectNextUnitMaybe();
	void SelectNextUnitOrSwitchToTileSelection();

	SlotBadges* GetSlotBadges( const size_t slot_index ) const;
	void DefineSlotBadges( const size_t slot_index, const faction::Faction* faction );

	const types::Vec3 GetCloserCoords( const types::Vec3& coords, const types::Vec3& ref_coords ) const;

private:

	Game* m_game;
	sprite::InstancedSpriteManager* m_ism;

	const size_t m_slot_index;

	Unit* m_selected_unit = nullptr;

	std::unordered_map< std::string, UnitDef* > m_unitdefs = {};
	std::unordered_map< size_t, Unit* > m_units = {};
	std::vector< Unit* > m_selectable_units = {};

	BadgeDefs* m_badge_defs = nullptr;
	std::unordered_map< size_t, SlotBadges* > m_slot_badges = {};

	struct moving_unit_info_t {
		tile::Tile* tile;
		size_t animation_id;
	};
	std::unordered_map< Unit*, moving_unit_info_t > m_moving_units = {};

	Unit* m_previously_deselected_unit = nullptr;

	void AddSelectable( Unit* unit );
	void RemoveSelectable( Unit* unit );
	void UpdateSelectable( Unit* unit );
	Unit* GetNextSelectable();

};

}
}
}
