/*
 * See LICENSE file for copyright and license details.
 */
#include "server.h"

int
main(int argc, char *argv[])
{
	char *startup_cmd = NULL;
	char *color_config_file = NULL;
	int c;

	while ((c = getopt(argc, argv, "s:c:hdv")) != -1) {
		if (c == 's')
			startup_cmd = optarg;
		else if (c == 'c')
			color_config_file = optarg;
		else if (c == 'd')
			log_level = WLR_DEBUG;
		else if (c == 'v')
			die("dwl " VERSION);
		else
			goto usage;
	}
	if (optind < argc)
		goto usage;

	/* Parse and load dynamic color palette configuration */
	load_color_config(color_config_file);

	/* Wayland requires XDG_RUNTIME_DIR for creating its communications socket */
	if (!getenv("XDG_RUNTIME_DIR"))
		die("XDG_RUNTIME_DIR must be set");

	setup();
	run(startup_cmd);
	cleanup();
	return EXIT_SUCCESS;

usage:
	die("Usage: %s [-v] [-d] [-c color config] [-s startup command]", argv[0]);
}
