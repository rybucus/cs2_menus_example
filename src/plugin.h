#ifndef _RUBY_ANTIAFK_PLUGIN_H_
#define _RUBY_ANTIAFK_PLUGIN_H_

#include <igameevents.h>
#include <sh_vector.h>

#include "version_gen.h"
#include "ctx.h"

class MenusExample : public ISmmPlugin, public IMetamodListener
{
public:

	bool Load( PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late );
	bool Unload( char* error, size_t maxlen );
	void AllPluginsLoaded( );

private:

	void OnLevelInit( char const* pMapName, char const *pMapEntities, char const *pOldLevel, char const *pLandmarkName, bool loadGame, bool background );
	void OnLevelShutdown( );

	void Hook_GameFrame( bool bSimulating, bool bFirstTick, bool bLastTick );
	bool Hook_ClientConnect( CPlayerSlot Slot, const char* szName, uint64 llUid, const char* szNetworkID, bool bUnk, CBufferString* pRejectReason );
	void Hook_ClientDisconnect( CPlayerSlot Slot, ENetworkDisconnectionReason eReason, const char* szName, uint64 llUid, const char* szNetworkID );
	void Hook_ClientPutInServer( CPlayerSlot iSlot, char const* szName, int iType, uint64 llUid );
	bool Hook_FireEvent( IGameEvent* pEvent, bool bDontBroadcast );

public:

	const char* GetAuthor( ) { return PLUGIN_AUTHOR; }
	const char* GetName( ) { return PLUGIN_DISPLAY_NAME; }
	const char* GetDescription( ) { return PLUGIN_DESCRIPTION; }
	const char* GetURL( ) { return PLUGIN_URL; }
	const char* GetLicense( ) { return PLUGIN_LICENSE; }
	const char* GetVersion( ) { return PLUGIN_FULL_VERSION; }
	const char* GetDate( ) { return __DATE__; }
	const char* GetLogTag( ) { return PLUGIN_LOGTAG; }

};

extern MenusExample g_Plugin;

#endif //_RUBY_ANTIAFK_PLUGIN_H_
