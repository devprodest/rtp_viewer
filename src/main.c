#include <stdlib.h>

#include "app/app.h"
#include "app/config.h"

int main(int argc, char ** argv)
{
    app_config_s cfg;
    bool         exit_ok = false;

    if (!app_config_parse(argc, argv, &cfg, &exit_ok))
    {
        return exit_ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    return app_run(&cfg);
}