#include <stdlib.h>

#include "app/app.h"
#include "app/config.h"
#include "log/logger.h"

int main(int argc, char ** argv)
{
    app_config_s cfg;
    bool exit_ok = false;

    logger_init(nullptr, LOG_INFO);

    if (!app_config_parse(argc, argv, &cfg, &exit_ok))
    {
        logger_cleanup();
        return exit_ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (cfg.logfile)
    {
        logger_cleanup();
        if (!logger_init(cfg.logfile, LOG_INFO))
        {
            return EXIT_FAILURE;
        }
    }

    int rc = app_run(&cfg);

    logger_cleanup();
    return rc;
}
