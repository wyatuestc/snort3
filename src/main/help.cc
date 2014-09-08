/*
** Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
** Copyright (C) 2013-2013 Sourcefire, Inc.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License Version 2 as
** published by the Free Software Foundation.  You may not use, modify or
** distribute this program under any other version of the GNU General
** Public License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "help.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <syslog.h>
#include <iostream>
#include <string>
using namespace std;

#include "config_file.h"
#include "helpers/process.h"
#include "main/snort.h"
#include "main/snort_module.h"
#include "managers/event_manager.h"
#include "managers/so_manager.h"
#include "managers/inspector_manager.h"
#include "managers/module_manager.h"
#include "managers/plugin_manager.h"
#include "packet_io/sfdaq.h"
#include "packet_io/intf.h"
#include "utils/util.h"
#include "helpers/markup.h"
#include "framework/module.h"
#include "framework/parameter.h"

static const char* snort_help =
"\n"
"Snort has several options to get more help:\n"
"\n"
"--help list command line options\n"
"--help! this overview of help\n"
"--help-builtin [<module prefix>] output matching builtin rules\n"
"--help-buffers output available inspection buffers\n"
"--help-commands [<module prefix>] output matching commands\n"
"--help-config [<module prefix>] output matching config options\n"
"--help-gids [<module prefix>] output matching generators\n"
"--help-module <module> output description of given module\n"
"--help-options [<option prefix>] output matching command line options\n"
"--help-signals dump available control signals\n"
"--list-modules list all known modules\n"
"--list-plugins list all known modules\n"
"--markup output help in asciidoc compatible format\n"
"\n"
"--help* and --list* options preempt other processing so should be last on the\n"
"command line since any following options are ignored.  To ensure options like\n"
"--markup and --plugin-path take effect, place them ahead of the help or list\n"
"options.\n"
"\n"
"Options that filter output based on a matching prefix, such as --help-config\n"
"won't output anything if there is no match.  If no prefix is given, everything\n"
"matches.\n"
"\n"
"Parameters are given with this format:\n"
"\n"
"    type name = default: help { range }\n"
"\n"
"+ For Lua configuration (not IPS rules), if the name ends with [] it is a\n"
"  list item and can be repeated.\n"
"+ For IPS rules only, names starting with ~ indicate positional parameters.\n"
"  The name does not appear in the rule.\n"
"+ IPS rules may also have a wild card parameter, which is indicated by a *.\n"
"  Only used for metadata that Snort ignores.\n"
"+ The snort module has command line options starting with a -.\n"
;

//-------------------------------------------------------------------------

void help_args(const char* pfx)
{
    Module* m = get_snort_module();
    const Parameter* p = m->get_parameters();
    unsigned n = pfx ? strlen(pfx) : 0;

    while ( p->name )
    {
        if ( p->help && (!n || !strncasecmp(p->name, pfx, n)) )
        {
            cout << Markup::item();
            cout << Markup::emphasis_on();

            //const char* prefix = strlen(p->name) > 1 ? "--" : "-";
            //cout << prefix << p->name;
            cout << p->name;
            cout << Markup::emphasis_off();

            cout << " " << p->help;
            cout << endl;
        }
        ++p;
    }
}

void help_basic(SnortConfig*, const char*)
{
    fprintf(stdout, "%s\n", snort_help);
    exit(0);
}

void help_usage(SnortConfig*, const char* val)
{
    fprintf(stdout, "USAGE: %s [-options]\n", "snort");
    help_args(val);
    exit(1);
}

void help_options(SnortConfig*, const char* val)
{
    help_args(val);
    exit(0);
}

void help_signals(SnortConfig*, const char*)
{
    help_signals();
    exit(0);
}

enum HelpType {
    HT_CFG, HT_CMD, HT_GID, HT_IPS, HT_MOD,
    HT_BUF, HT_LST, HT_PLG, HT_DDR, HT_DBR
};

static void show_help(SnortConfig* sc, const char* val, HelpType ht)
{
    snort_conf = new SnortConfig;
    PluginManager::load_plugins(sc->plugin_path);
    ModuleManager::init();

    switch ( ht )
    {
    case HT_CFG:
        ModuleManager::show_configs(val);
        break;
    case HT_CMD:
        ModuleManager::show_commands(val);
        break;
    case HT_GID:
        ModuleManager::show_gids(val);
        break;
    case HT_IPS:
        ModuleManager::show_rules(val);
        break;
    case HT_MOD:
        ModuleManager::show_module(val);
        break;
    case HT_BUF:
        InspectorManager::dump_buffers();
        break;
    case HT_LST:
        ModuleManager::list_modules();
        break;
    case HT_PLG:
        PluginManager::list_plugins();
        break;
    case HT_DDR:
        SoManager::dump_rule_stubs(val);
        break;
    case HT_DBR:
        ModuleManager::dump_rules(val);
        break;
    }
    ModuleManager::term();
    PluginManager::release_plugins();
    delete snort_conf;
    exit(0);
}

void help_config(SnortConfig* sc, const char* val)
{
    show_help(sc, val, HT_CFG);
}

void help_commands(SnortConfig* sc, const char* val)
{
    show_help(sc, val, HT_CMD);
}

void config_markup(SnortConfig*, const char*)
{
    Markup::enable();
}

void help_gids(SnortConfig* sc, const char* val)
{
    show_help(sc, val, HT_GID);
}

void help_buffers(SnortConfig* sc, const char* val)
{
    show_help(sc, val, HT_BUF);
}

void help_builtin(SnortConfig* sc, const char* val)
{
    show_help(sc, val, HT_IPS);
}

void help_module(SnortConfig* sc, const char* val)
{
    show_help(sc, val, HT_MOD);
}

void list_modules(SnortConfig* sc, const char* val)
{
    show_help(sc, val, HT_LST);
}

void list_plugins(SnortConfig* sc, const char* val)
{
    show_help(sc, val, HT_PLG);
}

void dump_builtin_rules(SnortConfig* sc, const char* val)
{
    show_help(sc, val, HT_DBR);
}

void dump_dynamic_rules(SnortConfig* sc, const char* val)
{
    show_help(sc, val, HT_DDR);
}

void help_version(SnortConfig*, const char*)
{
    DisplayBanner();
    exit(0);
}

void list_interfaces(SnortConfig*, const char*)
{
    DisplayBanner();
    PrintAllInterfaces();
    exit(0);
}

void list_daqs(SnortConfig* sc, const char* val)
{
    if ( val )
        ConfigDaqDir(sc, val);

    DAQ_Load(sc);
    DAQ_PrintTypes(stdout);
    DAQ_Unload();
    exit(0);
}
