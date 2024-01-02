#include "p4_plugin.h"
#include <cstring>
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/variant/utility_functions.hpp"

//#include <openssl/md5.h>

const godot::String VCS_NAME = "Perforce";


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



/*****************************************************************************
 * 
 *  Inputs the connection variables. 
 * 
 * ***************************************************************************/
void P4Plugin::_setup_connection(const godot::String &username, const godot::String &password, const godot::String &host_server, const godot::String &portnum)
{
    /* Save credentials */
	creds.username = username;
    creds.password = password;
    creds.path = host_server;
    creds.port = portnum;
}

/*****************************************************************************
 * 
 *  Updates P4CONFIG from the input credentials. 
 * 
 * ***************************************************************************/
bool P4Plugin::_update_p4config()
{
    /* Update P4PORT */
    p4_client.DefinePort(creds.port.utf8(), &p4Err);
    if( p4Err.Test() )
    { 
        p4Err.Fmt( &msg );
        fprintf( stderr, "%s\n", msg.Text() );
        return 1;
    }

    /* Update P4USER */
    p4_client.DefineUser(creds.username.utf8(), &p4Err);
    if( p4Err.Test() )
    { 
        p4Err.Fmt( &msg );
        fprintf( stderr, "%s\n", msg.Text() );
        return 1;
    }

    /* Update P4HOST */
    p4_client.DefineHost(creds.path.utf8(), &p4Err);
    if( p4Err.Test() )
    { 
        p4Err.Fmt( &msg );
        fprintf( stderr, "%s\n", msg.Text() );
        return 1;
    }

    /* Update P4PASSWD */
    // TODO: store as md5
    p4_client.DefinePassword(creds.password.utf8(), &p4Err);
    if( p4Err.Test() )
    { 
        p4Err.Fmt( &msg );
        fprintf( stderr, "%s\n", msg.Text() );
        return 1;
    }
}

/*****************************************************************************
 * 
 *  Starts the client. This is called after _setup_connection in the current
 *  setup. 
 * 
 * ***************************************************************************/
bool P4Plugin::_start_client()
{
    p4_client.Init( &p4Err );

	if( p4Err.Test() )
    { 
        p4Err.Fmt( &msg );
        fprintf( stderr, "%s\n", msg.Text() );
        return 1;
    }
}

/*****************************************************************************
 * 
 *  Create the .p4ignore file in the repo (if nonexistent).  
 * 
 * ***************************************************************************/
void P4Plugin::create_p4ignore() {
	if (!godot::FileAccess::file_exists(repo_project_path + "/.p4ignore")) {
		godot::Ref<godot::FileAccess> file = godot::FileAccess::open(repo_project_path + "/.p4ignore", godot::FileAccess::ModeFlags::WRITE);
		ERR_FAIL_COND(file.is_null());
		file->store_string(
				"## Ignore Godot Files ##\n"
				".godot/\n"
                "\n"
                "## Ignore Imports ##\n"
                "*.import\n"
                "*.import/*\n"
                "\n"
                "## Ignore Configuration Files ##\n"
                "*.cfg\n"
                "*.cfg*\n");
        
        /* Set the ignore file */
        char* fullIgnorePath = CString(repo_project_path).data;
        int len = strlen(fullIgnorePath);
        char* ignore = "/.p4ignore";
        fullIgnorePath[len+1] = *ignore;
        p4_client.SetIgnoreFile(fullIgnorePath);
	}
}

/*****************************************************************************
 * 
 *  Returns the VCS name.   
 * 
 * ***************************************************************************/
godot::String P4Plugin::_get_vcs_name() {
	return VCS_NAME;
}

/*****************************************************************************
 * 
 *  Initialize the plugin to enable other functionality. 
 * 
 * ***************************************************************************/
bool P4Plugin::_initialize(const godot::String &project_path) {
	using namespace godot;
	StrBuf msg;

	// Initialize P4
	P4Libraries::Initialize( P4LIBRARIES_INIT_ALL, &p4Err );

	if( p4Err.Test() )
    { 
        p4Err.Fmt( &msg );
        fprintf( stderr, "%s\n", msg.Text() );
        return 1;
    }
}