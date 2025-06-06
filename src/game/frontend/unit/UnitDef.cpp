#include "UnitDef.h"

#include "game/backend/unit/StaticDef.h"
#include "game/backend/unit/SpriteRender.h"
#include "engine/Engine.h"
#include "loader/texture/TextureLoader.h"
#include "util/String.h"
#include "game/frontend/sprite/InstancedSpriteManager.h"
#include "game/backend/map/Consts.h"

namespace game {
namespace frontend {
namespace unit {

UnitDef::UnitDef( sprite::InstancedSpriteManager* ism, const backend::unit::Def* unitdef )
	: m_ism( ism )
	, m_id( unitdef->m_id )
	, m_name( unitdef->m_name )
	, m_type( unitdef->m_type ) {

	switch ( unitdef->m_type ) {
		case backend::unit::DT_STATIC: {
			const auto* def = (backend::unit::StaticDef*)unitdef;

			switch ( def->m_render->m_type ) {

				case backend::unit::Render::RT_SPRITE: {
					m_render = ( (backend::unit::SpriteRender*)def->m_render )->m_render;

					static_.movement_type = def->m_movement_type;
					static_.movement_per_turn = def->m_movement_per_turn;
					static_.render.is_sprite = true;

					break;
				}
				default:
					THROW( "unknown unit render type: " + std::to_string( def->m_render->m_type ) );
			}
			break;
		}
		default:
			THROW( "unknown unit def type: " + std::to_string( unitdef->m_type ) );
	}
}

UnitDef::~UnitDef() {
	if ( m_type == backend::unit::DT_STATIC ) {
		if ( m_render.morale_based_xshift ) {
			if ( static_.render.morale_based_sprites ) {
				DELETE( static_.render.morale_based_sprites );
			}
		}
	}
}

const bool UnitDef::IsArtillery() const {
	return m_id != "SporeLauncher";
}

sprite::Sprite* UnitDef::GetSprite( const backend::unit::morale_t morale ) {
	ASSERT( m_type == backend::unit::DT_STATIC, "only static units are supported for now" );
	ASSERT( static_.render.is_sprite, "only sprite unitdefs are supported for now" );

	if ( m_render.morale_based_xshift ) {
		if ( !static_.render.morale_based_sprites ) {
			NEW( static_.render.morale_based_sprites, morale_based_sprites_t );
		}
		auto it = static_.render.morale_based_sprites->find( morale );
		if ( it == static_.render.morale_based_sprites->end() ) {
			const uint32_t xshift = m_render.morale_based_xshift * ( morale - backend::unit::MORALE_MIN );
			it = static_.render.morale_based_sprites->insert(
				{
					morale,
					{
						m_ism->GetInstancedSprite(
							"Unit_" + m_id + "_" + std::to_string( morale ), GetSpriteTexture(), {
								m_render.x + xshift,
								m_render.y,
							},
							{
								m_render.w,
								m_render.h,
							},
							{
								m_render.cx + xshift,
								m_render.cy,
							},
							{
								backend::map::s_consts.tile.scale.x,
								backend::map::s_consts.tile.scale.y * backend::map::s_consts.sprite.y_scale
							},
							ZL_UNITS
						),
					}
				}
			).first;
		}
		return &it->second;
	}
	else {
		if ( !static_.render.sprite.instanced_sprite ) {
			static_.render.sprite = {
				m_ism->GetInstancedSprite(
					"Unit_" + m_id, GetSpriteTexture(), {
						m_render.x,
						m_render.y,
					},
					{
						m_render.w,
						m_render.h,
					},
					{
						m_render.cx,
						m_render.cy,
					},
					{
						backend::map::s_consts.tile.scale.x,
						backend::map::s_consts.tile.scale.y * backend::map::s_consts.sprite.y_scale
					},
					ZL_UNITS
				),
				1
			};
		}
		return &static_.render.sprite;
	}
}

const bool UnitDef::IsImmovable() const {
	return static_.movement_type == backend::unit::MT_IMMOVABLE;
}

const std::string UnitDef::GetNameString() const {
	return m_name;
}

const std::string UnitDef::GetStatsString() const {
	std::string str = "";
	if ( m_id == "SporeLauncher" ) {
		str += "(?)";
	}
	else {
		str += "?";
	}
	return str + " - ? - " + util::String::ApproximateFloat( static_.movement_per_turn );

}

types::texture::Texture* UnitDef::GetSpriteTexture() {
	if ( !static_.render.texture ) {
		static_.render.texture = g_engine->GetTextureLoader()->LoadCustomTexture( m_render.file );
	}
	return static_.render.texture;
}

}
}
}
