#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "common.h"

static const char* g_version_text =
#ifdef _WIN32
    "psxe.exe (" STR(OS_INFO) ") " STR(REP_VERSION) "-" STR(REP_COMMIT_HASH) "\n"
#elif __linux__
    "psxe (" STR(OS_INFO) ") " STR(REP_VERSION) "-" STR(REP_COMMIT_HASH) "\n"
#else
    "psxe (" STR(OS_INFO) ") " STR(REP_VERSION) "-" STR(REP_COMMIT_HASH) "\n"
#endif
    "\nPSXE - A simple, fast and portable Sony PlayStation emulator.\n\n"
    "Copyright (C) 2023 Allkern (Lisandro Alarcon)\n"
    "This is free software; see the source for copying conditions.  There is NO\n"
    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";

static const char g_default_settings[] =
    "# Settings file generated by PSXE\n"
    "\n"
    "# Don't change please, reserved for future use\n"
    "psxe_version = \"" STR(REP_VERSION) "\"\n"
    "\n"
    "# BIOS-related settings:\n"
    "[bios]\n"
    "    search_path     = \"bios\"\n"
    "    preferred_model = \"SCPH-1001\"\n"
    "    override_file   = \"\"\n"
    "\n"
    "# Console settings\n"
    "[console]\n"
    "    region          = \"auto\"\n";


static const char* g_models_text =
    "Available console models:\n"
    "\"scph1000\" (SCPH-1000) [NTSC-J]\n"
    "\"scph1001\" (SCPH-1001) [NTSC-U/C]\n"
    "\"scph1002\" (SCPH-1002) [PAL]\n"
    "\"scph3000\" (SCPH-3000) [NTSC-J]\n"
    "\"scph3500\" (SCPH-3500) [NTSC-J]\n"
    "\"scph5000\" (SCPH-5000) [NTSC-U/C]\n"
    "\"scph5501\" (SCPH-5501) [NTSC-U/C]\n"
    "\"scph5500\" (SCPH-5500) [NTSC-J]\n"
    "\"scph5502\" (SCPH-5502) [PAL]\n"
    "\"scph5552\" (SCPH-5552) [PAL]\n"
    "\"scph7000\" (SCPH-7000) [NTSC-J]\n"
    "\"scph7001\" (SCPH-7001) [NTSC-U/C]\n"
    "\"scph7002\" (SCPH-7002) [PAL]\n"
    "\"scph7003\" (SCPH-7003) [NTSC-J]\n"
    "\"scph7501\" (SCPH-7501) [NTSC]\n"
    "\"scph7502\" (SCPH-7502) [PAL]\n"
    "\"scph9002\" (SCPH-9002) [PAL]\n"
    "\"scph100\" (SCPH-100) [NTSC-J]\n"
    "\"scph101\" (SCPH-101) [NTSC-U/C]\n"
    "\"scph102a\" (SCPH-102A) [PAL]\n"
    "\"scph102b\" (SCPH-102B) [PAL]\n"
    "\"scph102c\" (SCPH-102C) [?]\n";

static const char* g_regions_text =
    "Available region options: \"ntsc\", \"pal\", \"auto\"\n";

static const char* g_desc_text =
    "\nPlease report any bugs to <https://github.com/allkern/psxe/issues>\n";


psxe_config_t* psxe_cfg_create() {
    return (psxe_config_t*)malloc(sizeof(psxe_config_t));
}

void psxe_cfg_init(psxe_config_t* cfg) {
    memset(cfg, 0, sizeof(psxe_config_t));
}

void psxe_cfg_load_defaults(psxe_config_t* cfg) {
    cfg->bios = NULL;
    cfg->bios_search = "bios";
    cfg->exe = NULL;
    cfg->help_model = 0;
    cfg->help_region = 0;
    cfg->model = "scph1001";
    cfg->psxe_version = STR(REP_VERSION);
    cfg->region = "ntsc";
    cfg->settings_path = NULL;
    cfg->use_args = 0;
    cfg->version = 0;
    cfg->log_level = LOG_FATAL;
    cfg->quiet = 0;
    cfg->cd_path = NULL;
}

void psxe_cfg_load(psxe_config_t* cfg, int argc, const char* argv[]) {
    log_set_level(LOG_INFO);

    int use_args = 0;
    int version = 0;
    int help_model = 0;
    int help_region = 0;
    int log_level = 0;
    int quiet = 0;
    const char* settings_path = NULL;
    const char* bios = NULL;
    const char* bios_search = NULL;
    const char* model = NULL;
    const char* exe = NULL;
    const char* region = NULL;
    const char* psxe_version = NULL;
    const char* cd_path = NULL;

    static const char *const usages[] = {
        "psxe [options]",
        NULL,
    };

    struct argparse_option options[] = {
        OPT_BOOLEAN ('h', "help"         , NULL          , "Display this information", argparse_help_cb, 0, 0),
        OPT_BOOLEAN (0  , "help-model"   , &help_model   , "Display available console models", NULL, 0, 0),
        OPT_BOOLEAN (0  , "help-region"  , &help_region  , "Display available region options", NULL, 0, 0),
        OPT_BOOLEAN ('v', "version"      , &version      , "Display version and build information", NULL, 0, 0),
        OPT_GROUP("Basic options"),
        OPT_BOOLEAN ('a', "use-args"     , &use_args     , "Ignore settings file, use CLI args instead", NULL, 0, 0),
        OPT_STRING  ('S', "settings-file", &settings_path, "Specify settings file path", NULL, 0, 0),
        OPT_STRING  ('b', "bios"         , &bios         , "Use this BIOS file (ignores -B, -M)", NULL, 0, 0),
        OPT_BOOLEAN ('B', "bios-folder"  , &bios_search  , "Specify a BIOS search folder", NULL, 0, 0),
        OPT_STRING  ('M', "model"        , &model        , "Specify console model (SPCH-XXXX)", NULL, 0, 0),
        OPT_STRING  ('r', "region"       , &region       , "Specify console region"),
        OPT_STRING  ('x', "exe"          , &exe          , "Boot this PS-X EXE file (ignores CDROM image)"),
        OPT_INTEGER ('L', "log-level"    , &log_level    , "Set log level"),
        OPT_BOOLEAN ('q', "quiet"        , &quiet        , "Silence all logs (ignores -L)"),
        OPT_STRING  (0  , "cdrom"        , &cd_path      , "Specify a CDROM image"),
        OPT_END()
    };

    struct argparse argparse;

    argparse_init(&argparse, options, usages, 0);
    argparse_describe(&argparse, NULL, g_desc_text);

    argc = argparse_parse(&argparse, argc, argv);

    if (help_model) {
        printf("%s\n", g_models_text);

        exit(0);
    }

    if (help_region) {
        printf("%s\n", g_regions_text);

        exit(0);
    }

    if (version) {
        printf("%s\n", g_version_text);

        exit(0);
    }

    log_set_quiet(quiet);

    if (!use_args) {
        if (!settings_path)
            settings_path = "settings.toml";

        FILE* settings = fopen(settings_path, "rb");

        char error[0x100];

        if (!settings) {
            settings = fopen("settings.toml", "w+b");

            if (!settings) {
                log_error("Couldn't create settings file, loading default settings");

                psxe_cfg_load_defaults(cfg);

                return;
            }

            fwrite(g_default_settings, 1, sizeof(g_default_settings) - 1, settings);

            fseek(settings, 0, 0);
        }

        log_info("Parsing settings file...");

        toml_table_t* conf = toml_parse_file(settings, error, sizeof(error));

        if (!conf) {
            log_error("Couldn't parse settings file");

            exit(1);
        }

        toml_datum_t s_version = toml_string_in(conf, "psxe_version");

        if (!s_version.ok) {
            log_error("Settings file lacking version number");

            exit(1);
        }

        toml_table_t* s_bios_table = toml_table_in(conf, "bios");

        if (s_bios_table) {
            toml_datum_t s_bios_search_path = toml_string_in(s_bios_table, "search_path");

            if (s_bios_search_path.ok)
                bios_search = s_bios_search_path.u.s;

            toml_datum_t s_bios_preferred_model = toml_string_in(s_bios_table, "preferred_model");

            if (s_bios_preferred_model.ok)
                model = s_bios_preferred_model.u.s;

            toml_datum_t s_bios_override_file = toml_string_in(s_bios_table, "override_file");

            if (s_bios_override_file.ok)
                bios = s_bios_override_file.u.s;
        }

        toml_table_t* s_console_table = toml_table_in(conf, "console");

        if (s_console_table) {
            toml_datum_t s_console_region = toml_string_in(s_console_table, "region");

            if (s_console_region.ok)
                region = s_console_region.u.s;
        }

        psxe_version = s_version.u.s;

        log_info("Settings file parsed. PSXE version: %s", psxe_version);

        fclose(settings);
    }

    if (argc) {
        if (argc > 1) {
            log_error("Unrecognized parameter \'%s\'", argv[1]);

            exit(1);
        }

        cd_path = argv[0];
    }

    if (cd_path)
        cfg->cd_path = cd_path;

    if (log_level)
        cfg->log_level = log_level - 1;

    if (bios)
        cfg->bios = bios;

    if (bios_search)
        cfg->bios_search = bios_search;

    if (model)
        cfg->model = model;

    if (exe)
        cfg->exe = exe;

    if (region)
        cfg->region = region;

    if (psxe_version)
        cfg->psxe_version = psxe_version;
}

char* psxe_cfg_get_bios_path(psxe_config_t* cfg) {
    if (cfg->bios) {
        return cfg->bios;
    }

    if (!cfg->bios_search)
        return NULL;
}

#undef STR1
#undef STR