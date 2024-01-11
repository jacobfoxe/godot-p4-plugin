#pragma once
#include "clientapi.h"
#include "p4libs.h"
#include "p4_wrappers.h"

#include "godot_cpp/classes/editor_vcs_interface.hpp"

struct Credentials { 
    godot::String username;
    godot::String password;
    godot::String path;
    godot::String port;
};

class P4Plugin : public godot::EditorVCSInterface {
    GDCLASS(P4Plugin, godot::EditorVCSInterface);

    public:
        Credentials creds;
        ClientUser p4_user;
        ClientApi p4_client;
        Error p4Err;
        StrBuf msg;

        godot::String repo_project_path;

        P4Plugin();
        
        bool _initialize(const godot::String &project_path) override;
        bool _shut_down() override;
        void _setup_connection(const godot::String &username, const godot::String &password, const godot::String &host_server, const godot::String &portnum);
        bool _update_p4config();
        bool _start_client();


        void create_p4ignore();
        godot::String _get_vcs_name() override;

        /* P4 Funcs */
        void _sync_depot();
        godot::TypedArray<godot::Dictionary> _parse_diff(void);
        godot::TypedArray<godot::Dictionary> _get_line_diff(const godot::String &file_path, const godot::String &text) override;

    protected:
	    static void _bind_methods();
};