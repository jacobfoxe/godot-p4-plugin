/*
 * p4dvcsapi.cc - a p4 server helper API example
 *
 * This file is part of the p4api distribution package.
 *
 * This example mimics the 'p4 init' and 'p4 clone' functionallity and then
 * demonstrates the use of the GetClient() helper method to run 'p4 -ztag info'
 *
 * Generally, compiling with the C++ compiler and linking with the
 * three provided libraries is sufficient to build this sample program.
 * 
 * See the Perforce C/C++ API User's Guide for further information.
 * https://www.perforce.com/manuals/p4api/Content/P4API/Home-p4api.html
 *
 * $Id: //depot/r23.2/p4/api/p4dvcsapi.cc#1 $
 */

# include "serverhelperapi.h"
# include "p4libs.h"

int main( int argc, char **argv );
int main( int argc, char **argv )
{
	ClientUser ui;
	StrBuf msg;
	Error e;
	int doClone = 0;

	P4Libraries::Initialize( P4LIBRARIES_INIT_ALL, &e );

	if( e.Test() )
	{
	    e.Fmt( &msg );
	    fprintf( stderr, "%s\n", msg.Text() );
	    return 1;
	}

	const char *p4port = 0;
	const char *cloneTarget = 0;
	
	// argv[1] - init or clone
	// argv[2] - dir
	// argv[3] - remote or filepath ( not for init )
	// argv[4] - p4port

	if( argc < 3 || argc > 5 )
	    return 1;

	if( StrRef( "clone" ) == argv[1] )
	{
	    doClone = 1;
	    if( argc < 4 )
	        return 1;
	    else
	        cloneTarget = argv[3];

	    if( argc == 5 )
	        p4port = argv[4];
	}
	else if( StrRef( "init" ) == argv[1] )
	{
	    if( argc > 4 )
	        return 1;
	    if( argc == 4 )
	        p4port = argv[3];
	}
	else
	    return 1;


	ServerHelperApi personalServer( &e ), centralServer( &e );

	if( e.Test() )
	{
	    e.Fmt( &msg );
	    fprintf( stderr, "%s\n", msg.Text() );
	    return 1;
	}
	
	// Check that we don't already have a DVCS tree at the target directory

	personalServer.SetDvcsDir( argv[2], &e );
	if( e.Test() )
	    return 1;

	if( personalServer.Exists( &ui, &e ) )
	    return 1;

	centralServer.SetPort( p4port, &e );
	if( e.Test() )
	    return 1;

	if( doClone )
	{
	    if( !StrRef( "//" ).SCompareN( StrRef( cloneTarget ) ) )
	    {
	        // Make a remote spec from the remote server
	        if( !personalServer.PrepareToCloneFilepath(
	                    &centralServer, cloneTarget, &ui, &e ) )
	            return 1;
	    }
	    else
	    {
	        // Get the remote spec from the remote server
	        if( !personalServer.PrepareToCloneRemote(
	                    &centralServer, cloneTarget, &ui, &e ) )
	            return 1;
	    }
	}
	else
	{
	    // Discover the case sensitivity and unicode settings
	    if( !personalServer.CopyConfiguration( &centralServer, &ui, &e ) )
	        return 1;
	}
	
	// Create the local server
	// This returns the exit code from p4d, so 0 is success

	if( personalServer.InitLocalServer( &ui, &e ) )
	    return 1;

	if( doClone )
	{
	    // Fetch from the remote
	    char* debug = 0;
	    if( !personalServer.CloneFromRemote( 0, 0, debug, &ui, &e ) )
	        return 1;
	}

	// The init or clone is now complete
	// The following lines get a ClientApi for use with the local server
	// and runs the 'p4 info' command with tagged output.

	// Turn on tagged output for the client
	personalServer.SetProtocolV( P4Tag::v_tag );

	// Get an init'ed ClientApi object
	ClientApi *client = personalServer.GetClient( &e );
	if( !client )
	    return 1;

	// Run 'p4 info'
	client->RunTag( "info", &ui );

	client->Final( &e );
	delete client;

	P4Libraries::Shutdown( P4LIBRARIES_INIT_ALL, &e );

	if( e.Test() )
	{
	    e.Fmt( &msg );
	    fprintf( stderr, "%s\n", msg.Text() );
	    return 1;
	}

	return 0;
}
