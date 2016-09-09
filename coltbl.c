#include <math.h>

#include "coltbl.h"

/* Generate a U16 colortable of size entries in the given array.
 * Peak (in [0, 1]) gives the fraction of output intensity at the highest stimulation.
 * Gamma is the standard gamma correction factor (as a reciprocal--e.g., sRGB uses 2.4)
 */
void colortable(unsigned short *chan, size_t size, double peak, double gamma) {
	size_t i;

	for(i = 0; i < size; i++) {
		chan[i] = (unsigned short) (0xFFFF * pow(i * peak / size, 1 / gamma));
	}
}
