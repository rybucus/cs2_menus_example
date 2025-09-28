#include <CGameRules.h>
#include <CBaseEntity.h>
#include <entitysystem.h>
#include <chrono>
#include <utils.hpp>
#include <menus/menus.h>

#include "../plugin.h"
#include "../shared/players_controller.h"

MenusExample g_Plugin;
PLUGIN_EXPOSE( MenusExample, g_Plugin );

CEntitySystem*	   g_pEntitySystem	= nullptr;
ConVarRefAbstract* mp_freezetime	= nullptr;

SH_DECL_HOOK3_void( IServerGameDLL, GameFrame, SH_NOATTRIB, 0, bool, bool, bool );
SH_DECL_HOOK6( IServerGameClients, ClientConnect, SH_NOATTRIB, 0, bool, CPlayerSlot, const char*, uint64, const char*, bool, CBufferString* );
SH_DECL_HOOK5_void( IServerGameClients, ClientDisconnect, SH_NOATTRIB, 0, CPlayerSlot, ENetworkDisconnectionReason, const char *, uint64, const char * );
SH_DECL_HOOK4_void( IServerGameClients, ClientPutInServer, SH_NOATTRIB, 0, CPlayerSlot, char const*, int, uint64 );
SH_DECL_HOOK2( IGameEventManager2, FireEvent, SH_NOATTRIB, 0, bool, IGameEvent*, bool );

CGameEntitySystem* GameEntitySystem( )
{
	return *reinterpret_cast< CGameEntitySystem** >( reinterpret_cast< uintptr_t >( g_pGameResourceServiceServer ) + 0x58 );;
}

CEntityIdentity* CEntitySystem::GetEntityIdentity( const CEntityHandle& hEnt )
{
	auto pEntity = g_pCtx->m_Addresses.m_pGetBaseEntity.Call< CBaseEntity* >( this, hEnt.GetEntryIndex( ) );
	return pEntity ? pEntity->m_pEntity : nullptr;
}

CEntityIdentity* CEntitySystem::GetEntityIdentity( CEntityIndex iEnt )
{
	auto pEntity = g_pCtx->m_Addresses.m_pGetBaseEntity.Call< CBaseEntity* >( this, iEnt.Get( ) );
	return pEntity ? pEntity->m_pEntity : nullptr;
}

bool MenusExample::Load( PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late )
{
	PLUGIN_SAVEVARS( );

	if ( !g_pCtx->OnInit( ismm, error, maxlen ) )
		return false;

	if ( late )
		g_pEntitySystem = GameEntitySystem( );

	SH_ADD_HOOK( IServerGameDLL, GameFrame, g_pCtx->m_Interfaces.m_pServerDll, SH_MEMBER( this, &MenusExample::Hook_GameFrame ), true );
	SH_ADD_HOOK( IServerGameClients, ClientConnect, g_pCtx->m_Interfaces.m_pGameClients, SH_MEMBER( this, &MenusExample::Hook_ClientConnect ), false );
	SH_ADD_HOOK( IServerGameClients, ClientDisconnect, g_pCtx->m_Interfaces.m_pGameClients, SH_MEMBER( this, &MenusExample::Hook_ClientDisconnect ), true );
	SH_ADD_HOOK( IServerGameClients, ClientPutInServer, g_pCtx->m_Interfaces.m_pGameClients, SH_MEMBER( this, &MenusExample::Hook_ClientPutInServer ), true );
	SH_ADD_HOOK( IGameEventManager2, FireEvent, g_pCtx->m_Interfaces.m_pEventManager, SH_MEMBER( this, &MenusExample::Hook_FireEvent ), false );

	g_SMAPI->AddListener( this, this );

	g_pCVar = g_pCtx->m_Interfaces.m_pCvar;

	mp_freezetime = new ConVarRefAbstract( "mp_freezetime" );

	META_CONVAR_REGISTER( FCVAR_RELEASE | FCVAR_SERVER_CAN_EXECUTE | FCVAR_GAMEDLL );

	META_CONPRINTF( "Starting plugin.\n" );

	return true;
}

bool MenusExample::Unload( char* error, size_t maxlen )
{
	SH_REMOVE_HOOK( IServerGameDLL, GameFrame, g_pCtx->m_Interfaces.m_pServerDll, SH_MEMBER( this, &MenusExample::Hook_GameFrame ), true );
	SH_REMOVE_HOOK( IServerGameClients, ClientDisconnect, g_pCtx->m_Interfaces.m_pGameClients, SH_MEMBER( this, &MenusExample::Hook_ClientDisconnect ), true );
	SH_REMOVE_HOOK( IServerGameClients, ClientConnect, g_pCtx->m_Interfaces.m_pGameClients, SH_MEMBER( this, &MenusExample::Hook_ClientConnect ), false );
	SH_REMOVE_HOOK( IServerGameClients, ClientPutInServer, g_pCtx->m_Interfaces.m_pGameClients, SH_MEMBER( this, &MenusExample::Hook_ClientPutInServer ), true );
	SH_REMOVE_HOOK( IGameEventManager2, FireEvent, g_pCtx->m_Interfaces.m_pEventManager, SH_MEMBER( this, &MenusExample::Hook_FireEvent ), false );

	ConVar_Unregister( );

	return true;
}

void MenusExample::AllPluginsLoaded( )
{
	g_pCtx->OnPluginsReady( );
}

void MenusExample::OnLevelInit( char const* pMapName, char const* pMapEntities, char const* pOldLevel, char const* pLandmarkName, bool loadGame, bool background )
{
	g_pCtx->OnLevelInit( );
}

void MenusExample::OnLevelShutdown( )
{
	g_pCtx->OnLevelShutdown( );

	g_pPlayersController->EraseAllPlayers( );
}

constexpr auto iMaxDisconnectedTicks = TIME_TO_TICKS( 300.f );

void MenusExample::Hook_GameFrame( bool bSimulating, bool bFirstTick, bool bLastTick )
{
	if ( !g_pEntitySystem )
		g_pEntitySystem = GameEntitySystem( );

	if ( !g_pCtx->m_Interfaces.m_pGameRules
		&& g_pEntitySystem )
	{
		const auto pGameRulesProxy = static_cast< CCSGameRulesProxy* >( UTIL_FindEntityByClassname( "cs_gamerules" ) );

		if ( pGameRulesProxy )
			g_pCtx->m_Interfaces.m_pGameRules = pGameRulesProxy->m_pGameRules( );
	}

	if ( g_pCtx->GetGlobalVars( )
		&& g_pCtx->m_Interfaces.m_pGameRules )
	{
		g_pPlayersController->QueueConnectedPlayers( );

		const auto bIsRoundNotBegin   = g_pCtx->m_Interfaces.m_pGameRules->m_fRoundStartTime( ).GetTime( ) >= g_pCtx->GetGlobalVars( )->curtime;

		g_pPlayersController->RunForUniquePlayers
		(
			[ & ]( ::Ruby::IPlayersUtilities* pUtilities, ::Ruby::CPlayerData& PlayerData )
			{
				if ( PlayerData.m_iDisconnectTick != TICK_NEVER_THINK )
				{
					// todo
					return;
				}

				if ( g_pCtx->m_Interfaces.m_pGameRules->m_bWarmupPeriod( ) )
				{
					PlayerData.m_iLastIteractTick = g_pCtx->GetGlobalVars( )->tickcount;
					return;	
				}

				auto pPlayerController = g_pCtx->m_Addresses.m_pGetBaseEntityBySlot.Call< CCSPlayerController* >( PlayerData.m_Slot );

				if ( !pPlayerController )
					return;

				auto pPlayerPawn = reinterpret_cast< CCSPlayerPawn* >( pPlayerController->GetPawn( ) );

				if ( !pPlayerPawn )
					return;

				if ( pPlayerPawn->IsAlive( ) 
					&& ( pPlayerPawn->m_iTeamNum( ) != 1 ) )
				{	
					if ( ( ( pPlayerPawn->m_fFlags & FL_FROZEN ) != 0 )
						|| ( ( pPlayerPawn->m_fFlags & FL_ONGROUND ) == 0 )
						|| bIsRoundNotBegin )
					{
						PlayerData.m_iLastIteractTick = g_pCtx->GetGlobalVars( )->tickcount;
						return;
					}

					const auto vecPrevOrigin     = PlayerData.m_vecLastOrigin;
					const auto angPrevViewAngles = PlayerData.m_angLastViewAngles;

					PlayerData.m_vecLastOrigin     = pPlayerPawn->GetAbsOrigin( );
					PlayerData.m_angLastViewAngles = pPlayerPawn->m_angEyeAngles( );

					if ( ( PlayerData.m_vecLastOrigin - vecPrevOrigin ).Length( ) > 0.5f
						|| ( PlayerData.m_angLastViewAngles != angPrevViewAngles ) )
					{
						PlayerData.m_iLastIteractTick = g_pCtx->GetGlobalVars( )->tickcount;
						return;
					}

					return;
				}

				PlayerData.m_iLastIteractTick = g_pCtx->GetGlobalVars( )->tickcount;
			}
		);
	}
}

bool MenusExample::Hook_ClientConnect( CPlayerSlot Slot, const char* szName, uint64 llUid, const char* szNetworkID, bool bUnk, CBufferString* pRejectReason )
{
	g_pPlayersController->RegisterPlayer( Slot );

	RETURN_META_VALUE( MRES_IGNORED, true );
}

void MenusExample::Hook_ClientDisconnect( CPlayerSlot Slot, ENetworkDisconnectionReason eReason, const char* szName, uint64 llUid, const char* szNetworkID )
{
	if ( g_pCtx->GetGlobalVars( ) )
		g_pPlayersController->PlayerDisconnect( Slot );
}

void MenusExample::Hook_ClientPutInServer( CPlayerSlot iSlot, char const* szName, int iType, uint64 llUid )
{
	g_pPlayersController->ServerPutPlayer( iSlot );
}

bool MenusExample::Hook_FireEvent( IGameEvent* pEvent, bool bDontBroadcast )
{
	if ( !pEvent )
		RETURN_META_VALUE( MRES_IGNORED, false );

	const auto szEventName = pEvent->GetName( );
	if ( szEventName )
	{
		const auto iNameHash = hash_64_fnv1a_const( szEventName );
		switch ( iNameHash )
		{
		case hash_64_fnv1a_const( "round_start" ):
		{
			const auto pGlobals = g_pCtx->GetGlobalVars( );
			if ( pGlobals )
			{
				g_pCtx->m_flAbsRoundStartTime = pGlobals->curtime;
			}
			break;
		}
		case hash_64_fnv1a_const( "player_spawn" ):
		{
			GameEventKeySymbol_t EventHash( "userid" );
			if ( auto pController
				= reinterpret_cast< CCSPlayerController* >( pEvent->GetPlayerController( EventHash ) );
				pController )
			{
				if ( !g_pCtx->m_Interfaces.m_pPlayers->IsFakeClient( pController->GetPlayerSlot( ) )
					&& pController->m_steamID( ) != 0 )
				{
					auto& itUniquePlayer
						= g_pPlayersController->GetUniquePlayer( pController->m_steamID( ) );

					if ( !g_pPlayersController->IsValidUniquePlayer( itUniquePlayer ) )
						break;

					auto& UniquePlayer = std::get< ::Ruby::CPlayerData >( *itUniquePlayer );

					if ( auto pPawn
						= pController->GetPlayerPawn( );
						pPawn )
					{
						switch ( UniquePlayer.m_iHealthMode )
						{
							case 0:  
							{ 
								g_pPlayersController->RegisterFrameCmd
								(
									[ pPawn ]( )
									{
										g_pCtx->m_Addresses.m_pSetHealth.Call< void >( pPawn, 1 ); 
									}
								);
								break; 
							}
							case 2:  
							{ 
								g_pPlayersController->RegisterFrameCmd
								(
									[ pPawn ]( )
									{
										g_pCtx->m_Addresses.m_pSetHealth.Call< void >( pPawn, 1000 );
									}
								);
								break; 
							}
							default: {  break; }
						}
						switch ( UniquePlayer.m_iArmorMode )
						{
							case 1:  { pPawn->m_ArmorValue( ) = 25; break; }
							case 2:  { pPawn->m_ArmorValue( ) = 50;  break; }
							case 3:  { pPawn->m_ArmorValue( ) = 100;  break; }
							default: { pPawn->m_ArmorValue( ) = 0; break; }
						}
					}
				}
			}
			break;
		}
		case hash_64_fnv1a_const( "weapon_fire" ):
		{
			GameEventKeySymbol_t EventHash( "userid" );
			if ( auto pController
				= reinterpret_cast< CCSPlayerController* >( pEvent->GetPlayerController( EventHash ) );
				pController )
			{
				if ( !g_pCtx->m_Interfaces.m_pPlayers->IsFakeClient( pController->GetPlayerSlot( ) ) 
					&& pController->m_steamID( ) != 0 )
				{
					auto& itUniquePlayer
						= g_pPlayersController->GetUniquePlayer( pController->m_steamID( ) );

					if ( !g_pPlayersController->IsValidUniquePlayer( itUniquePlayer ) )
						break;

					auto& UniquePlayer = std::get< ::Ruby::CPlayerData >( *itUniquePlayer );

					if ( auto pPawn
						= pController->GetPlayerPawn( );
						pPawn )
					{
						if ( pPawn->m_pWeaponServices( ) )
						{
							if ( auto pActiveWeapon
								= pPawn->m_pWeaponServices( )->m_hActiveWeapon( ).Get( );
								pActiveWeapon )
							{
								switch ( UniquePlayer.m_iFireMode )
								{
									case 0: { break; }
									case 1: { *reinterpret_cast< int* >( &pActiveWeapon->m_nNextPrimaryAttackTick( ) )
										= g_pCtx->GetGlobalVars( )->tickcount + TIME_TO_TICKS( 5.f ); break; }
									case 2: { *reinterpret_cast< int* >( &pActiveWeapon->m_nNextPrimaryAttackTick( ) )
										= g_pCtx->GetGlobalVars( )->tickcount + TIME_TO_TICKS( 10.f ); break; }
								}
							}
						}
					}
				}
			}
			break;
		}
		default:
			break;
		}
	}

	RETURN_META_VALUE( MRES_IGNORED, true );
}


