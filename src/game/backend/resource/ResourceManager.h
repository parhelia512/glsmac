#pragma once

#include <vector>
#include <unordered_map>

#include "gse/GCWrappable.h"

#include "gse/value/Object.h"

#include "game/backend/map/tile/Types.h"

namespace game {
namespace backend {

class Game;

namespace slot {
class Slot;
}

namespace map::tile {
class Tile;
}

namespace resource {

class Resource;

CLASS( ResourceManager, gse::GCWrappable )

	ResourceManager( Game* game );
	~ResourceManager();

	void Clear();
	void DefineResource( resource::Resource* resource );
	void UndefineResource( const std::string& id );

	const map::tile::yields_t GetYields( GSE_CALLABLE, map::tile::Tile* tile, slot::Slot* slot );

	WRAPDEFS_PTR( ResourceManager )

	void Serialize( types::Buffer& buf ) const;
	void Unserialize( types::Buffer& buf );

private:
	Game* m_game;

	std::unordered_map< std::string, resource::Resource* > m_resources = {};
};

}
}
}
