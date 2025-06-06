#include "GLSMAC.h"

#include "util/FS.h"
#include "engine/Engine.h"
#include "config/Config.h"
#include "resource/ResourceManager.h"
#include "task/game/Game.h"
#include "scheduler/Scheduler.h"
#include "util/LogHelper.h"
#include "gc/Space.h"

#include "ui/UI.h"

#include "gse/GSE.h"
#include "gse/ExecutionPointer.h"
#include "gse/Async.h"
#include "gse/context/GlobalContext.h"
#include "gse/callable/Native.h"
#include "gse/Exception.h"
#include "gse/value/Undefined.h"

#include "game/backend/Game.h"
#include "game/backend/State.h"
#include "game/backend/slot/Slots.h"
#include "game/backend/Player.h"
#include "game/backend/faction/Faction.h"
#include "game/backend/faction/FactionManager.h"
#include "game/backend/connection/Server.h"
#include "game/backend/connection/Client.h"

#include "game/frontend/Game.h"

static GLSMAC* s_glsmac = nullptr;

GLSMAC::GLSMAC()
	: gse::GCWrappable( nullptr ) {

	// there can only be one GLSMAC
	ASSERT( !s_glsmac, "GLSMAC already defined" );
	s_glsmac = this;

	Log( "Creating global state" );

	// scripting stuff
	NEW( m_gse, gse::GSE );
	m_gse->AddRootObject( this );
	m_gse->AddBindings( this );
	m_ctx = m_gse->CreateGlobalContext();
	m_gc_space = m_gse->GetGCSpace();

	const auto& c = g_engine->GetConfig();

	const auto entry_script =
		util::FS::GeneratePath(
			{
				c->GetDataPath(),
				"default", // only 'default' mod for now
				c->GetMainScript() // script name (extension is appended automatically)
			}, gse::GSE::PATH_SEPARATOR
		)
	;

	{
		m_gc_space->Accumulate( [ this, &entry_script ]() {
			gse::ExecutionPointer ep;

			// global objects
			m_ui = new ui::UI( m_gc_space, m_ctx, { "" }, ep );

			// run main(s)
			m_gse->RunScript( m_gc_space, m_ctx, {}, ep, entry_script );
			for ( const auto& mod_path : g_engine->GetConfig()->GetModPaths() ) {
				m_gse->RunScript(
					m_gc_space, m_ctx, {}, ep, util::FS::GeneratePath(
						{
							mod_path,
							"main"
						}
					)
				);
			}
		});
	}
}

GLSMAC::~GLSMAC() {

	if ( m_load_thread ) {
		Log( "Waiting for load thread to stop" );
		m_load_thread.load()->join();
		delete m_load_thread;
	}

	if ( m_is_loader_shown ) {
		HideLoader();
	}

	Log( "Destroying global state" );

	if ( m_game ) {
		m_game->Stop();
		delete m_game;
	}

	m_gse->GetAsync()->StopTimers();

	m_gc_space->Accumulate( [ this ](){
		gse::ExecutionPointer ep;
		m_ui->Destroy( m_gc_space, m_ctx, { "" }, ep );
	});

	DELETE( m_gse );

	s_glsmac = nullptr;
}

void GLSMAC::Iterate() {
	{
		bool ticked = false;
		while ( m_loader_dots_timer.HasTicked() ) {
			m_loader_dots++;
			if ( m_loader_dots > 3 ) {
				m_loader_dots = 1;
			}
			ticked = true;
		}
		if ( ticked ) {
			UpdateLoaderText();
		}
	}
	if ( m_load_thread ) {
		if ( !m_is_loading ) {
			ASSERT( m_f_after_load, "f_after_load not set" );
			m_load_thread.load()->join();
			delete m_load_thread;
			m_load_thread = nullptr;
			m_gc_space->Accumulate( [ this ](){
				HideLoader();
				m_f_after_load();
			});
			m_f_after_load = nullptr;
		}
	}
	m_gse->Iterate();
	m_ui->Iterate();
	if ( m_game ) {
		m_game->Iterate();
	}
}

WRAPIMPL_BEGIN( GLSMAC )
	auto* game = g_engine->GetGame();
	WRAPIMPL_PROPS
	WRAPIMPL_TRIGGERS
		{
			"config",
			g_engine->GetConfig()->Wrap( GSE_CALL, true ),
		},
		{
			"ui",
			m_ui->Wrap( GSE_CALL, true )
		},
		{
			"run",
			NATIVE_CALL( this ) {
				N_EXPECT_ARGS( 0 );
				S_Init( GSE_CALL, {} );
				return VALUE( gse::value::Undefined );
			} )
		},
		{
			"exit",
			NATIVE_CALL( this ) {
				N_EXPECT_ARGS( 0 );
				g_engine->ShutDown();
				return VALUE( gse::value::Undefined );
			} )
		},
		{
			"deinit",
			NATIVE_CALL( this ) {
				N_EXPECT_ARGS( 0 );
				DeinitGameState( GSE_CALL );
				return VALUE( gse::value::Undefined );
			} )
		},
		{
			"init",
			NATIVE_CALL( this ) {
				N_EXPECT_ARGS( 0 );
				InitGameState( GSE_CALL );
				return VALUE( gse::value::Undefined );
			} ),
		},
		{
			"game",
			game->Wrap( GSE_CALL, true ),
		},
		{
			"connect",
			NATIVE_CALL( this ) {
				N_EXPECT_ARGS( 0 );
				if ( !m_state ) {
					GSE_ERROR( gse::EC.GAME_ERROR, "Game not initialized" );
				}
				if ( m_state->GetConnection() ) {
					GSE_ERROR( gse::EC.GAME_ERROR, "Connection is already established" );
				}
				if ( m_is_game_running ) {
					GSE_ERROR( gse::EC.GAME_ERROR, "Game is already running" );
				}
				game::backend::connection::Connection* connection = nullptr;
				switch ( m_state->m_settings.local.network_role ) {
					case game::backend::settings::LocalSettings::NR_SERVER: {
						NEW( connection, game::backend::connection::Server, gc_space, &m_state->m_settings.local );
						break;
					}
					case game::backend::settings::LocalSettings::NR_CLIENT: {
						NEW( connection, game::backend::connection::Client, gc_space, &m_state->m_settings.local );
						break;
					}
					default:
						GSE_ERROR( gse::EC.GAME_ERROR, "Network role not set" );
				}
				m_state->SetConnection( connection );
				return connection->Wrap( GSE_CALL, true );
			} ),
		},
		{
			"start_game",
			NATIVE_CALL( this ) {
				N_EXPECT_ARGS( 0 );
				if ( !m_state ) {
					GSE_ERROR( gse::EC.GAME_ERROR, "Game not initialized" );
				}
				if ( m_is_game_running ) {
					GSE_ERROR( gse::EC.GAME_ERROR, "Game is already running" );
				}
				if ( m_is_loading ) {
					GSE_ERROR( gse::EC.GAME_ERROR, "Game still initializing" );
				}
				AddSinglePlayerSlot( nullptr );
				StartGame( GSE_CALL );
				return VALUE( gse::value::Undefined );
			} )
		},
	};
WRAPIMPL_END_PTR()

UNWRAPIMPL_PTR( GLSMAC )

void GLSMAC::ShowLoader( const std::string& text ) {
	if ( !m_is_loader_shown ) {
		m_is_loader_shown = true;
		m_loader_text = text;
		m_loader_dots = 1;
		m_loader_dots_timer.SetInterval( 100 );
		m_gc_space->Accumulate( [ this ] () {
			TriggerObject( m_ui, "loader_show" );
		});
	}
	UpdateLoaderText();
}

void GLSMAC::SetLoaderText( const std::string& text ) {
	if ( !m_is_loader_shown ) {
		ShowLoader( text );
	}
	else {
		m_loader_text = text;
		UpdateLoaderText();
	}
}

void GLSMAC::HideLoader() {
	if ( m_is_loader_shown ) {
		m_is_loader_shown = false;
		m_loader_dots_timer.Stop();
		m_gc_space->Accumulate( [ this ](){
			TriggerObject( m_ui, "loader_hide" );
		});
	}
}

void GLSMAC::ShowError( const std::string& text, const std::function< void() >& on_close ) {
	Log( text );
	m_gc_space->Accumulate([ this ](){
		TriggerObject( m_ui, "error_popup" );
	});
}

gse::Value* const GLSMAC::TriggerObject( gse::Wrappable* object, const std::string& event, const f_args_t& f_args ) {
	gse::ExecutionPointer ep;
	return object->Trigger( m_gc_space, m_ctx, {}, ep, event, f_args );
}

void GLSMAC::WithGSE( const std::function<void( GSE_CALLABLE )>& f ) {
	m_state->WithGSE( f );
}

void GLSMAC::GetReachableObjects( std::unordered_set< gc::Object* >& reachable_objects ) {
	gse::GCWrappable::GetReachableObjects( reachable_objects );

	g_engine->GetGame()->GetReachableObjects( reachable_objects );

	if ( m_state ) {
		GC_DEBUG_BEGIN( "state" );
		GC_REACHABLE( m_state );
		GC_DEBUG_END();
	}

	if ( m_wrapobj ) {
		GC_DEBUG_BEGIN( "wrapobj" );
		GC_REACHABLE( m_wrapobj );
		GC_DEBUG_END();
	}

	if ( m_ui ) {
		GC_DEBUG_BEGIN( "ui" );
		GC_REACHABLE( m_ui );
		GC_DEBUG_END();
	}

	GC_DEBUG_BEGIN( "main callables" );
	for ( const auto& m : m_main_callables ) {
		GC_REACHABLE( m );
	}
	GC_DEBUG_END();

}

void GLSMAC::AsyncLoad( const std::string& text, const std::function< void() >& f, const std::function< void() >& f_after_load ) {
	ASSERT( !m_load_thread, "load thread already set" );
	ASSERT( !m_f_after_load, "f_after load already set" );
	m_is_loading = true;
	m_f_after_load = f_after_load;
	ShowLoader( text );
	m_load_thread = new std::thread( [ this, f ] {
		f();
		m_is_loading = false;
	});
}

void GLSMAC::S_Init( GSE_CALLABLE, const std::optional< std::string >& path ) {
	const auto& c = g_engine->GetConfig();
	auto* r = g_engine->GetResourceManager();
	if ( !m_state ) {
		m_state = new game::backend::State( m_gc_space, m_ctx, this );
		m_state->m_settings.global.Initialize();
	}
	try {
		if ( path.has_value() ) {
			r->Init( { path.value() }, config::ST_AUTO );
		}
		else {
			r->Init( c->GetPossibleSMACPaths(), c->GetSMACType() );
		}
	} catch ( const std::runtime_error& e ) {
		gse::value::object_properties_t args = {
			{
				"set_smacpath", NATIVE_CALL( this ) {
					N_EXPECT_ARGS( 1 );
					N_GETVALUE( path, 0, String );
					S_Init( GSE_CALL, path );
					return VALUE( gse::value::Undefined );
				} )
			}
		};
		if ( path.has_value() ) {
			args.insert({ "last_failed_path", VALUE( gse::value::String,, path.value() ) } );
		}
		TriggerObject( this, "smacpath_prompt", ARGS( args ) );
		return;
	}
	if ( path.has_value() ) {
		c->SetSMACPath( path.value() );
	}

	if ( c->HasLaunchFlag( config::Config::LF_QUICKSTART ) ) {
		InitGameState( GSE_CALL );
		m_state->m_settings.local.game_mode = game::backend::settings::LocalSettings::GM_SINGLEPLAYER;
		m_state->m_settings.global.Initialize();
		const auto& rules = m_state->m_settings.global.rules;
		game::backend::faction::Faction* faction = nullptr;
		if ( c->HasLaunchFlag( config::Config::LF_QUICKSTART_FACTION ) ) {
			const auto* fm = m_state->GetFM();
			ASSERT( fm, "fm is null" );
			faction = fm->Get( c->GetQuickstartFaction() );
			if ( !faction ) {
				std::string errmsg = "Faction \"" + c->GetQuickstartFaction() + "\" does not exist. Available factions:";
				for ( const auto& f : fm->GetAll() ) {
					errmsg += " " + f->m_id;
				}
				THROW( errmsg );
			}
		}
		AddSinglePlayerSlot( faction );
		auto ep2 = ep;
		StartGame( m_gc_space, ctx, si, ep2 );
	}
	else if (
		c->HasLaunchFlag( config::Config::LF_SKIPINTRO ) ||
		r->GetDetectedSMACType() == config::ST_LEGACY // it doesn't have firaxis logo image
	) {
		S_MainMenu( GSE_CALL );
	}
	else {
		S_Intro( GSE_CALL );
	}
}

void GLSMAC::S_Intro( GSE_CALLABLE ) {
	TriggerObject( this, "intro", ARGS_F( this ) {
		{
			"mainmenu", NATIVE_CALL( this ) {
				N_EXPECT_ARGS( 0 );
				S_MainMenu( GSE_CALL );
				return VALUE( gse::value::Undefined );
			} )
		}
	}; } );
}

void GLSMAC::S_MainMenu( GSE_CALLABLE ) {
	TriggerObject( this, "mainmenu_show", ARGS_F( this ) {
		{
			"settings",
			m_state->m_settings.Wrap( GSE_CALL, true ),
		}
	}; } );
}

void GLSMAC::S_Game( GSE_CALLABLE ) {
	TriggerObject( this, "mainmenu_hide", {} );
	m_game->Start();
}

void GLSMAC::UpdateLoaderText() {
	ASSERT( m_is_loader_shown, "loader not shown" );
	std::string text = m_loader_text + std::string( m_loader_dots, '.' ) + std::string( 3 - m_loader_dots, ' ' );
	m_gc_space->Accumulate( [ this, &text ]() {
		TriggerObject( m_ui, "loader_text", ARGS_F( this, &text ) {
			{
				"text", VALUEEXT( gse::value::String, m_gc_space, text )
			}
		}; } );
	});
}

void GLSMAC::DeinitGameState( GSE_CALLABLE ) {
	if ( m_state ) {
		if ( m_is_game_running ) {
			GSE_ERROR( gse::EC.GAME_ERROR, "Game is still running" );
		}
		m_state->Reset();
	}
}

void GLSMAC::InitGameState( GSE_CALLABLE ) {
	if ( m_is_game_running ) {
		GSE_ERROR( gse::EC.GAME_ERROR, "Game is already running" );
	}
	auto* game = g_engine->GetGame();
	m_state->Reset();
	m_state->SetGame( game );
	game->SetState( m_state );
	m_state->WithGSE( [ this ]( GSE_CALLABLE ) {
		TriggerObject( this, "configure_state", ARGS_F( this ) {
			{
				"fm",
				m_state->GetFM()->Wrap( GSE_CALL, true ),
			}
		}; } );
	});
}

void GLSMAC::RandomizeSettings( GSE_CALLABLE ) {
	if ( !m_state ) {
		GSE_ERROR( gse::EC.GAME_ERROR, "Game is not initialized" );
	}
	if ( m_is_game_running ) {
		GSE_ERROR( gse::EC.GAME_ERROR, "Game is already running" );
	}
}

void GLSMAC::AddSinglePlayerSlot( game::backend::faction::Faction* const faction ) {
	m_state->m_slots->Resize( 7 ); // TODO: make dynamic?
	const auto& rules = m_state->m_settings.global.rules;
	m_state->m_settings.local.player_name = "Player";
	NEWV( player, ::game::backend::Player,
		m_state->m_settings.local.player_name,
		::game::backend::Player::PR_SINGLE,
		faction,
		rules.GetDefaultDifficultyLevel() // TODO: make configurable
	);
	m_state->AddPlayer( player );
	size_t slot_num = 0; // player always has slot 0
	m_state->AddCIDSlot( 0, slot_num ); // for consistency
	auto& slot = m_state->m_slots->GetSlot( slot_num );
	slot.SetPlayer( player, 0, "" );
	slot.SetPlayerFlag( ::game::backend::slot::PF_READY );
	slot.SetLinkedGSID( m_state->m_settings.local.account.GetGSID() );
}

void GLSMAC::StartGame( GSE_CALLABLE ) {
	// real state belongs to game task now
	// save it as backup, then make temporary shallow copy (no connection, players etc)
	//   just for the sake of passing settings to previous menu
	auto* real_state = m_state;

	//m_state = nullptr; // TODO: why?

	m_is_game_running = true;

	auto* game = g_engine->GetGame();
	ASSERT( game, "game not set" );

	m_game = new ::game::frontend::Game( nullptr, this, real_state, UH( this, ctx, si, ep ) {
		//g_engine->GetScheduler()->RemoveTask( this );
		m_gc_space->Accumulate( [ this ](){
			TriggerObject( this, "start_game" );
		});
	}, UH( this, real_state ) {
		//m_menu_object->MaybeClose();
		THROW( "TODO: cancel" );
	} );

	S_Game( GSE_CALL );

	TriggerObject( this, "configure_game", ARGS_F( &game ) {
		{
			"game",
			game->Wrap( GSE_CALL, true ),
		}
	}; } );
}

void GLSMAC::RunMain() {
	ASSERT( m_gc_space, "gc space is null" );
	for ( const auto& main : m_main_callables ) {
		ASSERT( main->type == gse::Value::T_CALLABLE, "main not callable" );
		m_gc_space->Accumulate( [ this, &main ] (){
			ASSERT( !m_wrapobj, "GLSMAC wrapobj already set" );
			gse::ExecutionPointer ep;
			m_wrapobj = Wrap( m_gc_space, m_ctx, {}, ep );
			( (gse::value::Callable*)main )->Run( m_gc_space, m_ctx, {}, ep, {
				m_wrapobj
			} );
		});
	}
}

void GLSMAC::AddToContext( gc::Space* const gc_space, gse::context::Context* ctx, gse::ExecutionPointer& ep ) {
	ctx->CreateBuiltin( "main", NATIVE_CALL( this ) {
		N_EXPECT_ARGS( 1 );
		const auto& main = arguments.at(0);
		N_CHECKARG( main, 0, Callable );
		m_main_callables.push_back( main );
		return VALUE( gse::value::Undefined );
	} ), ep );
}
