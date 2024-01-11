/*
 * Copyright 1995, 2000 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * ClientUser -- default implementation of user interface for p4 client
 *
 * This file implements the p4 client command line interface by defining
 * the methods of the ClientUser class.  This interface reads from stdin,
 * writes to stdout and stderr, and invokes the user's editor.
 */

# define NEED_FILE
# define NEED_STAT

# ifdef OS_NT
# include <process.h>
# define NEED_FCNTL
# ifdef __BORLANDC__
# define ARGVCAST(x)	((char * const *)(x))
# else
# define ARGVCAST(x)	(x)
# endif
# endif

# include <clientapi.h>
# include <clientprog.h>

# include <diff.h>
# include <enviro.h>
# include <echoctl.h>
# include <signaler.h>
# include <strops.h>
# include <runcmd.h>
# include <i18napi.h>
# include <charcvt.h>

# include <msgclient.h>
# include <msgserver.h>

# ifdef OS_MACOSX
# include <CoreFoundation/CoreFoundation.h>
# endif

int commandChaining = 0; // can be set by clientmain to support '-x run'


/*
 * ClientUser::ClientUser() - default constructor
 */

ClientUser::ClientUser( int autoLoginPrompt, int apiVersion )
{
	binaryStdout = 0;
	outputCharset = 0;
	quiet = 0;
	autoLogin = autoLoginPrompt;
	outputTaggedWithErrorLevel = 0;
	transfer = 0;
	ssoHandler = 0;
	apiVer = apiVersion;
	if( apiVersion == -1 )
	    apiVer = atoi( P4Tag::l_client );

# ifdef HAS_CPP11
	setterGuard = new std::mutex;
# else
	setterGuard = 0;
# endif
}

/*
 * ClientUser::~ClientUser() - virtual destructor holder
 */

ClientUser::~ClientUser()
{
	delete transfer;
	delete ssoHandler;
# ifdef HAS_CPP11
	delete (std::mutex *)setterGuard;
# endif
}

/*
 * ClientUser::Prompt() - prompt the user, and wait for a response.
 */

void
ClientUser::Prompt( Error *err, StrBuf &buf, int noEcho, Error *e )
{
	StrBuf ebuf;
	err->Fmt( ebuf, EF_PLAIN );
	Prompt( ebuf, buf, noEcho, e );
}

void
ClientUser::Prompt( Error *err, StrBuf &buf, int noEcho, int noOutput, Error *e )
{
	StrBuf ebuf;
	err->Fmt( ebuf, EF_PLAIN );
	Prompt( ebuf, buf, noEcho, noOutput, e );
}

void
ClientUser::Prompt( const StrPtr &msg, StrBuf &buf, int noEcho, Error *e )
{
	Prompt( msg, buf, noEcho, 0, e );
}

void
ClientUser::Prompt( const StrPtr &msg, StrBuf &buf, int noEcho, int noOutput, Error *e )
{
	if ( !noOutput )
	    printf( "%s", msg.Text() );

	fflush( stdout );
	int flushStdin = 1;
# ifdef OS_NT
	struct _stat statBuf;
	_fstat( _fileno( stdin ), &statBuf );
	if( statBuf.st_mode != _S_IFCHR )
	    flushStdin = 0;
# endif
	if( flushStdin )
	    fflush( stdin );

	// Turn off echoing

	NoEcho *setEcho = noEcho ? new NoEcho : 0;

	// Prompt'em

	buf.Clear();
	char *b = buf.Alloc( 2048 );

	if( !fgets( b, 2048, stdin ) )
	{
	    e->Set( MsgClient::Eof );
	    buf.SetEnd( b );
	}
	else
	{
	    // Set end and truncate any newline

	    buf.SetEnd( b += strlen( b ) );
	    
# ifdef USE_CR
	    /* 
 	     * Codewarrior (7.0?) oddity: 
 	     *	fprintf to file translates \n (10) -> \r.
 	     *	fgets from file translates \n (10) <- \r.
 	     *	fprintf to console translates \n (10) -> newline on screen.
 	     *	fgets from stdin DOES NOT translate "enter".
 	     */
	     
	    if( buf.Length() && buf.End()[ -1 ] == '\r' )
	    	buf.End()[ -1 ] = '\n';
# endif

	    if( buf.Length() && buf.End()[ -1 ] == '\n' )
	    {
		buf.SetEnd( buf.End() - 1 );
		buf.Terminate();
	    }

# ifdef OS_NT
	    if( buf.Length() && buf.End()[ -1 ] == '\r' )
	    {
	        buf.SetEnd( buf.End() - 1 );
	        buf.Terminate();
	    }
# endif
	}

	delete setEcho;
}

void
ClientUser::Help( const char *const *help )
{
	for( ; *help; help++ )
	    printf( "%s\n", *help );
}

void
ClientUser::HandleError( Error *err )
{
	// Dumb implementation is just to format 
	// the error and call (old) OutputError.

	// API callers against 99.1+ servers can do better by
	// having HandleError probe 'err' directly for detail.

	StrBuf buf;
	err->Fmt( buf, EF_NEWLINE );
	OutputError( buf.Text() );
}

void
ClientUser::OutputError( const char *errBuf )
{
	fflush( stdout );
# ifdef OS_VMS
	// Note - VMS likes the args this way,  Mac the other,
	// UNIX doesn't care.
	//
	fwrite( errBuf, strlen( errBuf ), 1, stderr );
# else
	fwrite( errBuf, 1, strlen( errBuf ), stderr );
# endif
}

void
ClientUser::OutputInfo( char level, const char *data )
{
	if( quiet )
	    return;

	switch( level )
	{
	default:
	case '0': break;
	case '1': printf( "... " ); break;
	case '2': printf( "... ... " ); break;
	}

# ifdef OS_VMS
	// Note - VMS likes the args this way,  Mac the other,
	// UNIX doesn't care.
	//
	fwrite( data, strlen( data ), 1, stdout );
# else
	fwrite( data, 1, strlen( data ), stdout );
# endif
	fputc( '\n', stdout );
}

void
ClientUser::OutputText( const char *data, int length )
{

# ifdef OS_VMS
	// Note - VMS likes the args this way,  Mac the other,
	// UNIX doesn't care.
	//
	fwrite( data, length, 1, stdout );
# else
	fwrite( data, 1, length, stdout );
# endif
}

void
ClientUser::OutputBinary( const char *data, int length )
{
# ifdef OS_NT
	// we rely on a trailing zero length buffer to
	// tell us to turn off binary output.

	if( binaryStdout == !length )
	{
	    // toggle
	    binaryStdout = !!length;
	    fflush( stdout );
	    setmode( fileno( stdout ), binaryStdout ? O_BINARY : O_TEXT );
	}
# endif

	fwrite( data, 1, length, stdout );
}

void
ClientUser::InputData( StrBuf *buf, Error * )
{
	if( commandChaining )
	{
	    for( ;; )
	    {
	        StrBuf lb;
	        char *b = lb.Alloc( 2048 );

	        if( !fgets( b, 2048, stdin ) )
	            break;
		int l = strlen( b );
		if( l > 0 && l <= 3 &&
	            b[0] == '.' && ( b[1] == '\r' || b[1] == '\n' ) )
	            break;
		buf->Append( b );
	    }
	    buf->Terminate();
	    return;
	}

	int n;
	int size = FileSys::BufferSize();

	buf->Clear();

	do {
	    char *b = buf->Alloc( size );
	    n = read( 0, b, size );
	    buf->SetEnd( b + ( n >= 0 ? n : 0 ) );
	} while( n > 0 );

	buf->Terminate();
}

void
ClientUser::OutputStat( StrDict *varList )
{
	int i;
	StrBuf msg;
	StrRef var, val;

	// Dump out the variables, using the GetVar( x ) interface.
	// Don't display the function (duh), which is only relevant to rpc.
	// Don't display "specFormatted" which is only relevant to us.

	for( i = 0; varList->GetVar( i, var, val ); i++ )
	{
	    if( var == "func" || var == P4Tag::v_specFormatted ) continue;

	    // otherAction and otherOpen go at level 2, as per 99.1 + earlier

	    msg.Clear();
	    msg << var << " " << val;
	    char level = strncmp( var.Text(), "other", 5 ) ? '1' : '2';
	    OutputInfo( level, msg.Text() );
	}

	// blank line

	OutputInfo( '0', "" );
}

void
ClientUser::ErrorPause( char *errBuf, Error *e )
{
	StrBuf buf;
	OutputError( errBuf );
	Prompt( StrRef( "Hit return to continue..." ), buf, 0, e );

	if( editFile.Length() > 0 )
	{
	    FileSys *f = File( FST_UNICODE );
	    f->Set( editFile );
	    f->Unlink( e );
	    delete f;
	    editFile.Clear();
	}
}

void
ClientUser::HandleUrl( const StrPtr *url )
{
	StrBuf result;
	Error e;

	// Give a textual notification of what we're doing (or what should be
	// done if no browser could be launched).

	e.Set( MsgClient::GotoUrl ) << url;
	HandleError( &e );

	char *env;
	if( ( env = enviro->Get( "P4USEBROWSER" ) ) &&
	    ( !StrPtr::CCompare( "false", env ) ||
	      !StrPtr::CCompare( "no", env ) ) )
	    return;

	int canLaunch = 1;
	RunCommand::RunShell( url, canLaunch, &e );

	// Note: if the browser launched, but in the background, it might still
	// not have been presented to the user and we have no way to detect
	// this case. One such example is if lynx is resolved as the browser:
	// the terminal controls it assumes are present are not, so nothing
	// appears to happen.
}

void
ClientUser::Edit( FileSys *f1, Error *e )
{
	Edit( f1, enviro, e );

	editFile.Set( f1->Name() );
	f1->ClearDeleteOnClose();
}

void
ClientUser::Edit( FileSys *f1, Enviro * env, Error *e )
{
	// The presence of $SHELL on NT implies MKS, which has vi.
	// On VMS, we look for POSIX$SHELL - if set, we'll use vi.
	// Otherwise, EDIT.

	if( !f1->IsTextual() )
	{
	    e->Set( MsgClient::CantEdit ) << f1->Name();
	    return;
	}

	const char *editor = env->Get( "P4EDITOR" );

	if( !editor )
	    editor = env->Get( "EDITOR" );

# ifdef OS_NT
	if( !editor && !env->Get( "SHELL" ) )
	    editor = "notepad";
# endif

# ifdef OS_VMS
	if( !editor && !env->Get( "POSIX$SHELL" ) )
	    editor = "edit";
# endif

# ifdef OS_AS400
  if( !editor )
      editor = "edtf";
# endif

# ifdef OS_MACOSX

	// open flags:  -W wait
	//              -n create a new instance even if one is running
	//              -t use the default text editor
	//              -a use the specified editor

	if( !editor )
	  editor = "open -Wnt";
    
	// It's possible P4EDITOR is set to an actual gui application bundle.
	// Passing bundles to RunCmd won't execute them.
	// Instead, we can prefix it with "open" so the system
	// will open it and bring it forward
    
	FileSys * editorFile = FileSys::Create( FST_BINARY );
	editorFile->Set( editor );
    
	StrBuf openCommand("open -Wna ");
    
	// 1 - the file must exist
	
	if ( editorFile->Stat() & FSF_EXISTS )
	{

	    // 2 - in order to use 'open', the file must also point to a
	    //     bundled application. We check to see if there is a bundle
	    //     with an executable

	    CFURLRef bundleURL = CFURLCreateFromFileSystemRepresentation(NULL,
	                            (const UInt8 *)editor, strlen(editor), true);
	    CFBundleRef bundle = NULL;       

	    if ( bundleURL && (bundle = CFBundleCreate(NULL, bundleURL)) )
	    {
	        CFURLRef executableURL = CFBundleCopyExecutableURL( bundle );
	        if ( executableURL )
	        {
	            CFRelease( executableURL );
	            openCommand.Append( "\"" );
	            openCommand.Append( editor );
	            openCommand.Append( "\"" );

	            editor = openCommand.Text();
	        }
	    }
	    if ( bundle )    CFRelease( bundle );
	    if ( bundleURL ) CFRelease( bundleURL );
	}
  
# endif

	// Bill Joy rules, Emacs maggots!

	if( !editor )
	    editor = "vi";

	RunCmd( editor, f1->Name(), 0, 0, 0, 0, 0, e );
}

void
ClientUser::Diff( FileSys *f1, FileSys *f2, int doPage, char *df, Error *e )
{
	Diff(f1, f2, NULL, doPage, df, e);
}

void
ClientUser::Diff( FileSys *f1, FileSys *f2, FileSys *fout, int doPage, char *df, Error *e )
{
	if( !f1->IsTextual() || !f2->IsTextual() )
	{
	    if( f1->Compare( f2, e ) )
	    {
		StrRef s( "(... files differ ...)\n" );
		if( fout )
		{
		    fout->Open( FOM_WRITE, e );
		    if( !e->Test() )
		    {
			fout->Write( s, e );
			fout->Close( e );
		    }
		}
		else
		{
		    printf( "%s", s.Text() );
		}
	    }
	    return;
	}

	// Call diff to do text compare

	const char *diffunicode = NULL;
	const char *diff = enviro->Get( "P4DIFF" );
	const char *pager = enviro->Get( "P4PAGER" );
	int charset = 0;
	int output  = outputCharset;

	if( !diff )
	    diff = enviro->Get( "DIFF" );

	if( f1->IsUnicode() )
	{
	    diffunicode = enviro->Get( "P4DIFFUNICODE" );
	    charset = f1->GetContentCharSetPriv();
	    if( !output && charset == f2->GetContentCharSetPriv() )
		output = charset;
	}

	if( !doPage )
	    pager = 0;
	else if( !pager )
	    pager = enviro->Get( "PAGER" );

	// Do our own diff
	// VMS ReadFile is busted.

# ifdef OS_VMS

	if( !diff )
	    diff = "diff";

# else /* not VMS */

	if( !diffunicode && !diff )
	{
	    // diff expects to read files in raw mode, we must 
	    // create new FileSys to allow this.

	    FileSys *f1_bin = File( FST_BINARY );
	    FileSys *f2_bin = File( FST_BINARY );
	    int content_charset = f1->GetContentCharSetPriv();
	    int output_translate = 0;

	    if( f1->IsUnicode() &&
		content_charset != output &&
		content_charset != (int)CharSetApi::UTF_8 )
	    {
		// convert files to utf-8

		f1_bin->SetDeleteOnClose();
		f1_bin->MakeGlobalTemp();
		f2_bin->SetDeleteOnClose();
		f2_bin->MakeGlobalTemp();
		CharSetCvt *cvt;
		cvt = CharSetCvt::FindCvt(
		    (CharSetApi::CharSet)content_charset,
		    CharSetApi::UTF_8 );
		// error out if converter not found? - should not happen
		f1->Translator( cvt );
		f1->Copy( f1_bin, FPM_RW, e );
		
		if( !e->Test() )
		{
		    if( cvt ) cvt->ResetErr();
		    f2->Translator( cvt );
		    f2->Copy( f2_bin, FPM_RW, e );
		}
		delete cvt;

		if( output && output != (int)CharSetApi::UTF_8 )
		    output_translate = 1;
	    }
	    else
	    {
		if( f1->IsUnicode() &&
		    output != content_charset )
		    output_translate = 1;
		f1_bin->Set( f1->Name() );
		f2_bin->Set( f2->Name() );
	    }
	    
	    if( !e->Test() )
	    {
		DiffFlags flags( df );
# ifndef OS_NEXT
		::
# endif
		Diff d;
		FileSys *t = NULL;

		d.SetInput( f1_bin, f2_bin, flags, e );
		int fileError = e->Test();

		if( !fileError || flags.type == DiffFlags::Unified )
		{
		    if( fout )
		    {
			t = fout;
			d.SetOutput( t->Name(), e );
		    }
		    else if( pager || output_translate )
		    {
			// this does the work of FileSys::CreateGlobalTemp but
			// uses the client user object's FileSys create method
			t = File( FileSysType( ( f1->GetType() & FST_L_MASK )
					 | FST_UNICODE ) );
			t->SetDeleteOnClose();
			t->MakeGlobalTemp();
			d.SetOutput( t->Name(), e );
		    }
		    else
		    {
			d.SetOutput( stdout );
		    }
		}


		if( fileError && flags.type == DiffFlags::Unified )
	        {

		    d.DiffUnifiedDeleteFile( f1_bin, e );
		    d.CloseOutput( e );

		}
		else if( fileError )
		{
		    d.CloseOutput( e );

		}
		else if( !fileError )
		{

		    if( !e->Test() ) d.DiffWithFlags( flags );
		    d.CloseOutput( e );

		    if( output_translate )
		    {
			// set translator
			CharSetCvt *cvt;
			cvt = CharSetCvt::FindCvt(
			    CharSetApi::UTF_8,
			    (CharSetApi::CharSet)output );
			// error out if converter not found?
			t->Translator( cvt );

			if( pager )
			{
			    FileSys *tr = File( f1->GetType() );
			    tr->SetDeleteOnClose();
			    tr->MakeGlobalTemp();
			    t->Copy( tr, FPM_RW, e );
			    if( !fout )
				delete t;
			    t = tr;
			}
			else if( !fout )
			{
			    // read the file to stdout...

			    t->Open( FOM_READ, e );
			    if( !e->Test() )
			    {
				char buf[2048];
				int i;

				while( (i = t->Read( buf, sizeof(buf), e )) > 0
					&& !e->Test() )
				    fwrite( buf, i, 1, stdout );

   				t->Close( e );
			    }
			}

			delete cvt;
		    }

		    if( pager && !e->Test() )
			RunCmd( pager, t->Name(), 0, 0, 0, 0, 0, e );

		    if( !fout || pager )
			delete t;
		}
	    }

	    delete f1_bin;
	    delete f2_bin;
	    return;
	}

# endif /* VMS */

	// Build up flags args
	// Yuk.

	if( !df || !*df )
	{
	    if( diffunicode )
		RunCmd( diffunicode,
			CharSetApi::Name( (CharSetApi::CharSet)charset ),
			f1->Name(), f2->Name(), 0, 0, pager, e );
	    else
		RunCmd( diff, f1->Name(), f2->Name(), 0, 0, 0, pager, e );
	}
	else
	{
	    StrBuf flags;
	    flags.Set( "-", 1 );
	    flags << df;
	    if( diffunicode )
		RunCmd( diffunicode, flags.Text(),
			CharSetApi::Name( (CharSetApi::CharSet)charset ),
			f1->Name(), f2->Name(), 0, pager, e );
	    else
	        RunCmd( diff, flags.Text(), f1->Name(), f2->Name(),
			0, 0, pager, e );
	}
}

void
ClientUser::Merge( 
	FileSys *base,
	FileSys *leg1,
	FileSys *leg2,
	FileSys *result,
	Error *e )
{
	char *merger;

	if( result->IsUnicode() )
	{
	    int cs = result->GetContentCharSetPriv();

	    if( cs != 0 && ( merger = enviro->Get( "P4MERGEUNICODE" ) ) )
	    {
		RunCmd( merger, CharSetApi::Name( (CharSetApi::CharSet)cs ),
			base->Name(), leg1->Name(), leg2->Name(),
			result->Name(), 0, e );
		return;
	    }
	}

	merger = enviro->Get( "P4MERGE" );
	
	if( !merger )
	    merger = enviro->Get( "MERGE" );

	if( !merger )
	{
	    e->Set( MsgClient::NoMerger );
	    return;
	}

	RunCmd( merger, base->Name(), leg1->Name(), 
		leg2->Name(), result->Name(), 0, 0, e );
}

void
ClientUser::RunCmd(
	const char *command,
	const char *arg1,
	const char *arg2,
	const char *arg3,
	const char *arg4,
	const char *arg5,
	const char *pager,
	Error *e )
{
	// XXX RunCommand is dynamically allocated
	// to work around linux 2.5 24x86 compiler bug.
	// see job019081.

	RunCommand *rc = new RunCommand;

	fflush( stdout );

	signaler.Block();	// reset SIGINT to SIGDFL

	// Use AddCmd() to handle command which may be a mix of
	// cmd name (with spaces on NT) and flags (following spaces).

	RunArgs cmd;

	cmd.AddCmd( command );
	if( arg1 ) cmd.AddArg( arg1 );
	if( arg2 ) cmd.AddArg( arg2 );
	if( arg3 ) cmd.AddArg( arg3 );
	if( arg4 ) cmd.AddArg( arg4 );
	if( arg5 ) cmd.AddArg( arg5 );
	if( pager )
	{
	    cmd.AddArg( "|" );
	    cmd.AddArg( pager );
	}

	rc->Run( cmd, e );
	delete rc;

	signaler.Catch();	// catch SIGINT again
}

FileSys *
ClientUser::File( FileSysType type )
{
	return FileSys::Create( type );
}

int
ClientUser::ProgressIndicator()
{
	return 0;
}

ClientProgress *
ClientUser::CreateProgress( int t, P4INT64 s )
{
	if( s >= 1024 )
	    return CreateProgress( t );
	return 0;
}

ClientProgress *
ClientUser::CreateProgress( int )
{
	return NULL;
}

int
ClientUserProgress::ProgressIndicator()
{
	return 1;
}

ClientProgress *
ClientUserProgress::CreateProgress( int t )
{
	if( !ClientProgressText::InUse )
	    return new ClientProgressText( t );
	return 0;
}

int
ClientUser::Resolve( ClientMerge *m, Error *e )
{
	// pun from ClientMergeStatus
	return (int)m->Resolve( e );
}

int
ClientUser::Resolve( ClientResolveA *r, int preview, Error *e )
{
	// pun from ClientMergeStatus
	return (int)r->Resolve( preview, e );
}

void
ClientUser::Message( Error *err )
{
	int keepfile = 0;

	if( err->IsInfo() )
	{
	    // Info
	    StrBuf buf;
	    err->Fmt( buf, EF_PLAIN );
	    OutputInfo( (char)err->GetGeneric() + '0', buf.Text() );

	    if( err->CheckId( MsgServer::SpecNotCorrect ) )
		keepfile = 1;
	}
	else
	{
	    // warn, failed, fatal
	    HandleError( err );
	    
	    // report file name left
	    if( !err->CheckId( MsgServer::ErrorInSpec ) )
		keepfile = 1;
	}

	if( editFile.Length() > 0 )
	{
	    if( keepfile )
	    {
		Error other;
		other.Set( MsgClient::FileKept ) << editFile.Text();
		HandleError( &other );
	    }
	    else
	    {
		FileSys *f = File( FST_UNICODE );
		f->Set( editFile );
		f->Unlink( err );
		delete f;
	    }
	    editFile.Clear();
	}
}

void
ClientUser::SetOutputCharset( int charset )
{
	outputCharset = charset;
}

void
ClientUser::DisableTmpCleanup()
{
	signaler.Disable();
}

void
ClientUser::SetTransfer( ClientTransfer* t )
{
	transfer = t;
}

void
ClientUser::SetSSOHandler( ClientSSO* s )
{
	ssoHandler = s;
}

void
ClientUser::SetQuiet()
{
	quiet = 1;
}

int
ClientUser::CanAutoLoginPrompt()
{
	// Only allow auto login prompting if:
	
	return autoLogin               &&     // it's enabled in the clientuser
	    !quiet                     &&     // clientuser isn't in quiet mode
	    isatty( fileno( stdin ) )  &&     // all STDIO pipes are tty's
	    isatty( fileno( stdout ) ) &&
	    isatty( fileno( stderr ) ) ? 1 : 0;
}

int
ClientUser::IsOutputTaggedWithErrorLevel()
{
	return outputTaggedWithErrorLevel;
}

void
ClientUser::SetEnviro( Enviro *env )
{
# ifdef HAS_CPP11
	std::lock_guard<std::mutex> lock( *(std::mutex *)setterGuard );
# endif
	enviro = env;
}

void
ClientUser::SetVarList( StrDict *l )
{
# ifdef HAS_CPP11
	std::lock_guard<std::mutex> lock( *(std::mutex *)setterGuard );
# endif
	varList = l;
}
