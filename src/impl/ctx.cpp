#include "../ctx.h"

#include <windows.h>
#include <tlhelp32.h>
#include <utils.hpp>
#include <CCSPlayerController.h>
#include <schemasystem.h>

#include "../shared/players_controller.h"

namespace Ruby
{
	bool CCtx::OnInit( ISmmAPI* ismm, char* error, size_t maxlen )
	{
		META_CONPRINTF( "m_hServerModule are: %p\n", m_Addresses.m_Modules.m_hServerModule.GetModuleBase( ).GetPtr( ) );

		auto result = m_Addresses.m_Modules.m_hServerModule.GetModuleBase( ) != 0;

		if ( !result )
		{
			META_CONPRINTF( "m_hServerModule nullptr!\n" );
			return result;
		}

		META_CONPRINTF( "Parsing Patterns...\n");

		result &= m_Addresses.Inititalize( );

		if ( !result )
		{
			META_CONPRINTF( "Failed to parse patterns!\n" );
			return result;
		}

		META_CONPRINTF( "Patterns good. Now parsing interfaces...\n" );

		GET_V_IFACE_ANY( GetServerFactory, m_Interfaces.m_pServerDll, IServerGameDLL, INTERFACEVERSION_SERVERGAMEDLL );
		GET_V_IFACE_ANY( GetEngineFactory, g_pNetworkServerService, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION );
		GET_V_IFACE_ANY( GetServerFactory, m_Interfaces.m_pGameClients, IServerGameClients, INTERFACEVERSION_SERVERGAMECLIENTS );
		GET_V_IFACE_ANY( GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION );
		GET_V_IFACE_CURRENT( GetEngineFactory, m_Interfaces.m_pEngine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION );
		GET_V_IFACE_CURRENT( GetEngineFactory, g_pGameResourceServiceServer, IGameResourceService, GAMERESOURCESERVICESERVER_INTERFACE_VERSION );
		GET_V_IFACE_CURRENT( GetEngineFactory, m_Interfaces.m_pCvar, ICvar, CVAR_INTERFACE_VERSION );

		m_Interfaces.m_pEventManager = m_Addresses.m_pGameEventsManager.DerefSelf( ).RCast< IGameEventManager2* >( );

		return result;
	}

	void CCtx::OnPluginsReady( )
	{
		int iRetCode = 0;

		m_Interfaces.m_pUtils = reinterpret_cast< IUtilsApi* >( g_SMAPI->MetaFactory( Utils_INTERFACE, &iRetCode, NULL ) );
		if ( iRetCode == META_IFACE_FAILED
			|| !m_Interfaces.m_pUtils  )
		{
			m_Interfaces.m_pEngine->ServerCommand( ( "meta unload " + std::to_string( g_PLID ) ).c_str( ) );
			return;
		}

		m_Interfaces.m_pPlayers = reinterpret_cast< IPlayersApi* >( g_SMAPI->MetaFactory( PLAYERS_INTERFACE, &iRetCode, NULL ) );
		if ( iRetCode == META_IFACE_FAILED
			|| !m_Interfaces.m_pPlayers )
		{
			m_Interfaces.m_pEngine->ServerCommand( ( "meta unload " + std::to_string( g_PLID ) ).c_str( ) );
			return;
		}

		m_Interfaces.m_pMenus = reinterpret_cast< IMenusApi* >( g_SMAPI->MetaFactory( Menus_INTERFACE, &iRetCode, NULL ) );
		if ( iRetCode == META_IFACE_FAILED
			|| !m_Interfaces.m_pMenus )
		{
			m_Interfaces.m_pEngine->ServerCommand( ( "meta unload " + std::to_string( g_PLID ) ).c_str( ) );
			return;
		}

		std::vector< std::string > vecCommands{ std::string( "!settings" ) };
		m_Interfaces.m_pUtils->RegCommand
		( 
			g_PLID, 
			{ "settings" },
			std::vector< std::string >{ std::string( "!settings" ) },
			[ ]( int iSlot, const char* szContent ) -> bool
			{
				Menu hMenu;
				g_pCtx->m_Interfaces.m_pMenus->SetTitleMenu( hMenu, "Players Settings" );

				for ( auto i = 0; i < 64; i++ )
				{
					if ( g_pCtx->m_Interfaces.m_pPlayers->IsFakeClient( i ) )
						continue;

					if ( auto pController = g_pCtx->m_Addresses.m_pGetBaseEntityBySlot.Call< CCSPlayerController* >( i )
						; pController )
					{
						if ( auto pPawn = pController->GetPawn( ); 
							pPawn )
						{
							if ( auto szName = pController->GetPlayerName( );
								szName )
							{
								g_pCtx->m_Interfaces.m_pMenus->AddItemMenu( hMenu,
									"player_choiced", szName );
							}
						}
					}
				}

				g_pCtx->m_Interfaces.m_pMenus->SetExitMenu( hMenu, true );

				g_pCtx->m_Interfaces.m_pMenus->SetCallback
				(
					hMenu,
					[ ]( const char* szBack, const char* szFront, int iItem, int iSlot )
					{
						if ( hash_64_fnv1a_const( szBack )
							== hash_64_fnv1a_const( "player_choiced" ) )
						{
							std::uint64_t iSteamId = 0;
							for ( auto i = 0; i < 64; i++ )
							{
								if ( g_pCtx->m_Interfaces.m_pPlayers->IsFakeClient( i ) )
									continue;

								if ( auto pController = g_pCtx->m_Addresses.m_pGetBaseEntityBySlot.Call< CCSPlayerController* >( i )
									; pController )
								{
									if ( auto szName = pController->GetPlayerName( );
										szName )
									{
										if ( hash_64_fnv1a_const( g_pCtx->m_Interfaces.m_pMenus->escapeString( szName ).c_str( ) ) 
											== hash_64_fnv1a_const( szFront ) )
										{
											iSteamId = pController->m_steamID( );
											break;
										}
									}
								}
							}

							[ iSlot, iSteamId ]( )
							{
								g_pCtx->m_Interfaces.m_pMenus->ClosePlayerMenu( iSlot );

								Menu hPlayerMenu;
								g_pCtx->m_Interfaces.m_pMenus->SetTitleMenu( hPlayerMenu, "Choice type" );

								g_pCtx->m_Interfaces.m_pMenus->AddItemMenu( hPlayerMenu,
									"health_settings", "Health Settings" );

								g_pCtx->m_Interfaces.m_pMenus->AddItemMenu( hPlayerMenu,
									"armor_settings", "Armor Settings" );

								g_pCtx->m_Interfaces.m_pMenus->AddItemMenu( hPlayerMenu,
									"fire_mode_settings", "Fire mode Settings" );

								g_pCtx->m_Interfaces.m_pMenus->SetExitMenu( hPlayerMenu, true );

								g_pCtx->m_Interfaces.m_pMenus->SetCallback
								( 
									hPlayerMenu,
									[ iSteamId ]( const char* szBack, const char* szFront, int iItem, int iSlot )
									{
										auto& itUniquePlayer = g_pPlayersController->GetUniquePlayer( iSteamId );
										if ( !g_pPlayersController->IsValidUniquePlayer( itUniquePlayer ) )
											return;

										auto& UniquePlayer   = std::get< ::Ruby::CPlayerData >( *itUniquePlayer );
										const auto iSlotCopy = iSlot;

										const auto iBackHash = hash_64_fnv1a_const( szBack );

										switch ( iBackHash )
										{
											case hash_64_fnv1a_const( "health_settings" ):
											{
												Menu hPlayerHealthMenu;
												g_pCtx->m_Interfaces.m_pMenus->SetTitleMenu( hPlayerHealthMenu, ( "Health Settings" ) );

												g_pCtx->m_Interfaces.m_pMenus->AddItemMenu( hPlayerHealthMenu,
													"1", ( UniquePlayer.m_iHealthMode == 0 ? "Selected: 1 HP" : "1 HP" ) );
												g_pCtx->m_Interfaces.m_pMenus->AddItemMenu( hPlayerHealthMenu,
													"2", ( UniquePlayer.m_iHealthMode == 1 ? "Selected: 100 HP" : "100 HP" ) );
												g_pCtx->m_Interfaces.m_pMenus->AddItemMenu( hPlayerHealthMenu,
													"3", ( UniquePlayer.m_iHealthMode == 2 ? "Selected: 1000 HP" : "1000 HP" ) );

												g_pCtx->m_Interfaces.m_pMenus->SetExitMenu( hPlayerHealthMenu, true );

												g_pCtx->m_Interfaces.m_pMenus->SetCallback
												(
													hPlayerHealthMenu,
													[ itUniquePlayer ]( const char* szBack, const char* szFront, int iItem, int iSlot )
													{
														auto& pUniquePlayer = std::get< ::Ruby::CPlayerData >( *itUniquePlayer );

														const auto iHpMode = std::atoi( szBack );

														switch ( iHpMode )
														{
														case 1:
														{
															pUniquePlayer.m_iHealthMode = 0;
															break;
														}
														case 2:
														{
															pUniquePlayer.m_iHealthMode = 1;
															break;
														}
														case 3:
														{
															pUniquePlayer.m_iHealthMode = 2;
															break;
														}
														}

														g_pCtx->m_Interfaces.m_pMenus->ClosePlayerMenu( iSlot );
													}
												);

												g_pCtx->m_Interfaces.m_pMenus->ClosePlayerMenu( iSlotCopy );

												g_pCtx->m_Interfaces.m_pMenus->DisplayPlayerMenu( hPlayerHealthMenu, iSlotCopy, true, true );
												break;
											}
											case hash_64_fnv1a_const( "armor_settings" ):
											{
												Menu hPlayerArmorMenu;
												g_pCtx->m_Interfaces.m_pMenus->SetTitleMenu( hPlayerArmorMenu, ( "Armor Settings" ) );

												g_pCtx->m_Interfaces.m_pMenus->AddItemMenu( hPlayerArmorMenu,
													"1", ( UniquePlayer.m_iArmorMode == 0 ? "Selected: 0 Armor" : "0 Armor" ) );
												g_pCtx->m_Interfaces.m_pMenus->AddItemMenu( hPlayerArmorMenu,
													"2", ( UniquePlayer.m_iArmorMode == 1 ? "Selected: 25 Armor" : "25 Armor" ) );
												g_pCtx->m_Interfaces.m_pMenus->AddItemMenu( hPlayerArmorMenu,
													"3", ( UniquePlayer.m_iArmorMode == 2 ? "Selected: 50 Armor" : "75 Armor" ) );
												g_pCtx->m_Interfaces.m_pMenus->AddItemMenu( hPlayerArmorMenu,
													"4", ( UniquePlayer.m_iArmorMode == 3 ? "Selected: 100 Armor" : "100 Armor" ) );

												g_pCtx->m_Interfaces.m_pMenus->SetExitMenu( hPlayerArmorMenu, true );

												g_pCtx->m_Interfaces.m_pMenus->SetCallback
												(
													hPlayerArmorMenu,
													[ itUniquePlayer ]( const char* szBack, const char* szFront, int iItem, int iSlot )
													{
														auto& pUniquePlayer = std::get< ::Ruby::CPlayerData >( *itUniquePlayer );
														const auto iArmorMode = std::atoi( szBack );

														switch ( iArmorMode )
														{
															case 1:
															{
																pUniquePlayer.m_iArmorMode = 0;
																break;
															}
															case 2:
															{
																pUniquePlayer.m_iArmorMode = 1;
																break;
															}
															case 3:
															{
																pUniquePlayer.m_iArmorMode = 2;
																break;
															}
															case 4:
															{
																pUniquePlayer.m_iArmorMode = 3;
																break;
															}
														}

														g_pCtx->m_Interfaces.m_pMenus->ClosePlayerMenu( iSlot );
													}
												);

												g_pCtx->m_Interfaces.m_pMenus->ClosePlayerMenu( iSlotCopy );

												g_pCtx->m_Interfaces.m_pMenus->DisplayPlayerMenu( hPlayerArmorMenu, iSlotCopy, true, true );
												break;
											}
											case hash_64_fnv1a_const( "fire_mode_settings" ):
											{
												Menu hPlayerFireModeMenu;
												g_pCtx->m_Interfaces.m_pMenus->SetTitleMenu( hPlayerFireModeMenu, ( "Fire mode Settings" ) );

												g_pCtx->m_Interfaces.m_pMenus->AddItemMenu( hPlayerFireModeMenu,
													"1", ( UniquePlayer.m_iFireMode == 0 ? "Selected: Disabled" : "Disabled" ) );
												g_pCtx->m_Interfaces.m_pMenus->AddItemMenu( hPlayerFireModeMenu,
													"2", ( UniquePlayer.m_iFireMode == 1 ? "Selected: 1 shot per 5 seconds" : "1 shot per 5 seconds" ) );
												g_pCtx->m_Interfaces.m_pMenus->AddItemMenu( hPlayerFireModeMenu,
													"3", ( UniquePlayer.m_iFireMode == 2 ? "Selected: 1 shot per 10 seconds" : "1 shot per 10 seconds" ) );

												g_pCtx->m_Interfaces.m_pMenus->SetExitMenu( hPlayerFireModeMenu, true );

												g_pCtx->m_Interfaces.m_pMenus->SetCallback
												(
													hPlayerFireModeMenu,
													[ itUniquePlayer ]( const char* szBack, const char* szFront, int iItem, int iSlot )
													{
														auto& pUniquePlayer = std::get< ::Ruby::CPlayerData >( *itUniquePlayer );
														const auto iArmorMode = std::atoi( szBack );

														switch ( iArmorMode )
														{
															case 1:
															{
																pUniquePlayer.m_iFireMode = 0;
																break;
															}
															case 2:
															{
																pUniquePlayer.m_iFireMode = 1;
																break;
															}
															case 3:
															{
																pUniquePlayer.m_iFireMode = 2;
																break;
															}
														}

														g_pCtx->m_Interfaces.m_pMenus->ClosePlayerMenu( iSlot );
													}
												);

												g_pCtx->m_Interfaces.m_pMenus->ClosePlayerMenu( iSlotCopy );

												g_pCtx->m_Interfaces.m_pMenus->DisplayPlayerMenu( hPlayerFireModeMenu, iSlotCopy, true, true );
												break;
											}
											default: break;
										}

									}
								);

								g_pCtx->m_Interfaces.m_pMenus->DisplayPlayerMenu( hPlayerMenu, iSlot, true, true );
							}
							( );
						}
					} 
				);

				g_pCtx->m_Interfaces.m_pMenus->DisplayPlayerMenu( hMenu, iSlot, true, true );

				return false;
			}
		);
	}
}
