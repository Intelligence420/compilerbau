#include "globals.h"

struct globals global;


/*
 * Initialize global variables from globals.mac
 */

void GLBinitializeGlobals()
{
    global.col = 0;
    global.line = 0;
    global.input_file = NULL;
    global.output_file = NULL;
    global.skip_optimization = false;
    global.skip_codegen = false;
    global.print_debug = false;
}
