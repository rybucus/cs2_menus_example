#ifndef _PLUGIN_CTX
#define _PLUGIN_CTX

#ifdef _WIN64 || _WIN32
	#define PLAT_WINDOWS 1
	#define PLAY_LINUX 0
#else
	#define PLAY_LINUX 1
	#define PLAT_WINDOWS 0
#endif

#include <edict.h>
#include <ISmmPlugin.h>
#include <entitysystem.h>
#include <CGameRules.h>
#include <iserver.h>
#include <module.h>
#include <function.h>
#include <igameevents.h>
#include <menus/menus.h>
#include <memory>

PLUGIN_GLOBALVARS( );

#define INIT_MODULE( var, module_name_str ) DynLibUtils::CModule var{ module_name_str };
#define INIT_SERVER_MODULE( var ) DynLibUtils::CModule var{ "server", true };

#define INIT_ADDRESS( mod, var, pat ) var = mod.FindBytesPattern( pat; if ( !var ) return false;
#define INIT_ADDRESS_RELATIVE( mod, var, pat, pre, post ) var = mod.FindBytesPattern( pat ).Relative( pre, post ); if ( !var ) return false;
#define INIT_FUNCTION( mod, var, pat ) var = DynLibUtils::CFunction( mod, pat ); if ( !var ) return false;
#define INIT_FUNCTION_RELATIVE( mod, var, pat, pre, post ) var = DynLibUtils::CFunction( mod, pat, pre, post ); if ( !var ) return false;

#define TICK_NEVER_THINK static_cast< int >( -1 )
#define INVALID_SLOT static_cast< CPlayerSlot >( -1 )
#define INTERVAL_PER_TICK 0.015625f
#define TIME_TO_TICKS( TIME ) ( static_cast< int >( 0.5f + static_cast< float >( TIME ) / INTERVAL_PER_TICK ) )
#define TICKS_TO_TIME( TICKS ) ( INTERVAL_PER_TICK * static_cast< float >( TICKS ) )
#define ROUND_TO_TICKS( TIME ) ( INTERVAL_PER_TICK * TIME_TO_TICKS( TIME ) )
#define STOP_EPSILON 0.1f

namespace Ruby
{
	namespace Constans
	{
		constexpr auto isWindows = PLAT_WINDOWS ? true : false;
		constexpr auto isLinux   = PLAY_LINUX ? true : false;
	}

	class CCtx
	{
	public:

		CCtx( ) = default;

		bool OnInit( ISmmAPI* ismm, char* error, size_t maxlen );
		void OnPluginsReady( );

		void OnLevelInit( )
		{
			m_bIsMapLoaded = true;
		}

		void OnLevelShutdown( )
		{
			m_Interfaces.m_pGameRules = nullptr;
			m_bIsMapLoaded = false;
			m_flAbsRoundStartTime = -1.f;
		}

		inline static CGlobalVars* GetGlobalVars( )
		{
			INetworkGameServer* pGameServer = g_pNetworkServerService->GetIGameServer( );

			if ( !pGameServer )
				return nullptr;

			return g_pNetworkServerService->GetIGameServer( )->GetGlobals( );
		}

	public:

		bool		 m_bIsMapLoaded	{ };
		float		 m_flAbsRoundStartTime { };

		struct
		{
			IServerGameDLL*     m_pServerDll     { };
			IServerGameClients* m_pGameClients   { };
			IVEngineServer2*	m_pEngine		 { };
			CCSGameRules*		m_pGameRules	 { };
			ICvar*				m_pCvar			 { };
			IGameEventManager2* m_pEventManager  { };
			IUtilsApi*			m_pUtils		 { };
			IMenusApi*			m_pMenus		 { };
			IPlayersApi*		m_pPlayers		 { };
		}
		m_Interfaces;

		struct
		{ 
			struct
			{
				DynLibUtils::CModule m_hServerModule{ "server", true };
			}
			m_Modules;

			DynLibUtils::CFunction m_pGetBaseEntityBySlot;
			DynLibUtils::CFunction m_pGetBaseEntity;
			DynLibUtils::CFunction m_pLevelInit;
			DynLibUtils::CFunction m_pFireEvent;
			DynLibUtils::CFunction m_pSetMaxHealth;
			DynLibUtils::CFunction m_pSetHealth;

			DynLibUtils::CMemory   m_pGameEventsManager;

			bool Inititalize( )
			{
				INIT_FUNCTION( m_Modules.m_hServerModule, m_pGetBaseEntityBySlot, "48 83 EC 28 48 8B 05 ?? ?? ?? ?? 48 85 C0 74 1D" );
				INIT_FUNCTION( m_Modules.m_hServerModule, m_pLevelInit, "48 89 5C 24 18 56 48 83 EC 30 48 8B 05" );
				INIT_FUNCTION( m_Modules.m_hServerModule, m_pGetBaseEntity, "4C 8D 49 10 81 FA ?? ?? ?? ??" );
				INIT_FUNCTION( m_Modules.m_hServerModule, m_pFireEvent, "40 53 57 41 54 41 55 41 56 48 83 EC 30" );
				INIT_FUNCTION_RELATIVE( m_Modules.m_hServerModule, m_pSetMaxHealth, "E8 ? ? ? ? F6 07 01 75 1B", 0x1, 0x0 );
				// "Get the health of this entity."
				INIT_FUNCTION_RELATIVE( m_Modules.m_hServerModule, m_pSetHealth, "E8 ? ? ? ? 8D 47 FE", 0x1, 0x0 );
				// xref to CGameEventListener::`vftable' or xref to someone event_name
				INIT_ADDRESS_RELATIVE( m_Modules.m_hServerModule, m_pGameEventsManager, "48 8B 0D ? ? ? ? 48 8D 15 ? ? ? ? 45 33 C9 48 89 B4 24 ? ? ? ? 45 33 C0", 0x3, 0x0 );

				return true;
			}
		} 
		m_Addresses;

	};
}

inline auto g_pCtx = std::make_unique< Ruby::CCtx >( );

#endif // !_PLUGIN_CTX

