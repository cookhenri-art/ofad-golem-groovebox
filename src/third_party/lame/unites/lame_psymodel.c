/* Groovebox : compile libmp3lame/psymodel.c avec la configuration portable (sans HAVE_CONFIG_H,
   qui ferait prendre ce config.h par d'autres bibliotheques, comme le pnglib de JUCE). */
#include "../config/lame_config.h"
#include "../libmp3lame/psymodel.c"
