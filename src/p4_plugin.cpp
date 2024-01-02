#include "p4_plugin.h"

const VCS_NAME = "Perforce"

void P4Plugin::create_p4ignore()
{

}

bool P4Plugin::_shut_down()
{
	// Close connection

    p4_client.Final( &p4Err );
    if( p4Err.Test() )  
    {
        p4Err.Fmt( &msg );
        fprintf( stderr, "%s\n", msg.Text() );
        return 1;
    }

    P4Libraries::Shutdown( P4LIBRARIES_INIT_ALL, &p4Err );

    if( p4Err.Test() )
    {
        p4Err.Fmt( &msg );
        fprintf( stderr, "%s\n", msg.Text() );
        return 1;
    }
}



/*******************************************
 * 
 *  Inputs the connection variables. 
 * 
 * *****************************************/
void P4Plugin::_setup_connection(const godot::String &username, const godot::String &password, const godot::String &host_server, const godot::String &portnum)
{
	creds.username = username;
    creds.password = password;
    creds.path = host_server;
    creds.port = portnum;
}

void P4Plugin::_start_client()
{
    p4_client.Init( &p4Err );

	if( p4Err.Test() )
    { 
        p4Err.Fmt( &msg );
        fprintf( stderr, "%s\n", msg.Text() );
        return 1;
    }
}


godot::String P4Plugin::_get_vcs_name() {
	return VCS_NAME;
}

bool P4Plugin::_initialize(const godot::String &project_path) {
	using namespace godot;
	StrBuf msg;
	// https://www.perforce.com/manuals/p4api/Content/P4API/sample-application.html

	// Initialize P4
	P4Libraries::Initialize( P4LIBRARIES_INIT_ALL, &p4Err );

	if( p4Err.Test() )
    { 
        p4Err.Fmt( &msg );
        fprintf( stderr, "%s\n", msg.Text() );
        return 1;
    }
}